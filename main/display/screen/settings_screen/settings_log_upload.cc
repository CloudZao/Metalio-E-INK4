#include "settings_log_upload.h"

#include "api_endpoints.h"
#include "api_http.h"
#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "power_policy.h"
#include "reader/download_gate.h"
#include "screen_common.h"
#include "sd_log_writer.h"
#include "sd_paths.h"
#include "system_info.h"
#include "device_state.h"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#include <cJSON.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_lv_adapter.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <sys/stat.h>

namespace {

constexpr const char* TAG = "SettingsLogUp";
constexpr const char* kLogFileName = "metalio_logs.txt";
constexpr const char* kCoredumpFileName = "metalio_coredump.elf"; // 仅 SD 路径，不读 Flash 槽
constexpr const char* kBoundary = "----ESP32_DEVICE_LOG_BOUNDARY";
constexpr int kHttpTimeoutMs = 60000;
// 内 DRAM：读 SD / TLS 关 flash cache 时栈不能在 PSRAM（否则 esp_task_stack_is_sane_cache_disabled）
constexpr int kWorkerStack = 8 * 1024;
constexpr size_t kReadChunk = 256;      // 读盘块：小栈缓冲
constexpr size_t kRespCap = 192;        // 业务 JSON 很短，勿 ReadAll 拉大包
constexpr int64_t kProgUiIntervalUs = 200 * 1000;
constexpr uint32_t kResultHoldMs = 2000;
constexpr lv_coord_t kBorderW = 2;
constexpr lv_coord_t kRadius = 8;

enum class UploadResultKind : uint8_t {
    Ok = 0,
    Fail,
    Cancelled,
    NoFile,
    NetFail,
};

struct UploadDoneMsg {
    uint32_t gen = 0;
    UploadResultKind kind = UploadResultKind::Fail;
};

struct UploadProgMsg {
    uint32_t gen = 0;
    int percent = 0;
    int done = 0;  // 已上传完成数
    int total = 0; // 待同步总数
    char name[40] = {}; // 当前文件名（标题单行）
};

struct UploadUi {
    lv_obj_t* page = nullptr;
    lv_obj_t* title_lbl = nullptr;
    lv_obj_t* count_lbl = nullptr; // 已上传 n / 共 m（对齐传输下载）
    lv_obj_t* bar = nullptr;
    lv_obj_t* pct_lbl = nullptr;
    lv_obj_t* cancel_btn = nullptr;
    lv_timer_t* close_timer = nullptr;
    int shown_percent = -1;
    int job_done = 0;
    int job_total = 0;
    bool open = false;
    bool settled = false; // 已出终态文案，忽略迟到的进度回调
};

UploadUi s_ui{};
DownloadGate s_gate{};
std::atomic<bool> s_active{false};
std::atomic<bool> s_worker_running{false};
std::atomic<uint32_t> s_gen{0};
std::atomic<int64_t> s_prog_last_ui_us{0};

int CountSdUploadJobs() {
    int n = 0;
    struct stat st {};
    if (stat(SD_PATH_RUNTIME_LOG, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
        ++n;
    }
    if (stat(SD_PATH_COREDUMP, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
        ++n;
    }
    return n;
}

void SetCountLabel(int done, int total) {
    if (s_ui.count_lbl == nullptr) {
        return;
    }
    if (total < 0) {
        total = 0;
    }
    if (done < 0) {
        done = 0;
    }
    if (done > total && total > 0) {
        done = total;
    }
    s_ui.job_done = done;
    s_ui.job_total = total;
    char buf[48];
    std::snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_LOG_UPLOAD_COUNT_FMT, done, total);
    lv_label_set_text(s_ui.count_lbl, buf);
}

void MacIdUpper(char* out, size_t out_cap) {
    if (out == nullptr || out_cap < 18) {
        return;
    }
    const std::string mac = SystemInfo::GetMacAddress();
    size_t n = mac.size();
    if (n >= out_cap) {
        n = out_cap - 1;
    }
    for (size_t i = 0; i < n; ++i) {
        out[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(mac[i])));
    }
    out[n] = '\0';
}

bool CodeIsZero(const cJSON* code) {
    if (cJSON_IsNumber(code)) {
        return code->valuedouble == 0.0;
    }
    if (cJSON_IsString(code) && code->valuestring != nullptr) {
        return code->valuestring[0] == '0' && code->valuestring[1] == '\0';
    }
    return false;
}

bool ParseUploadOk(const char* body) {
    if (body == nullptr || body[0] == '\0') {
        return false;
    }
    cJSON* root = cJSON_Parse(body);
    if (root == nullptr) {
        return false;
    }
    const cJSON* code = cJSON_GetObjectItem(root, "code");
    const bool ok = CodeIsZero(code);
    cJSON_Delete(root);
    return ok;
}

/** 上传会话：挡激活轮询 + SD 日志 hold；析构成对恢复（防漏恢复竞态） */
struct UploadSessionGuard {
    bool paused_activation = false;

    UploadSessionGuard() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateActivating) {
            app.PauseActivationPolling();
            paused_activation = true;
        }
        // 先停写再读文件，避免与 WriterTask/fopen 竞态
        SdLogWriter_BeginUploadHold();
    }

    ~UploadSessionGuard() {
        SdLogWriter_EndUploadHold();
        if (paused_activation) {
            Application::GetInstance().ResumeActivationPolling();
        }
    }

    UploadSessionGuard(const UploadSessionGuard&) = delete;
    UploadSessionGuard& operator=(const UploadSessionGuard&) = delete;
};

void CancelCloseTimer() {
    if (s_ui.close_timer != nullptr) {
        lv_timer_delete(s_ui.close_timer);
        s_ui.close_timer = nullptr;
    }
}

void HidePageLocked() {
    CancelCloseTimer();
    HapticSetZoneEarlyPulseEnabled(true);
    if (s_ui.page != nullptr) {
        lv_obj_add_flag(s_ui.page, LV_OBJ_FLAG_HIDDEN);
    }
    s_ui.open = false;
    s_ui.settled = false;
    s_ui.shown_percent = -1;
    s_active.store(false, std::memory_order_release);
}

void DestroyPage() {
    CancelCloseTimer();
    HapticSetZoneEarlyPulseEnabled(true);
    if (s_ui.page != nullptr) {
        lv_obj_delete(s_ui.page);
    }
    s_ui = {};
    s_active.store(false, std::memory_order_release);
}

void RefreshProgress(int percent) {
    if (!s_ui.open) {
        return;
    }
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    if (percent == s_ui.shown_percent) {
        return;
    }
    s_ui.shown_percent = percent;
    if (s_ui.bar != nullptr) {
        lv_bar_set_value(s_ui.bar, percent, LV_ANIM_OFF);
    }
    if (s_ui.pct_lbl != nullptr) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(s_ui.pct_lbl, buf);
    }
}

void SetTitle(const char* text) {
    if (s_ui.title_lbl != nullptr && text != nullptr) {
        lv_label_set_text(s_ui.title_lbl, text);
    }
}

void SetCancelEnabled(bool enabled) {
    if (s_ui.cancel_btn == nullptr) {
        return;
    }
    if (enabled) {
        lv_obj_add_flag(s_ui.cancel_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_state(s_ui.cancel_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_flag(s_ui.cancel_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(s_ui.cancel_btn, LV_STATE_DISABLED);
    }
}

void OnCancelClicked(lv_event_t* /*e*/) {
    SettingsLogUpload_RequestAbort("cancel_btn");
}

void EnsurePageBuilt() {
    if (s_ui.page != nullptr) {
        return;
    }
    lv_obj_t* parent = lv_screen_active();
    if (parent == nullptr) {
        return;
    }

    lv_obj_t* page = lv_obj_create(parent);
    s_ui.page = page;
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(page, 0, 0);
    ScreenApplyDotBackdrop(page);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(page);

    lv_obj_t* card = lv_obj_create(page);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, LV_HOR_RES - 48, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, kBorderW, 0);
    lv_obj_set_style_radius(card, kRadius, 0);
    lv_obj_set_style_pad_all(card, 18, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 14, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, [](lv_event_t* e) { lv_event_stop_bubbling(e); }, LV_EVENT_CLICKED,
                        nullptr);

    s_ui.title_lbl = lv_label_create(card);
    lv_label_set_text(s_ui.title_lbl, Lang::Strings::SETTINGS_LOG_UPLOADING);
    lv_obj_set_width(s_ui.title_lbl, LV_HOR_RES - 80);
    lv_obj_set_height(s_ui.title_lbl, lv_font_get_line_height(fontpack_lv_font_ui()));
    lv_label_set_long_mode(s_ui.title_lbl, LV_LABEL_LONG_DOT); // 单行超出省略
    lv_obj_set_style_text_align(s_ui.title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_ui.title_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.title_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.title_lbl, LV_OBJ_FLAG_CLICKABLE);

    s_ui.count_lbl = lv_label_create(card);
    lv_label_set_text(s_ui.count_lbl, "");
    lv_obj_set_width(s_ui.count_lbl, LV_HOR_RES - 80);
    lv_obj_set_style_text_align(s_ui.count_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_ui.count_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.count_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.count_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_coord_t bar_w = 320;
    if (bar_w > LV_HOR_RES - 80) {
        bar_w = LV_HOR_RES - 80;
    }
    s_ui.bar = lv_bar_create(card);
    lv_obj_set_size(s_ui.bar, bar_w, 28);
    lv_bar_set_range(s_ui.bar, 0, 100);
    lv_bar_set_value(s_ui.bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ui.bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ui.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_ui.bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_ui.bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ui.bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.bar, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_ui.bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_ui.bar, 4, LV_PART_INDICATOR);
    lv_obj_clear_flag(s_ui.bar, LV_OBJ_FLAG_CLICKABLE);

    s_ui.pct_lbl = lv_label_create(card);
    lv_label_set_text(s_ui.pct_lbl, "0%");
    lv_obj_set_style_text_font(s_ui.pct_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.pct_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.pct_lbl, LV_OBJ_FLAG_CLICKABLE);

    s_ui.cancel_btn = lv_obj_create(card);
    lv_obj_remove_style_all(s_ui.cancel_btn);
    lv_obj_set_size(s_ui.cancel_btn, 160, 44);
    lv_obj_set_style_bg_color(s_ui.cancel_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_ui.cancel_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_ui.cancel_btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_ui.cancel_btn, kBorderW, 0);
    lv_obj_set_style_radius(s_ui.cancel_btn, kRadius, 0);
    lv_obj_add_flag(s_ui.cancel_btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(s_ui.cancel_btn);
    lv_obj_add_event_cb(s_ui.cancel_btn, OnCancelClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* cancel_lbl = lv_label_create(s_ui.cancel_btn);
    lv_label_set_text(cancel_lbl, Lang::Strings::COMMON_CANCEL);
    lv_obj_set_style_text_font(cancel_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_center(cancel_lbl);
    lv_obj_clear_flag(cancel_lbl, LV_OBJ_FLAG_CLICKABLE);
}

void ShowPage(int job_total) {
    EnsurePageBuilt();
    if (s_ui.page == nullptr) {
        return;
    }
    CancelCloseTimer();
    s_ui.open = true;
    s_ui.settled = false;
    s_ui.shown_percent = -1;
    s_active.store(true, std::memory_order_release);
    SetTitle(Lang::Strings::SETTINGS_LOG_CONNECTING_NET);
    SetCountLabel(0, job_total > 0 ? job_total : 0);
    SetCancelEnabled(true);
    RefreshProgress(0);
    lv_obj_clear_flag(s_ui.page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.page);
    HapticSetZoneEarlyPulseEnabled(false);
}

void ScheduleCloseAfterHold() {
    CancelCloseTimer();
    SetCancelEnabled(false);
    s_ui.close_timer = lv_timer_create(
        [](lv_timer_t* timer) {
            (void)timer;
            s_ui.close_timer = nullptr;
            HidePageLocked();
        },
        kResultHoldMs, nullptr);
    if (s_ui.close_timer != nullptr) {
        lv_timer_set_repeat_count(s_ui.close_timer, 1);
    } else {
        HidePageLocked();
    }
}

void AsyncProgress(void* p) {
    auto* msg = static_cast<UploadProgMsg*>(p);
    if (msg->gen == s_gen.load(std::memory_order_acquire) && s_ui.open && !s_ui.settled) {
        if (msg->name[0] != '\0') {
            char title[96];
            std::snprintf(title, sizeof(title), Lang::Strings::SETTINGS_LOG_UPLOADING_NAME_FMT,
                          msg->name);
            SetTitle(title);
        } else {
            SetTitle(Lang::Strings::SETTINGS_LOG_UPLOADING);
        }
        SetCountLabel(msg->done, msg->total);
        RefreshProgress(msg->percent);
    }
    delete msg;
}

void ReportProgress(int percent, const char* filename, int done, int total) {
    if (s_gate.IsCancelled()) {
        return;
    }
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    const int64_t now_us = esp_timer_get_time();
    const int64_t last_us = s_prog_last_ui_us.load(std::memory_order_relaxed);
    const bool force = (percent <= 0) || (percent >= 99);
    if (!force && last_us != 0 && (now_us - last_us) < kProgUiIntervalUs) {
        return;
    }
    s_prog_last_ui_us.store(now_us, std::memory_order_relaxed);

    auto* msg = new (std::nothrow) UploadProgMsg{};
    if (msg == nullptr) {
        return;
    }
    msg->gen = s_gen.load(std::memory_order_acquire);
    msg->percent = percent;
    msg->done = done;
    msg->total = total;
    if (filename != nullptr) {
        std::snprintf(msg->name, sizeof(msg->name), "%s", filename);
    }
    if (!ScreenLvAsync(AsyncProgress, msg)) {
        delete msg;
    }
}

bool SdFileExistsNonEmpty(const char* path, size_t* size_out) {
    if (size_out != nullptr) {
        *size_out = 0;
    }
    if (path == nullptr) {
        return false;
    }
    struct stat st {};
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        return false;
    }
    if (size_out != nullptr) {
        *size_out = static_cast<size_t>(st.st_size);
    }
    return true;
}

/** 取消路径：只摘绑，Close/join 交给 AbortHttpAsync，避免双 Close 卡死 receive task */
void HandOffHttpOnCancel() {
    s_gate.UnbindHttp();
}

bool SessionAbandoned(uint32_t gen) {
    return gen != s_gen.load(std::memory_order_acquire) || s_gate.IsCancelled();
}

/** 上传 SD 上单个文件（form 字段名仍为 log）；done=已完成数（不含本文件） */
UploadResultKind UploadOneSdFile(const char* path, const char* filename, const char* content_type,
                                 int done, int job_total, uint32_t gen) {
    if (SessionAbandoned(gen)) {
        return UploadResultKind::Cancelled;
    }
    size_t file_bytes = 0;
    if (!SdFileExistsNonEmpty(path, &file_bytes)) {
        return UploadResultKind::NoFile;
    }

    FILE* fp = fopen(path, "rb");
    if (fp == nullptr) {
        return UploadResultKind::NoFile;
    }

    char mac[18];
    MacIdUpper(mac, sizeof(mac));
    const std::string url = api::DeviceLogsUploadUrl(mac);
    if (url.empty()) {
        fclose(fp);
        return UploadResultKind::Fail;
    }

    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        fclose(fp);
        return UploadResultKind::NetFail;
    }
    auto http = network->CreateHttp(3);
    if (http == nullptr) {
        fclose(fp);
        return UploadResultKind::Fail;
    }
    Http* http_raw = http.get();
    if (!s_gate.BindHttp(std::move(http))) {
        fclose(fp);
        return UploadResultKind::Cancelled;
    }

    char ctype_hdr[96];
    std::snprintf(ctype_hdr, sizeof(ctype_hdr), "multipart/form-data; boundary=%s", kBoundary);
    api::ApplyCommonHeaders(http_raw);
    http_raw->SetHeader("Content-Type", ctype_hdr);
    http_raw->SetTimeout(kHttpTimeoutMs);

    api::LogHttpBinaryRequest(TAG, "POST", url, file_bytes, "device/logs");
    ESP_LOGI(TAG, "upload %s %u bytes mac=%s (%d/%d)", filename, static_cast<unsigned>(file_bytes),
             mac, done, job_total);
    ReportProgress(0, filename, done, job_total);

    if (!http_raw->Open("POST", url)) {
        fclose(fp);
        if (SessionAbandoned(gen)) {
            HandOffHttpOnCancel();
            return UploadResultKind::Cancelled;
        }
        s_gate.CloseHttp();
        return UploadResultKind::Fail;
    }
    if (SessionAbandoned(gen)) {
        fclose(fp);
        HandOffHttpOnCancel();
        return UploadResultKind::Cancelled;
    }

    {
        char file_header[220];
        const int hdr_n = std::snprintf(
            file_header, sizeof(file_header),
            "--%s\r\nContent-Disposition: form-data; name=\"log\"; filename=\"%s\"\r\n"
            "Content-Type: %s\r\n\r\n",
            kBoundary, filename, content_type != nullptr ? content_type : "application/octet-stream");
        if (hdr_n <= 0 || http_raw->Write(file_header, static_cast<size_t>(hdr_n)) < 0) {
            fclose(fp);
            if (SessionAbandoned(gen)) {
                HandOffHttpOnCancel();
                return UploadResultKind::Cancelled;
            }
            s_gate.CloseHttp();
            return UploadResultKind::Fail;
        }
    }

    size_t sent = 0;
    char chunk[kReadChunk];
    while (true) {
        if (SessionAbandoned(gen)) {
            fclose(fp);
            HandOffHttpOnCancel();
            return UploadResultKind::Cancelled;
        }
        const size_t n = fread(chunk, 1, sizeof(chunk), fp);
        if (n == 0) {
            break;
        }
        if (http_raw->Write(chunk, n) < 0) {
            fclose(fp);
            if (SessionAbandoned(gen)) {
                HandOffHttpOnCancel();
                return UploadResultKind::Cancelled;
            }
            s_gate.CloseHttp();
            return UploadResultKind::Fail;
        }
        sent += n;
        if (file_bytes > 0) {
            int pct = static_cast<int>((sent * 100ULL) / file_bytes);
            if (pct > 99) {
                pct = 99;
            }
            ReportProgress(pct, filename, done, job_total);
        }
    }
    fclose(fp);

    if (sent == 0) {
        s_gate.CloseHttp();
        return UploadResultKind::NoFile;
    }

    {
        char footer[48];
        const int foot_n = std::snprintf(footer, sizeof(footer), "\r\n--%s--\r\n", kBoundary);
        if (foot_n <= 0 || http_raw->Write(footer, static_cast<size_t>(foot_n)) < 0) {
            if (SessionAbandoned(gen)) {
                HandOffHttpOnCancel();
                return UploadResultKind::Cancelled;
            }
            s_gate.CloseHttp();
            return UploadResultKind::Fail;
        }
    }
    http_raw->Write("", 0);

    if (SessionAbandoned(gen)) {
        HandOffHttpOnCancel();
        return UploadResultKind::Cancelled;
    }

    const int status = http_raw->GetStatusCode();
    char resp[kRespCap];
    size_t resp_n = 0;
    while (resp_n + 1 < sizeof(resp)) {
        const int r = http_raw->Read(resp + resp_n, sizeof(resp) - 1 - resp_n);
        if (r <= 0) {
            break;
        }
        resp_n += static_cast<size_t>(r);
    }
    resp[resp_n] = '\0';
    char drain[64];
    while (http_raw->Read(drain, sizeof(drain)) > 0) {
    }
    s_gate.CloseHttp();
    api::LogHttpResponse(TAG, status, resp);

    if (status < 200 || status >= 300 || !ParseUploadOk(resp)) {
        ESP_LOGW(TAG, "upload %s failed status=%d sent=%u body=%s", filename, status,
                 static_cast<unsigned>(sent), resp_n == 0 ? "(empty)" : resp);
        return UploadResultKind::Fail;
    }
    ReportProgress(100, filename, done + 1, job_total);
    ESP_LOGI(TAG, "upload %s ok status=%d sent=%u", filename, status, static_cast<unsigned>(sent));
    return UploadResultKind::Ok;
}

/** 依次上传 SD：运行日志 + 崩溃 dump（有则传；均无则 NoFile） */
UploadResultKind UploadSdLogFiles(uint32_t gen) {
    const bool have_log = SdFileExistsNonEmpty(SD_PATH_RUNTIME_LOG, nullptr);
    const bool have_elf = SdFileExistsNonEmpty(SD_PATH_COREDUMP, nullptr);
    const int job_total = (have_log ? 1 : 0) + (have_elf ? 1 : 0);
    if (job_total == 0) {
        return UploadResultKind::NoFile;
    }

    int done = 0;
    if (have_log) {
        const UploadResultKind k = UploadOneSdFile(SD_PATH_RUNTIME_LOG, kLogFileName, "text/plain",
                                                   done, job_total, gen);
        if (k != UploadResultKind::Ok) {
            return k;
        }
        ++done;
    }

    if (have_elf) {
        // 只读 SD 上已导出的 metalio_coredump.elf，不读 Flash coredump 分区
        const UploadResultKind k = UploadOneSdFile(SD_PATH_COREDUMP, kCoredumpFileName,
                                                   "application/octet-stream", done, job_total, gen);
        if (k != UploadResultKind::Ok) {
            return k;
        }
    }
    return UploadResultKind::Ok;
}

void AsyncDone(void* p) {
    auto* msg = static_cast<UploadDoneMsg*>(p);
    s_worker_running.store(false, std::memory_order_release);
    if (msg->gen != s_gen.load(std::memory_order_acquire) || !s_ui.open) {
        ESP_LOGW(TAG, "done dropped kind=%u open=%d", static_cast<unsigned>(msg->kind),
                 s_ui.open ? 1 : 0);
        delete msg;
        s_active.store(false, std::memory_order_release);
        return;
    }

    s_ui.settled = true; // 挡迟到的 AsyncProgress，避免盖住「上传失败」
    ESP_LOGI(TAG, "ui done kind=%u", static_cast<unsigned>(msg->kind));
    switch (msg->kind) {
        case UploadResultKind::Ok:
            SetTitle(Lang::Strings::SETTINGS_LOG_UPLOAD_OK);
            if (s_ui.job_total > 0) {
                SetCountLabel(s_ui.job_total, s_ui.job_total);
            }
            RefreshProgress(100);
            ScheduleCloseAfterHold();
            break;
        case UploadResultKind::Cancelled:
            SetTitle(Lang::Strings::CLOUD_CANCELLED);
            ScheduleCloseAfterHold();
            break;
        case UploadResultKind::NoFile:
            SetTitle(Lang::Strings::SETTINGS_LOG_UPLOAD_NO_FILE);
            ScheduleCloseAfterHold();
            break;
        case UploadResultKind::NetFail:
            SetTitle(Lang::Strings::SETTINGS_CONV_NET_NOT_READY);
            ScheduleCloseAfterHold();
            break;
        case UploadResultKind::Fail:
        default:
            SetTitle(Lang::Strings::SETTINGS_LOG_UPLOAD_FAIL);
            ScheduleCloseAfterHold();
            break;
    }
    delete msg;
}

void UploadWorker(void* /*arg*/) {
    {
        // PowerNeedHold / UploadSessionGuard 须在 vTaskDeleteWithCaps 前析构
        PowerNeedHold hold(PowerNeed::OtaDownload);
        UploadSessionGuard session;
        const uint32_t gen = s_gen.load(std::memory_order_acquire);
        UploadResultKind kind = UploadResultKind::Fail;

        if (SessionAbandoned(gen)) {
            kind = UploadResultKind::Cancelled;
        } else if (!Board::GetInstance().EnsureNetworkReady()) {
            kind = SessionAbandoned(gen) ? UploadResultKind::Cancelled : UploadResultKind::NetFail;
        } else if (SessionAbandoned(gen)) {
            kind = UploadResultKind::Cancelled;
        } else {
            kind = UploadSdLogFiles(gen);
        }

        auto* msg = new (std::nothrow) UploadDoneMsg{};
        if (msg == nullptr) {
            s_worker_running.store(false, std::memory_order_release);
            s_active.store(false, std::memory_order_release);
            vTaskDeleteWithCaps(nullptr);
            return;
        }
        msg->gen = gen;
        msg->kind = kind;
        // 先清 running：返回键可区分「已收工等刷 UI」与「仍在传」
        s_worker_running.store(false, std::memory_order_release);
        // 终态直接 Schedule+持锁调用，避免 lv_async_call 队列满时丢掉「上传失败」
        Application::GetInstance().Schedule([msg]() {
            if (!esp_lv_adapter_is_initialized() || esp_lv_adapter_lock(-1) != ESP_OK) {
                delete msg;
                s_active.store(false, std::memory_order_release);
                return;
            }
            AsyncDone(msg);
            esp_lv_adapter_unlock();
        });
    }
    vTaskDeleteWithCaps(nullptr);
}

std::atomic<bool> s_abort_task_busy{false};

/** Http::Close/join 可能堵数秒：独立任务，勿占 MainEventLoop / 勿在 LVGL 锁内 */
void AbortHttpAsync() {
    bool expected = false;
    if (!s_abort_task_busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }
    if (xTaskCreatePinnedToCoreWithCaps(
            [](void* /*arg*/) {
                s_gate.AbortBoundHttp();
                s_abort_task_busy.store(false, std::memory_order_release);
                vTaskDeleteWithCaps(nullptr);
            },
            "log_up_ab", 8 * 1024, nullptr, 5, nullptr, 0,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) != pdPASS) {
        s_abort_task_busy.store(false, std::memory_order_release);
        ESP_LOGW(TAG, "abort task create failed");
        Application::GetInstance().Schedule([]() { s_gate.AbortBoundHttp(); });
    }
}

void BeginCancelSession(const char* why) {
    ESP_LOGI(TAG, "dismiss (%s)", why != nullptr ? why : "?");
    s_gen.fetch_add(1, std::memory_order_acq_rel);
    s_gate.RequestCancel();
    AbortHttpAsync();
    // 立刻放行再次点击；旧 worker 靠 gen 丢 UI，Close 由 abort 任务收尾
    s_active.store(false, std::memory_order_release);
    s_worker_running.store(false, std::memory_order_release);
}

void AsyncHidePage(void* /*p*/) {
    HidePageLocked();
}

void AsyncDismissAndGoHome(void* /*p*/) {
    HidePageLocked();
    ScreenGoHome();
}

void QueueHidePage() {
    if (lv_async_call(AsyncHidePage, nullptr) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "lv_async_call hide failed");
        s_active.store(false, std::memory_order_release);
    }
}

}  // namespace

bool SettingsLogUpload_IsActive() {
    return s_active.load(std::memory_order_acquire);
}

void SettingsLogUpload_RequestAbort(const char* why) {
    if (!SettingsLogUpload_IsActive() && !s_worker_running.load(std::memory_order_acquire)) {
        return;
    }
    // 一律作废会话并收起进度（取消/待机/电源）；勿再套「正在取消」异步改标题
    BeginCancelSession(why);
    QueueHidePage();
}

void SettingsLogUpload_Teardown() {
    BeginCancelSession("teardown");
    DestroyPage();
    // worker 自行清 s_worker_running；勿在此强清以免与进行中的 HTTP 叠任务
}

bool SettingsLogUpload_OnVkKey(const char* key) {
    if (key == nullptr || !SettingsLogUpload_IsActive()) {
        return false;
    }
    // 短按 Home / 返回：只关上传层，留在设置（盖板 Home 长按才回系统首页）
    if (std::strcmp(key, "vk_home") == 0 || std::strcmp(key, "vk_prev") == 0) {
        BeginCancelSession(key);
        QueueHidePage();
        return true;
    }
    return true;
}

void SettingsLogUpload_DismissAndGoHome() {
    if (!SettingsLogUpload_IsActive() && !s_worker_running.load(std::memory_order_acquire)) {
        ScreenRequestHome();
        return;
    }
    BeginCancelSession("vk_home_long");
    if (lv_async_call(AsyncDismissAndGoHome, nullptr) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "lv_async_call home failed");
        s_active.store(false, std::memory_order_release);
        ScreenRequestHome();
    }
}

void SettingsLogUpload_Start() {
    // 以进度页是否打开为准（取消后 active/running 已清；避免残留标志导致「点击无响应」）
    if (s_ui.open) {
        return;
    }
    auto& board = Board::GetInstance();
    if (board.IsWifiConfigMode()) {
        // 未配网：不全屏，由调用方/本处仅打日志；提示挂在全屏前用短暂 overlay 标题亦可
        EnsurePageBuilt();
        if (s_ui.page == nullptr) {
            ESP_LOGW(TAG, "need wifi cfg but no screen");
            return;
        }
        s_ui.open = true;
        s_active.store(true, std::memory_order_release);
        SetTitle(Lang::Strings::SETTINGS_LOG_NEED_NET);
        SetCountLabel(0, 0);
        RefreshProgress(0);
        SetCancelEnabled(false);
        lv_obj_clear_flag(s_ui.page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_ui.page);
        HapticSetZoneEarlyPulseEnabled(false);
        ScheduleCloseAfterHold();
        return;
    }

    // 踢掉可能残留的旧会话，再开新传
    s_gen.fetch_add(1, std::memory_order_acq_rel);
    s_gate.RequestCancel();
    AbortHttpAsync();
    s_gate.Reset();
    s_prog_last_ui_us.store(0, std::memory_order_relaxed);
    s_worker_running.store(true, std::memory_order_release);
    // 进页前统计待传文件数（hold 前 SD 仍可读）
    ShowPage(CountSdUploadJobs());

    if (xTaskCreatePinnedToCoreWithCaps(UploadWorker, "log_upload", kWorkerStack, nullptr, 5,
                                        nullptr, 0, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) !=
        pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate(log_upload) failed");
        s_worker_running.store(false, std::memory_order_release);
        SetTitle(Lang::Strings::SETTINGS_LOG_UPLOAD_FAIL);
        ScheduleCloseAfterHold();
    }
}
