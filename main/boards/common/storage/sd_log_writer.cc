#include "sd_log_writer.h"

#include "sd_paths.h"
#include "SdCardManager.hpp"
#include "settings.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <esp_core_dump.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <sdkconfig.h>

namespace {

constexpr const char* TAG = "SdLog";
constexpr const char* kNvsNs = "display";
constexpr const char* kNvsKey = "sd_log";
constexpr bool kDefaultEnabled = true;

constexpr size_t kMaxFileBytes = 256 * 1024; // 256KB 环形上限
constexpr size_t kRingCap = 12 * 1024;
/** 钩子内仅此一块栈缓冲（对齐 esp_debug_cloud）；忌再叠大数组，小栈任务会 overflow */
constexpr size_t kFormatCap = 256;

constexpr const char* kLogFilePath = SD_PATH_RUNTIME_LOG;
constexpr size_t kCoredumpChunk = 1024;

vprintf_like_t s_prev_vprintf = nullptr;
TaskHandle_t s_task = nullptr;
SemaphoreHandle_t s_data_sem = nullptr;

portMUX_TYPE s_ring_mux = portMUX_INITIALIZER_UNLOCKED;
char s_ring[kRingCap];
size_t s_ring_head = 0;
size_t s_ring_tail = 0;
size_t s_ring_used = 0;

volatile bool s_enabled = kDefaultEnabled;
volatile bool s_hooked = false;
volatile bool s_sd_ready = false; // false：卸载中/未挂载，禁止 fopen/写盘（防 MSC 竞态）
volatile uint32_t s_hook_depth = 0;
volatile uint8_t s_upload_hold = 0; // 上传占用计数：>0 时停采环/写盘（不改 NVS）

FILE* s_fp = nullptr;
size_t s_write_pos = 0;
SemaphoreHandle_t s_file_mu = nullptr;

void LoadFromNvs() {
    Settings settings(kNvsNs, false);
    s_enabled = settings.GetBool(kNvsKey, kDefaultEnabled);
}

void PersistTask(void* arg) {
    const bool enabled = (arg != nullptr);
    Settings settings(kNvsNs, true);
    settings.SetBool(kNvsKey, enabled);
    vTaskDelete(nullptr);
}

size_t RingFree() {
    return kRingCap - s_ring_used;
}

void RingWriteBytes(const char* data, size_t len) {
    // 卸载 / 虚拟 U 盘 / 上传占用：不落盘也不进环
    if (data == nullptr || len == 0 || !s_enabled || !s_sd_ready || s_upload_hold != 0) {
        return;
    }
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

/** 就地去掉 \\r，并保证以 \\n 结尾；返回新长度 */
size_t NormalizeLineInPlace(char* s, size_t len, size_t cap) {
    if (s == nullptr || cap < 2 || len == 0) {
        return 0;
    }
    size_t o = 0;
    for (size_t i = 0; i < len && o + 1 < cap; ++i) {
        if (s[i] == '\r') {
            continue;
        }
        s[o++] = s[i];
    }
    if (o == 0) {
        return 0;
    }
    if (s[o - 1] != '\n') {
        if (o + 1 >= cap) {
            o = cap - 2;
        }
        s[o++] = '\n';
    }
    s[o] = '\0';
    return o;
}

size_t FormatTimestamp(char* dst, size_t dst_cap) {
    if (dst == nullptr || dst_cap < 28) {
        return 0;
    }
    timeval tv {};
    gettimeofday(&tv, nullptr);
    tm t {};
    localtime_r(&tv.tv_sec, &t);
    const int ms = static_cast<int>(tv.tv_usec / 1000);
    const int n = snprintf(dst, dst_cap, "%04d-%02d-%02d %02d:%02d:%02d.%03d ",
                           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
                           t.tm_sec, ms);
    if (n <= 0) {
        return 0;
    }
    return static_cast<size_t>(n) < dst_cap ? static_cast<size_t>(n) : dst_cap - 1;
}

void CloseFileLocked() {
    if (s_fp != nullptr) {
        fflush(s_fp);
        fclose(s_fp);
        s_fp = nullptr;
    }
}

bool OpenFileLocked() {
    if (s_fp != nullptr) {
        return true;
    }
    // 卸载窗口内 IsMounted 可能仍为 true，必须以 s_sd_ready 为准
    if (!s_sd_ready || !SdCardManager::GetInstance().IsMounted()) {
        return false;
    }
    if (!SdEnsureDir("/sdcard/metalio") || !SdEnsureDir(SD_APP_ROOT) || !SdEnsureDir(SD_PATH_LOGS)) {
        return false;
    }

    struct stat st {};
    const bool exists = (stat(kLogFilePath, &st) == 0 && S_ISREG(st.st_mode));
    s_fp = fopen(kLogFilePath, exists ? "r+b" : "w+b");
    if (s_fp == nullptr) {
        return false;
    }
    if (exists && static_cast<size_t>(st.st_size) < kMaxFileBytes) {
        s_write_pos = static_cast<size_t>(st.st_size);
    } else if (exists) {
        s_write_pos = 0;
    } else {
        s_write_pos = 0;
    }
    if (fseek(s_fp, static_cast<long>(s_write_pos), SEEK_SET) != 0) {
        CloseFileLocked();
        return false;
    }
    return true;
}

bool WriteChunkLocked(const char* data, size_t len) {
    if (s_fp == nullptr || data == nullptr || len == 0) {
        return false;
    }
    size_t off = 0;
    while (off < len) {
        if (s_write_pos >= kMaxFileBytes) {
            s_write_pos = 0;
            if (fseek(s_fp, 0, SEEK_SET) != 0) {
                CloseFileLocked();
                return false;
            }
        }
        size_t room = kMaxFileBytes - s_write_pos;
        size_t n = len - off;
        if (n > room) {
            n = room;
        }
        const size_t w = fwrite(data + off, 1, n, s_fp);
        if (w == 0) {
            CloseFileLocked();
            return false;
        }
        s_write_pos += w;
        off += w;
        if (s_write_pos >= kMaxFileBytes) {
            s_write_pos = 0;
            if (fseek(s_fp, 0, SEEK_SET) != 0) {
                CloseFileLocked();
                return false;
            }
        }
    }
    return true;
}

void FlushRingToSd() {
    // 上传 hold 期间仍允许刷空环（BeginUploadHold 先置位再 drain）
    if (!s_enabled || !s_sd_ready || s_file_mu == nullptr) {
        return;
    }
    char chunk[1024];
    for (;;) {
        if (!s_sd_ready) {
            break;
        }
        const size_t n = RingReadBytes(chunk, sizeof(chunk));
        if (n == 0) {
            break;
        }
        xSemaphoreTake(s_file_mu, portMAX_DELAY);
        if (!s_enabled || !s_sd_ready || !OpenFileLocked()) {
            xSemaphoreGive(s_file_mu);
            // 已出环但无法写：塞回环头代价高，卸载场景直接丢本批
            break;
        }
        if (!WriteChunkLocked(chunk, n) || fflush(s_fp) != 0) {
            // 写失败则关掉 FILE*，下次 OpenFileLocked 重建
            CloseFileLocked();
            xSemaphoreGive(s_file_mu);
            ESP_LOGW(TAG, "flush write failed (errno=%d), will reopen", errno);
            break;
        }
        xSemaphoreGive(s_file_mu);
    }
}

void WriterTask(void* /*arg*/) {
    for (;;) {
        if (s_data_sem != nullptr) {
            xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        if (!s_enabled || !s_sd_ready || s_upload_hold != 0) {
            if (s_file_mu != nullptr) {
                xSemaphoreTake(s_file_mu, portMAX_DELAY);
                CloseFileLocked();
                xSemaphoreGive(s_file_mu);
            }
            continue;
        }
        FlushRingToSd();
    }
}

int HookedVprintf(const char* fmt, va_list args) {
    if (xPortInIsrContext() || s_hook_depth > 0 || !s_enabled) {
        return s_prev_vprintf ? s_prev_vprintf(fmt, args) : vprintf(fmt, args);
    }

    s_hook_depth++;

    // 单块小栈：时间戳 + 正文；禁止再叠第二块大缓冲
    char buf[kFormatCap];
    const size_t ts_n = FormatTimestamp(buf, sizeof(buf));
    if (ts_n + 4 < sizeof(buf)) {
        va_list copy;
        va_copy(copy, args);
        const int needed = vsnprintf(buf + ts_n, sizeof(buf) - ts_n, fmt, copy);
        va_end(copy);
        if (needed > 0) {
            size_t body_n = static_cast<size_t>(needed);
            if (body_n >= sizeof(buf) - ts_n) {
                body_n = sizeof(buf) - ts_n - 1;
            }
            const size_t total = NormalizeLineInPlace(buf, ts_n + body_n, sizeof(buf));
            if (total > 0) {
                RingWriteBytes(buf, total);
            }
        }
    }

    const int ret = s_prev_vprintf ? s_prev_vprintf(fmt, args) : vprintf(fmt, args);
    s_hook_depth--;
    return ret;
}

void InstallHook() {
    if (s_hooked) {
        return;
    }
    s_prev_vprintf = esp_log_set_vprintf(HookedVprintf);
    s_hooked = true;
}

void UninstallHook() {
    if (!s_hooked) {
        return;
    }
    if (s_prev_vprintf != nullptr) {
        esp_log_set_vprintf(s_prev_vprintf);
        s_prev_vprintf = nullptr;
    } else {
        esp_log_set_vprintf(vprintf);
    }
    s_hooked = false;
}

#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
/** Flash coredump → 覆盖写入 SD；仅整份写成功后擦 Flash */
bool ExportCoreDumpToSd() {
    if (!s_enabled || !s_sd_ready || !SdCardManager::GetInstance().IsMounted()) {
        return false;
    }

    esp_err_t err = esp_core_dump_image_check();
    if (err == ESP_ERR_NOT_FOUND) {
        return true;
    }
    if (err != ESP_OK) {
        // 空槽/旧槽常见 INVALID_SIZE，擦掉即可；勿当上传失败
        ESP_LOGI(TAG, "coredump flash slot stale (%s), erase", esp_err_to_name(err));
        esp_core_dump_image_erase();
        return false;
    }

    size_t flash_addr = 0;
    size_t dump_size = 0;
    err = esp_core_dump_image_get(&flash_addr, &dump_size);
    if (err != ESP_OK || dump_size == 0) {
        ESP_LOGW(TAG, "coredump get: %s", esp_err_to_name(err));
        return false;
    }

    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    if (part == nullptr) {
        ESP_LOGE(TAG, "coredump partition missing");
        return false;
    }
    if (dump_size > part->size) {
        ESP_LOGE(TAG, "coredump size %u > partition %u", static_cast<unsigned>(dump_size),
                 static_cast<unsigned>(part->size));
        esp_core_dump_image_erase();
        return false;
    }

    ESP_LOGI(TAG, "coredump export begin size=%u flash=0x%x", static_cast<unsigned>(dump_size),
             static_cast<unsigned>(flash_addr));

    if (!SdEnsureDir("/sdcard/metalio") || !SdEnsureDir(SD_APP_ROOT) || !SdEnsureDir(SD_PATH_LOGS)) {
        ESP_LOGW(TAG, "coredump: logs dir not ready");
        return false;
    }

    // 覆盖写：只保留最新一份，避免多次 dump 拼进同一文件
    FILE* fp = fopen(SD_PATH_COREDUMP, "wb");
    if (fp == nullptr) {
        ESP_LOGW(TAG, "coredump fopen write failed errno=%d", errno);
        return false;
    }

    uint8_t chunk[kCoredumpChunk];
    size_t off = 0;
    bool ok = true;
    while (off < dump_size) {
        const size_t n = (dump_size - off > kCoredumpChunk) ? kCoredumpChunk : (dump_size - off);
        err = esp_partition_read(part, off, chunk, n);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "coredump read @%u: %s", static_cast<unsigned>(off), esp_err_to_name(err));
            ok = false;
            break;
        }
        if (fwrite(chunk, 1, n, fp) != n) {
            ESP_LOGE(TAG, "coredump fwrite failed errno=%d", errno);
            ok = false;
            break;
        }
        off += n;
    }
    if (ok && fflush(fp) != 0) {
        ESP_LOGE(TAG, "coredump fflush failed errno=%d", errno);
        ok = false;
    }
    fclose(fp);

    if (!ok) {
        unlink(SD_PATH_COREDUMP);
        return false;
    }

    err = esp_core_dump_image_erase();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "coredump wrote %u bytes but flash erase: %s",
                 static_cast<unsigned>(dump_size), esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "coredump %u bytes wrote -> %s; flash cleared",
             static_cast<unsigned>(dump_size), SD_PATH_COREDUMP);
    return true;
}
#endif  // CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH

}  // namespace

void SdLogWriter_Start(void) {
    LoadFromNvs();
    if (s_file_mu == nullptr) {
        s_file_mu = xSemaphoreCreateMutex();
    }
    if (s_data_sem == nullptr) {
        s_data_sem = xSemaphoreCreateBinary();
    }
    if (s_task == nullptr) {
        // 内部 DRAM：FatFS 写路径可能关 cache
        BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(
            WriterTask, "sd_log", 4096, nullptr, tskIDLE_PRIORITY + 2, &s_task, 0,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (ok != pdPASS) {
            s_task = nullptr;
            ESP_LOGE(TAG, "create sd_log task failed");
            return;
        }
    }
    if (s_enabled) {
        InstallHook();
        if (SdCardManager::GetInstance().IsMounted()) {
            SdLogWriter_OnSdMounted();
        }
    }
}

/** 恢复日志写句柄；不含 Flash coredump 导出 */
static void ReopenLogFileAfterHoldOrMount() {
    if (!s_enabled || s_file_mu == nullptr) {
        return;
    }
    // 上传 hold：只记挂载就绪，勿与上传读文件抢开写句柄
    if (s_upload_hold != 0) {
        s_sd_ready = true;
        return;
    }
    xSemaphoreTake(s_file_mu, portMAX_DELAY);
    CloseFileLocked();
    // 先置位：即使本次 open 失败，WriterTask 仍会按 s_sd_ready 重试；
    // 卸载路径走 OnSdUnmounting 清位。勿因瞬时 fopen 失败永久停写。
    s_sd_ready = true;
    const bool ok = OpenFileLocked();
    xSemaphoreGive(s_file_mu);
    if (!ok) {
        ESP_LOGW(TAG, "OnSdMounted: open deferred (errno=%d), writer will retry", errno);
    }
    if (s_data_sem != nullptr) {
        xSemaphoreGive(s_data_sem);
    }
}

void SdLogWriter_OnSdMounted(void) {
    ReopenLogFileAfterHoldOrMount();
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
    // 仅真实挂载/开机路径导出；EndUploadHold 勿走此处（空 Flash 槽会刷 INVALID_SIZE）
    if (s_upload_hold == 0 && s_enabled && s_sd_ready) {
        ExportCoreDumpToSd();
    }
#endif
}

void SdLogWriter_OnSdUnmounting(void) {
    // 先清 ready：钩子不再进环；再关文件，避免卸 VFS 前再次 fopen
    s_sd_ready = false;
    if (s_file_mu == nullptr) {
        return;
    }
    xSemaphoreTake(s_file_mu, portMAX_DELAY);
    CloseFileLocked();
    xSemaphoreGive(s_file_mu);
    // 丢弃环内未落盘内容（U 盘期间不保存）
    portENTER_CRITICAL(&s_ring_mux);
    s_ring_head = 0;
    s_ring_tail = 0;
    s_ring_used = 0;
    portEXIT_CRITICAL(&s_ring_mux);
}

bool SdLogWriter_IsEnabled(void) {
    return s_enabled;
}

void SdLogWriter_SetEnabled(bool enabled) {
    s_enabled = enabled;
    if (enabled) {
        InstallHook();
        // 上传 hold 中勿抢开文件；EndUploadHold 会按最新开关重开
        if (s_upload_hold == 0 && SdCardManager::GetInstance().IsMounted()) {
            SdLogWriter_OnSdMounted();
        }
    } else {
        UninstallHook();
        if (s_file_mu != nullptr) {
            xSemaphoreTake(s_file_mu, portMAX_DELAY);
            CloseFileLocked();
            xSemaphoreGive(s_file_mu);
        }
    }
    if (xTaskCreate(PersistTask, "sd_log_nvs", 4096, enabled ? reinterpret_cast<void*>(1) : nullptr, 5,
                    nullptr) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate(sd_log_nvs) failed");
    }
}

void SdLogWriter_Flush(void) {
    FlushRingToSd();
    if (s_file_mu == nullptr) {
        return;
    }
    // FatFS：仅 fflush 时目录项 size 常仍为 0，二次 fopen/stat 读不到内容；须 fclose
    xSemaphoreTake(s_file_mu, portMAX_DELAY);
    CloseFileLocked();
    xSemaphoreGive(s_file_mu);
}

void SdLogWriter_BeginUploadHold(void) {
    if (s_upload_hold < 255) {
        ++s_upload_hold;
    }
    if (s_upload_hold != 1) {
        return;
    }
    // 先挡新日志，再刷空环并关句柄，避免与 WriterTask/二次 fopen 竞态读空
    FlushRingToSd();
    if (s_file_mu != nullptr) {
        xSemaphoreTake(s_file_mu, portMAX_DELAY);
        CloseFileLocked();
        xSemaphoreGive(s_file_mu);
    }
}

void SdLogWriter_EndUploadHold(void) {
    if (s_upload_hold == 0) {
        return;
    }
    --s_upload_hold;
    if (s_upload_hold != 0) {
        return;
    }
    // 只重开日志文件，不重新 Export Flash coredump（与本次上传的 SD elf 无关）
    if (s_enabled && SdCardManager::GetInstance().IsMounted()) {
        ReopenLogFileAfterHoldOrMount();
    }
}

void SdLogWriter_ExportCoreDumpIfNeeded(void) {
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
    if (!s_enabled || !SdCardManager::GetInstance().IsMounted()) {
        return;
    }
    if (!s_sd_ready) {
        // Start 前 Mount 曾早退；此处补齐 ready 再导出
        SdLogWriter_OnSdMounted();
        return;
    }
    ExportCoreDumpToSd();
#else
    ESP_LOGD(TAG, "coredump export skipped (not built to flash)");
#endif
}
