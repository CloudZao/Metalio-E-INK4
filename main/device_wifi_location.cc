#include "device_wifi_location.h"

#include "api_endpoints.h"
#include "api_http.h"
#include "board.h"
#include "power_policy.h"

#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <cJSON.h>
#include <esp_event.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

namespace device_wifi_location {
namespace {

constexpr const char* TAG = "WifiLoc";
constexpr int kHttpTimeoutMs = 15000;
/** DRAM 栈：HTTP / cJSON / 扫网事件等待 */
constexpr uint32_t kWorkerStack = 10 * 1024;
constexpr UBaseType_t kWorkerPriority = tskIDLE_PRIORITY + 2;
constexpr size_t kMaxAps = 10;
/** 扫表上限，避免 ap_num 异常撑爆堆；再按 RSSI 截 TopN */
constexpr uint16_t kMaxScanRecords = 48;
constexpr int kScanTimeoutMs = 8000;
/** 链路稳定后再扫，减少与开机 OTA/同步抢射频 */
constexpr int kBootSettleMs = 2000;
constexpr EventBits_t kBitScanDone = BIT0;

std::atomic<bool> s_running{false};

struct ApItem {
    char ssid[33] = {};
    char mac[18] = {};
    int8_t signal = 0;
};

struct WifiStackOwned {
    bool owned = false;
    esp_netif_t* netif = nullptr;
};

bool CanReportNow() {
    auto& board = Board::GetInstance();
    if (board.IsWifiConfigMode()) {
        return false;
    }
    return board.IsNetworkReady();
}

void FormatBssid(const uint8_t bssid[6], char out[18]) {
    std::snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x", bssid[0], bssid[1], bssid[2],
                  bssid[3], bssid[4], bssid[5]);
}

void OnWifiEvent(void* arg, esp_event_base_t base, int32_t id, void* /*data*/) {
    auto* evt = static_cast<EventGroupHandle_t>(arg);
    if (evt == nullptr) {
        return;
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        xEventGroupSetBits(evt, kBitScanDone);
    }
}

/**
 * 保证 STA 射频可扫：WiFi 已起则复用；4G 在线时临时拉起只扫、扫完由调用方 teardown。
 * 绝不调用 WifiStation::Start，避免与保网/重连状态机缠在一起。
 */
bool EnsureWifiRadioForScan(WifiStackOwned& stack) {
    stack.owned = false;
    stack.netif = nullptr;

    wifi_mode_t mode = WIFI_MODE_NULL;
    const esp_err_t mode_err = esp_wifi_get_mode(&mode);
    if (mode_err == ESP_OK && mode != WIFI_MODE_NULL) {
        // 已 init 但 stop（如浅睡）：尝试 start；不置 owned，交回原所有者
        // 已在跑则 start 返回 INVALID_STATE；勿当成失败
        const esp_err_t start_err = esp_wifi_start();
        if (start_err != ESP_OK && start_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "esp_wifi_start: %s", esp_err_to_name(start_err));
            return false;
        }
        return true;
    }

    // 未 init：临时自建 STA 只扫（典型：当前走 4G）

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event_loop: %s", esp_err_to_name(err));
        return false;
    }

    stack.netif = esp_netif_create_default_wifi_sta();
    if (stack.netif == nullptr) {
        ESP_LOGE(TAG, "create default wifi sta failed");
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = false;
    err = esp_wifi_init(&cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        // WiFi 实际已由别处 init：丢掉刚建的重复 netif，复用现有栈，切勿 deinit
        ESP_LOGW(TAG, "wifi already init; reuse without ownership");
        esp_netif_destroy(stack.netif);
        stack.netif = nullptr;
        err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "esp_wifi_set_mode(reuse): %s", esp_err_to_name(err));
            return false;
        }
        err = esp_wifi_start();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "esp_wifi_start(reuse): %s", esp_err_to_name(err));
            return false;
        }
        return true;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(err));
        esp_netif_destroy(stack.netif);
        stack.netif = nullptr;
        return false;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode: %s", esp_err_to_name(err));
        esp_wifi_deinit();
        esp_netif_destroy(stack.netif);
        stack.netif = nullptr;
        return false;
    }
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_wifi_start(owned): %s", esp_err_to_name(err));
        esp_wifi_deinit();
        esp_netif_destroy(stack.netif);
        stack.netif = nullptr;
        return false;
    }

    stack.owned = true;
    return true;
}

void TeardownOwnedWifi(WifiStackOwned& stack) {
    if (!stack.owned) {
        return;
    }
    esp_wifi_scan_stop();
    esp_wifi_stop();
    esp_wifi_deinit();
    if (stack.netif != nullptr) {
        esp_netif_destroy(stack.netif);
        stack.netif = nullptr;
    }
    stack.owned = false;
}

bool ScanNearbyAps(std::vector<ApItem>& out) {
    out.clear();

    WifiStackOwned stack;
    if (!EnsureWifiRadioForScan(stack)) {
        return false;
    }

    EventGroupHandle_t scan_evt = xEventGroupCreate();
    if (scan_evt == nullptr) {
        TeardownOwnedWifi(stack);
        return false;
    }

    esp_event_handler_instance_t scan_inst = nullptr;
    esp_err_t reg =
        esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &OnWifiEvent,
                                            scan_evt, &scan_inst);
    if (reg != ESP_OK) {
        ESP_LOGE(TAG, "register SCAN_DONE: %s", esp_err_to_name(reg));
        vEventGroupDelete(scan_evt);
        TeardownOwnedWifi(stack);
        return false;
    }

    auto cleanup = [&]() {
        // 先注销再删 EventGroup，避免 SCAN_DONE 晚到 SetBits 已释放句柄
        if (scan_inst != nullptr) {
            esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, scan_inst);
            scan_inst = nullptr;
        }
        if (scan_evt != nullptr) {
            vEventGroupDelete(scan_evt);
            scan_evt = nullptr;
        }
        TeardownOwnedWifi(stack);
    };

    // 停掉可能残留的扫网，再发短时主动扫（不阻塞业务线程）
    (void)esp_wifi_scan_stop();
    xEventGroupClearBits(scan_evt, kBitScanDone);

    wifi_scan_config_t cfg = {};
    cfg.show_hidden = false;
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 40;
    cfg.scan_time.active.max = 80;

    const esp_err_t start_err = esp_wifi_scan_start(&cfg, false);
    if (start_err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_start: %s", esp_err_to_name(start_err));
        cleanup();
        return false;
    }

    const EventBits_t bits =
        xEventGroupWaitBits(scan_evt, kBitScanDone, pdTRUE, pdTRUE, pdMS_TO_TICKS(kScanTimeoutMs));
    if (!(bits & kBitScanDone)) {
        ESP_LOGW(TAG, "scan timeout (%d ms)", kScanTimeoutMs);
        (void)esp_wifi_scan_stop();
        cleanup();
        return false;
    }

    uint16_t ap_num = 0;
    (void)esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num == 0) {
        cleanup();
        return true;
    }

    uint16_t fetch = ap_num;
    if (fetch > kMaxScanRecords) {
        fetch = kMaxScanRecords;
    }

    // nothrow：堆紧时失败返回，避免 vector 构造 abort
    auto* records = static_cast<wifi_ap_record_t*>(
        heap_caps_malloc(sizeof(wifi_ap_record_t) * fetch, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (records == nullptr) {
        records = static_cast<wifi_ap_record_t*>(
            heap_caps_malloc(sizeof(wifi_ap_record_t) * fetch, MALLOC_CAP_DEFAULT));
    }
    if (records == nullptr) {
        ESP_LOGW(TAG, "scan OOM records=%u", static_cast<unsigned>(fetch));
        cleanup();
        return false;
    }

    uint16_t got = fetch;
    const esp_err_t get_err = esp_wifi_scan_get_ap_records(&got, records);
    // 无论成败先卸 handler / 临时栈，避免泄漏
    cleanup();
    if (get_err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_get_ap_records: %s", esp_err_to_name(get_err));
        heap_caps_free(records);
        return false;
    }

    std::sort(records, records + got,
              [](const wifi_ap_record_t& a, const wifi_ap_record_t& b) { return a.rssi > b.rssi; });

    out.reserve(std::min(static_cast<size_t>(got), kMaxAps));
    for (uint16_t i = 0; i < got; ++i) {
        if (out.size() >= kMaxAps) {
            break;
        }
        const wifi_ap_record_t& r = records[i];
        // SSID 最长 32 字节且可不带 '\0'，必须显式截断
        char ssid[33];
        std::memcpy(ssid, r.ssid, 32);
        ssid[32] = '\0';
        if (ssid[0] == '\0') {
            continue;
        }
        // 同 BSSID 去重（同名多 AP 保留）
        char mac[18];
        FormatBssid(r.bssid, mac);
        bool dup = false;
        for (const auto& ex : out) {
            if (std::strcmp(ex.mac, mac) == 0) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }

        ApItem item;
        std::memcpy(item.ssid, ssid, sizeof(item.ssid));
        std::memcpy(item.mac, mac, sizeof(item.mac));
        item.signal = r.rssi;
        out.push_back(item);
    }
    heap_caps_free(records);

    ESP_LOGI(TAG, "scan ok ap_raw=%u kept=%u", static_cast<unsigned>(got),
             static_cast<unsigned>(out.size()));
    return true;
}

std::string BuildBody(const std::vector<ApItem>& aps) {
    cJSON* arr = cJSON_CreateArray();
    if (arr == nullptr) {
        return {};
    }

    for (const auto& ap : aps) {
        cJSON* item = cJSON_CreateObject();
        if (item == nullptr) {
            continue;
        }
        if (cJSON_AddStringToObject(item, "ssid", ap.ssid) == nullptr ||
            cJSON_AddStringToObject(item, "mac", ap.mac) == nullptr ||
            cJSON_AddNumberToObject(item, "signal", ap.signal) == nullptr) {
            cJSON_Delete(item);
            continue;
        }
        if (!cJSON_AddItemToArray(arr, item)) {
            cJSON_Delete(item);
        }
    }

    char* printed = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (printed == nullptr) {
        return {};
    }
    std::string body(printed);
    cJSON_free(printed);
    return body;
}

void PostLocation(const std::string& body) {
    if (body.empty()) {
        ESP_LOGW(TAG, "skip POST: empty body");
        return;
    }
    if (!CanReportNow()) {
        ESP_LOGW(TAG, "skip POST: network gone or wifi config mode");
        return;
    }

    // 不调 EnsureNetworkReady：开机门闩已确认在线；避免 4G/WiFi 失败路径硬占网续扫
    PowerNeedHold hold_net(PowerNeed::OtaDownload);

    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        ESP_LOGW(TAG, "skip POST: no network iface");
        return;
    }
    auto http = network->CreateHttp(0);
    if (http == nullptr) {
        ESP_LOGW(TAG, "skip POST: CreateHttp failed");
        return;
    }

    const std::string url = api::DeviceLocationWifiUrl();
    if (url.empty()) {
        ESP_LOGI(TAG, "skip POST: cloud endpoints blank");
        return;
    }
    http->SetTimeout(kHttpTimeoutMs);
    api::ApplyJsonHeaders(http);
    api::LogHttpRequest(TAG, "POST", url, body);
    http->SetContent(std::string(body));
    if (!http->Open("POST", url)) {
        api::LogHttpResponse(TAG, -1, "open failed");
        ESP_LOGW(TAG, "POST open failed");
        return;
    }

    const int status = http->GetStatusCode();
    const std::string resp = http->ReadAll();
    http->Close();
    api::LogHttpResponse(TAG, status, api::RedactClawUrlsForLog(resp));

    // 按需求：不做业务解析，整段 JSON 打出来即可
    ESP_LOGI(TAG, "location status=%d json=%s", status, resp.empty() ? "(empty)" : resp.c_str());
}

void WorkerTask(void* /*arg*/) {
    {
        ESP_LOGI(TAG, "boot wifi-location start");

        if (!CanReportNow()) {
            ESP_LOGW(TAG, "skip: not ready (config_mode or offline)");
        } else {
            vTaskDelay(pdMS_TO_TICKS(kBootSettleMs));
            if (!CanReportNow()) {
                ESP_LOGW(TAG, "skip: network lost during settle");
            } else {
                std::vector<ApItem> aps;
                if (!ScanNearbyAps(aps)) {
                    ESP_LOGW(TAG, "scan failed");
                } else if (aps.empty()) {
                    ESP_LOGW(TAG, "skip POST: no AP");
                } else {
                    const std::string body = BuildBody(aps);
                    aps.clear();
                    aps.shrink_to_fit();
                    if (body.empty()) {
                        ESP_LOGW(TAG, "skip: build JSON failed");
                    } else {
                        PostLocation(body);
                    }
                }
            }
        }
    }

    s_running.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

}  // namespace

void RequestBootReport() {
    bool expected = false;
    if (!s_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        ESP_LOGD(TAG, "boot wifi-location already running");
        return;
    }

    const BaseType_t ok =
        xTaskCreatePinnedToCore(WorkerTask, "wifi_loc", kWorkerStack, nullptr, kWorkerPriority,
                                nullptr, 0);
    if (ok != pdPASS) {
        s_running.store(false, std::memory_order_release);
        ESP_LOGW(TAG, "xTaskCreate(wifi_loc) failed");
    }
}

}  // namespace device_wifi_location
