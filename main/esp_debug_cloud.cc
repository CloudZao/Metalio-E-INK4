#include "esp_debug_cloud.h"

#if !ESP_DEBUG_CLOUD_ENABLE

void EspDebugCloud_Start(void) {}

#else

/**
 * 崩溃修复要点（Interrupt WDT / 栈损坏）：
 * 1. 日志钩子禁止 malloc / FreeRTOS Queue（易与 log/heap 锁死锁，且大结构体上栈爆栈）
 * 2. 钩子只做：栈上短缓冲 vsnprintf + 字节环形缓冲写入（极短 critical）
 * 3. ISR 内直接旁路，只走原 vprintf
 * 4. 发送任务用 inet_pton 解析 IPv4，避免小栈 getaddrinfo
 */

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

namespace {

constexpr char kTag[] = "EspDbgCloud";
constexpr char kMagic[4] = {'E', 'D', 'C', '1'};

/** 钩子内格式化上限：只上栈，超长截断 */
constexpr size_t kFormatCap = 256;
/** 字节环：静态分配，约缓冲几十行日志 */
constexpr size_t kRingCap = 8192;
constexpr size_t kUdpPayloadCap = 1024;
constexpr int kIdlePollMs = 100;
constexpr int kResolveRetryMs = 5000;

#pragma pack(push, 1)
struct PacketHeader {
    char magic[4];
    uint8_t mac[6];
    uint16_t seq;
    uint16_t len;
};
#pragma pack(pop)

struct ServerCfg {
    char host[64];
    uint16_t port;
    bool ok;
};

static vprintf_like_t s_prev_vprintf = nullptr;
static TaskHandle_t s_task = nullptr;
static SemaphoreHandle_t s_data_sem = nullptr;
static SemaphoreHandle_t s_net_sem = nullptr;

static portMUX_TYPE s_ring_mux = portMUX_INITIALIZER_UNLOCKED;
static char s_ring[kRingCap];
static size_t s_ring_head = 0;  // 写
static size_t s_ring_tail = 0;  // 读
static size_t s_ring_used = 0;

static uint8_t s_mac[6] = {};
static uint16_t s_seq = 0;
static volatile bool s_started = false;
static volatile bool s_ip_ready = false;
/** 防重入：钩子内再打日志直接走原 vprintf */
static volatile uint32_t s_hook_depth = 0;

ServerCfg ParseServer(const char* raw) {
    ServerCfg out{};
    if (raw == nullptr || raw[0] == '\0') {
        return out;
    }

    // 拷到本地，去掉 scheme / path
    char buf[96];
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* s = buf;
    char* scheme = strstr(s, "://");
    if (scheme != nullptr) {
        s = scheme + 3;
    }
    char* slash = strchr(s, '/');
    if (slash != nullptr) {
        *slash = '\0';
    }

    char* colon = strrchr(s, ':');
    if (colon == nullptr || colon == s) {
        return out;
    }
    *colon = '\0';
    const int port = atoi(colon + 1);
    if (port <= 0 || port > 65535 || s[0] == '\0') {
        return out;
    }

    strncpy(out.host, s, sizeof(out.host) - 1);
    out.host[sizeof(out.host) - 1] = '\0';
    out.port = static_cast<uint16_t>(port);
    out.ok = true;
    return out;
}

size_t RingFree() {
    return kRingCap - s_ring_used;
}

void RingWriteBytes(const char* data, size_t len) {
    if (data == nullptr || len == 0) {
        return;
    }
    // 单次写入不超过环容量
    if (len > kRingCap) {
        data += (len - kRingCap);
        len = kRingCap;
    }

    portENTER_CRITICAL(&s_ring_mux);
    if (len > RingFree()) {
        const size_t drop = len - RingFree();
        s_ring_tail = (s_ring_tail + drop) % kRingCap;
        s_ring_used -= drop;
    }
    const size_t first = (s_ring_head + len <= kRingCap) ? len : (kRingCap - s_ring_head);
    memcpy(s_ring + s_ring_head, data, first);
    if (first < len) {
        memcpy(s_ring, data + first, len - first);
    }
    s_ring_head = (s_ring_head + len) % kRingCap;
    s_ring_used += len;
    portEXIT_CRITICAL(&s_ring_mux);

    if (s_data_sem != nullptr) {
        xSemaphoreGive(s_data_sem);
    }
}

size_t RingReadBytes(char* out, size_t max_len) {
    if (out == nullptr || max_len == 0) {
        return 0;
    }
    portENTER_CRITICAL(&s_ring_mux);
    const size_t n = s_ring_used < max_len ? s_ring_used : max_len;
    if (n == 0) {
        portEXIT_CRITICAL(&s_ring_mux);
        return 0;
    }
    const size_t first = (s_ring_tail + n <= kRingCap) ? n : (kRingCap - s_ring_tail);
    memcpy(out, s_ring + s_ring_tail, first);
    if (first < n) {
        memcpy(out + first, s_ring, n - first);
    }
    s_ring_tail = (s_ring_tail + n) % kRingCap;
    s_ring_used -= n;
    portEXIT_CRITICAL(&s_ring_mux);
    return n;
}

void RingDiscardAll() {
    portENTER_CRITICAL(&s_ring_mux);
    s_ring_head = 0;
    s_ring_tail = 0;
    s_ring_used = 0;
    portEXIT_CRITICAL(&s_ring_mux);
}

int HookedVprintf(const char* fmt, va_list args) {
    // ISR / 重入：绝不碰环与信号量
    if (xPortInIsrContext() || s_hook_depth > 0) {
        return s_prev_vprintf ? s_prev_vprintf(fmt, args) : vprintf(fmt, args);
    }

    s_hook_depth++;

    char stack_buf[kFormatCap];
    va_list copy;
    va_copy(copy, args);
    const int needed = vsnprintf(stack_buf, sizeof(stack_buf), fmt, copy);
    va_end(copy);

    if (needed > 0) {
        const size_t n = static_cast<size_t>(needed) < sizeof(stack_buf)
                             ? static_cast<size_t>(needed)
                             : (sizeof(stack_buf) - 1);
        RingWriteBytes(stack_buf, n);
    }

    const int ret = s_prev_vprintf ? s_prev_vprintf(fmt, args) : vprintf(fmt, args);
    s_hook_depth--;
    return ret;
}

void OnIpEvent(void* /*arg*/, esp_event_base_t event_base, int32_t event_id, void* /*event_data*/) {
    if (event_base != IP_EVENT) {
        return;
    }
    if (event_id == IP_EVENT_STA_GOT_IP) {
        s_ip_ready = true;
        if (s_net_sem != nullptr) {
            xSemaphoreGive(s_net_sem);
        }
    } else if (event_id == IP_EVENT_STA_LOST_IP) {
        s_ip_ready = false;
    }
}

bool ResolveIpv4(const ServerCfg& cfg, sockaddr_in* out) {
    if (!cfg.ok || out == nullptr) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port = htons(cfg.port);
    // 仅支持 IPv4 字面量（推荐配置）；主机名需 DNS，此处不做以免栈/阻塞风险
    if (inet_pton(AF_INET, cfg.host, &out->sin_addr) != 1) {
        return false;
    }
    return true;
}

void CloseSock(int* sock) {
    if (sock != nullptr && *sock >= 0) {
        close(*sock);
        *sock = -1;
    }
}

int OpenUdpSock() {
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return -1;
    }
    const int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
    return sock;
}

void SenderTask(void* /*arg*/) {
    const ServerCfg server = ParseServer(ESP_DEBUG_CLOUD_SERVER);
    if (!server.ok) {
        if (s_prev_vprintf != nullptr) {
            esp_log_set_vprintf(s_prev_vprintf);
            s_prev_vprintf = nullptr;
        }
        vTaskDelete(nullptr);
        return;
    }

    sockaddr_in dest{};
    if (!ResolveIpv4(server, &dest)) {
        // 配置了主机名而非 IP：卸钩退出，避免崩溃；请改用 x.x.x.x:port
        if (s_prev_vprintf != nullptr) {
            esp_log_set_vprintf(s_prev_vprintf);
            s_prev_vprintf = nullptr;
        }
        vTaskDelete(nullptr);
        return;
    }

    int sock = -1;
    TickType_t last_open_tick = 0;
    char payload[kUdpPayloadCap];
    uint8_t packet[sizeof(PacketHeader) + kUdpPayloadCap];

    while (true) {
        if (!s_ip_ready) {
            if (s_net_sem != nullptr) {
                xSemaphoreTake(s_net_sem, pdMS_TO_TICKS(1000));
            } else {
                vTaskDelay(pdMS_TO_TICKS(kIdlePollMs));
            }
            RingDiscardAll();
            CloseSock(&sock);
            continue;
        }

        if (s_data_sem != nullptr) {
            xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(kIdlePollMs));
        } else {
            vTaskDelay(pdMS_TO_TICKS(kIdlePollMs));
        }

        const size_t n = RingReadBytes(payload, sizeof(payload));
        if (n == 0) {
            continue;
        }

        if (sock < 0) {
            const TickType_t now = xTaskGetTickCount();
            if (last_open_tick != 0 &&
                (now - last_open_tick) < pdMS_TO_TICKS(kResolveRetryMs)) {
                continue;
            }
            last_open_tick = now;
            sock = OpenUdpSock();
            if (sock < 0) {
                continue;
            }
        }

        PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet);
        memcpy(hdr->magic, kMagic, 4);
        memcpy(hdr->mac, s_mac, 6);
        hdr->seq = s_seq++;
        hdr->len = static_cast<uint16_t>(n);
        memcpy(packet + sizeof(PacketHeader), payload, n);

        const ssize_t sent = sendto(sock, packet, sizeof(PacketHeader) + n, 0,
                                    reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
        if (sent < 0) {
            CloseSock(&sock);
        }
    }
}

void LoadMac() {
    if (esp_read_mac(s_mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        memset(s_mac, 0, sizeof(s_mac));
    }
}

}  // namespace

void EspDebugCloud_Start(void) {
    if (s_started) {
        return;
    }
    s_started = true;

    LoadMac();

    s_data_sem = xSemaphoreCreateBinary();
    s_net_sem = xSemaphoreCreateBinary();
    if (s_data_sem == nullptr || s_net_sem == nullptr) {
        s_started = false;
        return;
    }

    (void)esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &OnIpEvent, nullptr);
    (void)esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &OnIpEvent, nullptr);

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != nullptr) {
        esp_netif_ip_info_t info{};
        if (esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0) {
            s_ip_ready = true;
            xSemaphoreGive(s_net_sem);
        }
    }

    // 先起发送任务，再装钩子，避免早期日志撞上空任务
    const BaseType_t ok = xTaskCreate(SenderTask, "esp_dbg_cloud", 4096, nullptr, 1, &s_task);
    if (ok != pdPASS) {
        s_started = false;
        return;
    }

    s_prev_vprintf = esp_log_set_vprintf(HookedVprintf);

    // 用原路径打印（钩子已装，但深度保护下安全）；提示必须用 IPv4
    ESP_LOGI(kTag, "UDP log shipper -> %s (IPv4 only)", ESP_DEBUG_CLOUD_SERVER);
}

#endif  // ESP_DEBUG_CLOUD_ENABLE
