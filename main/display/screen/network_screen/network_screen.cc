#include "network_screen/network_screen.h"

#include "application.h"
#include "assets/lang_config.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "screen_common.h"
#include "ssid_manager.h"
#include "vk_key_handler.h"
#include "wifi_station.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

namespace {

constexpr const char* TAG = "NetworkScreen";
constexpr const char* kScreenId = "network";

constexpr size_t kMaxSsidLen = 32;
constexpr size_t kMaxPasswordLen = 64;
constexpr int kPageSizeFallback = 7;
constexpr int kRestartCountdownSec = 3;
constexpr int kRowPadVer = 6;  // SSID 与底部分割线留缝；单行由对称上下 padding 保证垂直居中
constexpr lv_coord_t kSavedActionBtnH = 40;  // 已保存行「设默认/删除」按钮高

constexpr EventBits_t kBitScanDone = BIT0;
constexpr EventBits_t kBitConnected = BIT1;
constexpr EventBits_t kBitDisconnected = BIT2;

enum class Tab : uint8_t { kNearby = 0, kSaved = 1 };

struct ApItem {
    std::string ssid;
    int8_t rssi = -127;
    wifi_auth_mode_t authmode = WIFI_AUTH_OPEN;
};

struct UiState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* list_panel = nullptr;  // 扫网/已保存主面板
    lv_obj_t* status_lbl = nullptr;
    lv_obj_t* nearby_tab_btn = nullptr;
    lv_obj_t* saved_tab_btn = nullptr;
    lv_obj_t* scan_btn = nullptr;
    lv_obj_t* list = nullptr;
    lv_obj_t* page_lbl = nullptr;
    lv_obj_t* clear_btn = nullptr;
    // 密码页（全屏，非弹框）：上 SSID+输入，下键盘
    lv_obj_t* pwd_page = nullptr;
    lv_obj_t* pwd_ssid_lbl = nullptr;
    lv_obj_t* pwd_box = nullptr;   // 输入框边框容器
    lv_obj_t* pwd_lbl = nullptr;   // 与拨号页一致：label 显示，避免 textarea 重
    lv_obj_t* pwd_keyboard = nullptr;
    lv_obj_t* status_overlay = nullptr;
    lv_obj_t* status_msg = nullptr;
};

UiState s_ui;
std::vector<ApItem> s_scan_results;
bool s_screen_active = false;
bool s_wifi_initialized = false;
bool s_wifi_station_was_active = false;
bool s_scan_in_progress = false;
bool s_connect_in_progress = false;
Tab s_tab = Tab::kNearby;
int s_nearby_page = 0;
int s_saved_page = 0;
int s_nearby_page_size = kPageSizeFallback;
int s_saved_page_size = kPageSizeFallback;
std::string s_pending_ssid;
wifi_auth_mode_t s_pending_authmode = WIFI_AUTH_OPEN;

// 自定义键盘：英（数字+字母）/ 符（全部符号）+ 大小写；6 列均分大方键
enum class KbMode : uint8_t { kEn = 0, kSym = 1 };
constexpr int kKbCols = 6;
constexpr int kKbRows = 7;  // 6 行字符 + 1 行功能（英需 10 数字 + 26 字母）
constexpr lv_coord_t kKbGap = 4;
KbMode s_kb_mode = KbMode::kEn;
bool s_kb_upper = false;
lv_obj_t* s_kb_keys[kKbRows * kKbCols] = {};
char s_password[kMaxPasswordLen + 1] = {};

esp_netif_t* s_netif = nullptr;
esp_event_handler_instance_t s_wifi_evt_inst = nullptr;
esp_event_handler_instance_t s_ip_evt_inst = nullptr;
EventGroupHandle_t s_evt_group = nullptr;
uint8_t s_last_disconnect_reason = 0;
lv_timer_t* s_restart_timer = nullptr;
lv_timer_t* s_fail_close_timer = nullptr;
int s_restart_remaining = 0;
std::string s_restart_headline;

void RebuildListPage();
void ScheduleScan();
void ScheduleConnect(const std::string& ssid, const std::string& password);
void OpenPasswordPage(const std::string& ssid, wifi_auth_mode_t authmode);
void ClosePasswordPage();
bool IsPasswordPageOpen();
void CloseStatusOverlay();
void OpenStatusOverlay(const char* msg);
void SetStatusOverlayText(const char* msg);
void OpenRestartCountdown(const std::string& headline);
void StyleTabBtn(lv_obj_t* btn, bool selected);
void StyleScanBtn(bool enabled);
const lv_font_t* UiFont();
const lv_font_t* KbFont();
void RebuildCustomKeyboard();
void LayoutCustomKeyboardSquares();

bool ScreenAlive() {
    return s_screen_active && s_ui.screen != nullptr && lv_obj_is_valid(s_ui.screen);
}

const lv_font_t* UiFont() {
    const lv_font_t* f = fontpack_lv_font_ui();
    return f;
}

/** 键盘用系统字库更大一号（30@2），失败回退 UI 25@2 */
const lv_font_t* KbFont() {
    const lv_font_t* f = fontpack_lv_font_get(30, 2);
    return f != nullptr ? f : UiFont();
}

bool IsPasswordPageOpen() {
    return s_ui.pwd_page != nullptr && !lv_obj_has_flag(s_ui.pwd_page, LV_OBJ_FLAG_HIDDEN);
}

const char* AuthLabel(wifi_auth_mode_t mode) {
    return (mode == WIFI_AUTH_OPEN) ? Lang::Strings::NETWORK_AUTH_OPEN : Lang::Strings::NETWORK_AUTH_SECURE;
}

const char* DisconnectReasonText(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_AUTH_LEAVE:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
        case WIFI_REASON_IE_IN_4WAY_DIFFERS:
        case WIFI_REASON_GROUP_CIPHER_INVALID:
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
        case WIFI_REASON_AKMP_INVALID:
        case WIFI_REASON_802_1X_AUTH_FAILED:
            return Lang::Strings::NETWORK_ERR_BAD_PASSWORD;
        case WIFI_REASON_NO_AP_FOUND:
            return Lang::Strings::NETWORK_ERR_AP_GONE;
        case WIFI_REASON_ASSOC_EXPIRE:
        case WIFI_REASON_ASSOC_TOOMANY:
        case WIFI_REASON_ASSOC_FAIL:
        case WIFI_REASON_NOT_ASSOCED:
            return Lang::Strings::NETWORK_ERR_ASSOC;
        case WIFI_REASON_BEACON_TIMEOUT:
            return Lang::Strings::NETWORK_ERR_WEAK;
        default:
            return nullptr;
    }
}

void Strip(lv_obj_t* obj) {
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_coord_t UiLineH() {
    const lv_font_t* font = fontpack_lv_font_ui();
    return font != nullptr ? static_cast<lv_coord_t>(font->line_height) : 28;
}

/** 附近列表按单行高度估条数，尽量填满刷新按钮上方后再换页。 */
lv_coord_t NearbyRowHEstimate() {
    return UiLineH() + kRowPadVer * 2 + 1;
}

/** 已保存行含操作按钮，按按钮高度估，避免一页挤爆。 */
lv_coord_t SavedRowHEstimate() {
    return std::max(NearbyRowHEstimate(), kSavedActionBtnH + kRowPadVer * 2 + 1);
}

int ComputeListPageSize(lv_coord_t row_h) {
    if (s_ui.list == nullptr) {
        return kPageSizeFallback;
    }
    if (s_ui.screen != nullptr) {
        lv_obj_update_layout(s_ui.screen);
    } else {
        lv_obj_update_layout(s_ui.list);
    }
    lv_coord_t h = lv_obj_get_content_height(s_ui.list);
    if (h <= 0) {
        h = lv_obj_get_height(s_ui.list);
    }
    // 行高略放大，吸收字高/底边框取整误差，避免末行压到刷新按钮
    const lv_coord_t fit_h = std::max<lv_coord_t>(1, row_h + 2);
    const int n = static_cast<int>(h / fit_h);
    return std::max(1, n);
}

void RefreshPageSizes() {
    s_nearby_page_size = ComputeListPageSize(NearbyRowHEstimate());
    s_saved_page_size = ComputeListPageSize(SavedRowHEstimate());
}

/** 名称可折两行：行高随内容，上下对称内边距避免贴分割线，同时保证单/两行垂直居中。 */
void StyleWifiNameRow(lv_obj_t* row, lv_obj_t* name) {
    const lv_coord_t line_h = UiLineH();
    lv_obj_set_style_pad_hor(row, 4, 0);
    lv_obj_set_style_pad_ver(row, kRowPadVer, 0);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_width(name, 0);
    lv_obj_set_flex_grow(name, 1);
    lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_max_height(name, line_h * 2, 0);
}

lv_obj_t* MakeBtn(lv_obj_t* parent, const char* text, lv_coord_t w, lv_coord_t h, lv_event_cb_t cb,
                  void* user_data = nullptr) {
    lv_obj_t* btn = lv_obj_create(parent);
    Strip(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(btn);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    return btn;
}

void StyleTabBtn(lv_obj_t* btn, bool selected) {
    if (btn == nullptr) {
        return;
    }
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    if (selected) {
        lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        if (lbl != nullptr) {
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        }
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        if (lbl != nullptr) {
            lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        }
    }
}

void StyleScanBtn(bool enabled) {
    if (s_ui.scan_btn == nullptr) {
        return;
    }
    if (enabled) {
        lv_obj_add_flag(s_ui.scan_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(s_ui.scan_btn, LV_OPA_COVER, 0);
    } else {
        lv_obj_clear_flag(s_ui.scan_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(s_ui.scan_btn, LV_OPA_50, 0);
    }
}

void SetStatus(const char* text) {
    if (s_ui.status_lbl != nullptr) {
        lv_label_set_text(s_ui.status_lbl, text != nullptr ? text : "");
    }
}

struct AsyncStatusMsg {
    char text[160];
};

void AsyncSetStatus(void* p) {
    auto* msg = static_cast<AsyncStatusMsg*>(p);
    if (ScreenAlive()) {
        SetStatus(msg->text);
    }
    delete msg;
}

void PostStatus(const char* text) {
    if (!s_screen_active) {
        return;
    }
    auto* msg = new AsyncStatusMsg{};
    snprintf(msg->text, sizeof(msg->text), "%s", text != nullptr ? text : "");
    if (lv_async_call(AsyncSetStatus, msg) != LV_RESULT_OK) {
        delete msg;
    }
}

void AsyncRebuildList(void* /*p*/) {
    if (ScreenAlive()) {
        RebuildListPage();
    }
}

void PostRebuildList() {
    if (!s_screen_active) {
        return;
    }
    // touch_feed / 扫网任务都可能调用：短持 adapter 锁只排队，真正 lv_obj_clean 在 LVGL 任务
    if (!ScreenLvAsyncUrgent(AsyncRebuildList)) {
        ESP_LOGW(TAG, "PostRebuildList async failed");
    }
}

void AsyncStyleScan(void* p) {
    const bool enabled = (reinterpret_cast<intptr_t>(p) != 0);
    if (ScreenAlive()) {
        StyleScanBtn(enabled);
    }
}

void PostScanBtnEnabled(bool enabled) {
    if (!s_screen_active) {
        return;
    }
    lv_async_call(AsyncStyleScan, reinterpret_cast<void*>(static_cast<intptr_t>(enabled ? 1 : 0)));
}

// ---- WiFi stack (owned while on this screen) ----

void WifiEvtHandler(void* /*arg*/, esp_event_base_t base, int32_t id, void* data) {
    if (s_evt_group == nullptr) {
        return;
    }
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_SCAN_DONE) {
            xEventGroupSetBits(s_evt_group, kBitScanDone);
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            auto* evt = static_cast<wifi_event_sta_disconnected_t*>(data);
            s_last_disconnect_reason = (evt != nullptr) ? evt->reason : 0;
            xEventGroupSetBits(s_evt_group, kBitDisconnected);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_evt_group, kBitConnected);
    }
}

bool WifiInitForScreen() {
    if (s_wifi_initialized) {
        return true;
    }

    wifi_mode_t mode_before = WIFI_MODE_NULL;
    const esp_err_t mode_err = esp_wifi_get_mode(&mode_before);
    s_wifi_station_was_active = (mode_err == ESP_OK && mode_before != WIFI_MODE_NULL);
    if (s_wifi_station_was_active) {
        // LpPaused 时栈仍 init，Stop 会 deinit；Resume 路径也走 Stop 更干净
        auto& sta = WifiStation::GetInstance();
        if (sta.IsLpPaused()) {
            (void)sta.ResumeFromLp();
        }
        sta.Stop();
    }

    if (s_evt_group == nullptr) {
        s_evt_group = xEventGroupCreate();
    } else {
        xEventGroupClearBits(s_evt_group, 0xFFFFFF);
    }

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

    s_netif = esp_netif_create_default_wifi_sta();
    if (s_netif == nullptr) {
        ESP_LOGE(TAG, "create sta netif failed");
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = false;
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(err));
        return false;
    }

    (void)esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiEvtHandler, nullptr,
                                              &s_wifi_evt_inst);
    (void)esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiEvtHandler, nullptr,
                                              &s_ip_evt_inst);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_initialized = true;
    ESP_LOGI(TAG, "local STA stack ready");
    return true;
}

void WifiTeardownForScreen() {
    if (!s_wifi_initialized) {
        return;
    }
    esp_wifi_scan_stop();
    esp_wifi_disconnect();
    if (s_wifi_evt_inst != nullptr) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_evt_inst);
        s_wifi_evt_inst = nullptr;
    }
    if (s_ip_evt_inst != nullptr) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_evt_inst);
        s_ip_evt_inst = nullptr;
    }
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_netif != nullptr) {
        esp_netif_destroy(s_netif);
        s_netif = nullptr;
    }
    s_wifi_initialized = false;
    if (s_wifi_station_was_active) {
        WifiStation::GetInstance().Start();
    }
    s_wifi_station_was_active = false;
    ESP_LOGI(TAG, "local STA stack torn down");
}

void ScanTask(void* /*arg*/) {
    s_scan_in_progress = true;
    PostScanBtnEnabled(false);
    PostStatus(Lang::Strings::NETWORK_SCANNING);
    PostRebuildList();

    if (!s_wifi_initialized) {
        PostStatus(Lang::Strings::NETWORK_WIFI_INIT);
        if (!WifiInitForScreen()) {
            PostStatus(Lang::Strings::NETWORK_WIFI_INIT_FAIL);
            s_scan_in_progress = false;
            PostRebuildList();
            PostScanBtnEnabled(true);
            vTaskDelete(nullptr);
            return;
        }
    }

    xEventGroupClearBits(s_evt_group, kBitScanDone);
    wifi_scan_config_t cfg = {};
    cfg.show_hidden = false;
    esp_err_t err = esp_wifi_scan_start(&cfg, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "scan_start: %s", esp_err_to_name(err));
        PostStatus(Lang::Strings::NETWORK_SCAN_FAIL);
        s_scan_in_progress = false;
        PostRebuildList();
        PostScanBtnEnabled(true);
        vTaskDelete(nullptr);
        return;
    }

    const EventBits_t bits =
        xEventGroupWaitBits(s_evt_group, kBitScanDone, pdTRUE, pdTRUE, pdMS_TO_TICKS(15000));
    if (!(bits & kBitScanDone)) {
        PostStatus(Lang::Strings::NETWORK_SCAN_TIMEOUT);
        s_scan_in_progress = false;
        PostRebuildList();
        PostScanBtnEnabled(true);
        vTaskDelete(nullptr);
        return;
    }

    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    std::vector<wifi_ap_record_t> records;
    if (ap_num > 0) {
        if (ap_num > 64) {
            ap_num = 64;
        }
        records.resize(ap_num);
        uint16_t got = ap_num;
        if (esp_wifi_scan_get_ap_records(&got, records.data()) == ESP_OK) {
            records.resize(got);
        } else {
            records.clear();
        }
    }

    std::sort(records.begin(), records.end(),
              [](const wifi_ap_record_t& a, const wifi_ap_record_t& b) { return a.rssi > b.rssi; });

    s_scan_results.clear();
    for (const auto& r : records) {
        char ssid[33];
        std::memcpy(ssid, r.ssid, 32);
        ssid[32] = '\0';
        if (ssid[0] == '\0') {
            continue;
        }
        bool dup = false;
        for (const auto& ex : s_scan_results) {
            if (ex.ssid == ssid) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }
        ApItem it;
        it.ssid = ssid;
        it.rssi = r.rssi;
        it.authmode = r.authmode;
        s_scan_results.push_back(std::move(it));
    }

    char buf[64];
    snprintf(buf, sizeof(buf), Lang::Strings::NETWORK_SCAN_DONE_FMT,
             static_cast<int>(s_scan_results.size()));
    PostStatus(buf);
    s_nearby_page = 0;
    s_scan_in_progress = false;
    PostRebuildList();
    PostScanBtnEnabled(true);
    vTaskDelete(nullptr);
}

void ScheduleScan() {
    if (s_scan_in_progress) {
        return;
    }
    if (s_connect_in_progress) {
        PostStatus(Lang::Strings::NETWORK_BUSY_CONNECT);
        return;
    }
    if (xTaskCreate(ScanTask, "net_scan", 4096, nullptr, 5, nullptr) != pdPASS) {
        PostStatus(Lang::Strings::NETWORK_SCAN_TASK_FAIL);
    }
}

struct ConnectCtx {
    std::string ssid;
    std::string password;
};

struct AsyncFailMsg {
    std::string title;
    std::string detail;
};

void AsyncShowFailure(void* p) {
    auto* msg = static_cast<AsyncFailMsg*>(p);
    if (ScreenAlive()) {
        char buf[192];
        snprintf(buf, sizeof(buf), "%s\n%s", msg->title.c_str(), msg->detail.c_str());
        OpenStatusOverlay(buf);
        if (s_fail_close_timer != nullptr) {
            lv_timer_delete(s_fail_close_timer);
            s_fail_close_timer = nullptr;
        }
        s_fail_close_timer = lv_timer_create(
            [](lv_timer_t* timer) {
                if (ScreenAlive()) {
                    CloseStatusOverlay();
                }
                s_fail_close_timer = nullptr;
                lv_timer_delete(timer);
            },
            2500, nullptr);
        lv_timer_set_repeat_count(s_fail_close_timer, 1);
    }
    delete msg;
}

void PostShowFailure(const std::string& title, const std::string& detail) {
    auto* msg = new AsyncFailMsg{title, detail};
    if (lv_async_call(AsyncShowFailure, msg) != LV_RESULT_OK) {
        delete msg;
    }
}

void AsyncOpenSuccess(void* p) {
    auto* ssid = static_cast<std::string*>(p);
    if (ScreenAlive()) {
        ClosePasswordPage();
        char headline[96];
        snprintf(headline, sizeof(headline), Lang::Strings::NETWORK_CONNECTED_FMT, ssid->c_str());
        OpenRestartCountdown(headline);
    }
    delete ssid;
}

void PostOpenSuccess(const std::string& ssid) {
    auto* msg = new std::string(ssid);
    if (lv_async_call(AsyncOpenSuccess, msg) != LV_RESULT_OK) {
        delete msg;
    }
}

void ConnectTask(void* arg) {
    auto* ctx = static_cast<ConnectCtx*>(arg);
    char buf[128];
    snprintf(buf, sizeof(buf), Lang::Strings::NETWORK_CONNECTING_FMT, ctx->ssid.c_str());
    PostStatus(buf);

    esp_wifi_scan_stop();
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));

    wifi_config_t wc = {};
    strlcpy(reinterpret_cast<char*>(wc.sta.ssid), ctx->ssid.c_str(), 32);
    strlcpy(reinterpret_cast<char*>(wc.sta.password), ctx->password.c_str(), 64);
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wc.sta.failure_retry_cnt = 1;

    xEventGroupClearBits(s_evt_group, kBitConnected | kBitDisconnected);
    s_last_disconnect_reason = 0;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wc);
    if (err != ESP_OK) {
        PostShowFailure(Lang::Strings::NETWORK_CONNECT_FAIL, esp_err_to_name(err));
        s_connect_in_progress = false;
        delete ctx;
        vTaskDelete(nullptr);
        return;
    }
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        PostShowFailure(Lang::Strings::NETWORK_CONNECT_FAIL, esp_err_to_name(err));
        s_connect_in_progress = false;
        delete ctx;
        vTaskDelete(nullptr);
        return;
    }

    const EventBits_t bits = xEventGroupWaitBits(s_evt_group, kBitConnected | kBitDisconnected, pdTRUE,
                                                 pdFALSE, pdMS_TO_TICKS(15000));
    if (bits & kBitConnected) {
        SsidManager::GetInstance().AddSsid(ctx->ssid, ctx->password);
        snprintf(buf, sizeof(buf), Lang::Strings::NETWORK_CONNECTED_FMT, ctx->ssid.c_str());
        PostStatus(buf);
        PostRebuildList();
        PostOpenSuccess(ctx->ssid);
    } else if (bits & kBitDisconnected) {
        const char* mapped = DisconnectReasonText(s_last_disconnect_reason);
        std::string detail;
        if (mapped != nullptr) {
            detail = mapped;
        } else {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), Lang::Strings::NETWORK_ERR_REASON_FMT, s_last_disconnect_reason);
            detail = tmp;
        }
        PostShowFailure(Lang::Strings::NETWORK_CONNECT_FAIL, detail);
    } else {
        esp_wifi_disconnect();
        PostShowFailure(Lang::Strings::NETWORK_CONNECT_TIMEOUT, Lang::Strings::NETWORK_CONNECT_TIMEOUT_HINT);
    }

    s_connect_in_progress = false;
    delete ctx;
    vTaskDelete(nullptr);
}

void ScheduleConnect(const std::string& ssid, const std::string& password) {
    if (s_connect_in_progress) {
        return;
    }
    if (ssid.empty() || ssid.size() > kMaxSsidLen) {
        PostStatus(Lang::Strings::NETWORK_SSID_INVALID);
        return;
    }
    if (password.size() > kMaxPasswordLen) {
        PostStatus(Lang::Strings::NETWORK_PWD_TOO_LONG);
        return;
    }
    if (!s_wifi_initialized && !WifiInitForScreen()) {
        PostStatus(Lang::Strings::NETWORK_WIFI_INIT_FAIL);
        return;
    }

    auto* ctx = new ConnectCtx{ssid, password};
    s_connect_in_progress = true;
    char buf[96];
    snprintf(buf, sizeof(buf), Lang::Strings::NETWORK_CONNECTING_FMT, ssid.c_str());
    OpenStatusOverlay(buf);

    if (xTaskCreate(ConnectTask, "net_conn", 4096, ctx, 5, nullptr) != pdPASS) {
        delete ctx;
        s_connect_in_progress = false;
        CloseStatusOverlay();
        PostStatus(Lang::Strings::NETWORK_CONNECT_TASK_FAIL);
    }
}

// ---- List UI ----

struct NearbyClickCtx {
    char ssid[kMaxSsidLen + 1];
    int authmode;
};

void OnNearbyItemClicked(lv_event_t* e) {
    auto* ctx = static_cast<NearbyClickCtx*>(lv_event_get_user_data(e));
    if (ctx == nullptr) {
        return;
    }
    const auto auth = static_cast<wifi_auth_mode_t>(ctx->authmode);
    if (auth == WIFI_AUTH_OPEN) {
        ScheduleConnect(ctx->ssid, "");
    } else {
        OpenPasswordPage(ctx->ssid, auth);
    }
}

void OnNearbyItemDelete(lv_event_t* e) {
    delete static_cast<NearbyClickCtx*>(lv_event_get_user_data(e));
}

struct SavedActionCtx {
    int index;
};

void OnSavedSetDefault(lv_event_t* e) {
    auto* ctx = static_cast<SavedActionCtx*>(lv_event_get_user_data(e));
    if (ctx == nullptr) {
        return;
    }
    SsidManager::GetInstance().SetDefaultSsid(ctx->index);
    s_saved_page = 0;
    RebuildListPage();
    SetStatus(Lang::Strings::NETWORK_SET_DEFAULT_OK);
}

void OnSavedRemove(lv_event_t* e) {
    auto* ctx = static_cast<SavedActionCtx*>(lv_event_get_user_data(e));
    if (ctx == nullptr) {
        return;
    }
    SsidManager::GetInstance().RemoveSsid(ctx->index);
    RebuildListPage();
    SetStatus(Lang::Strings::NETWORK_DELETED_OK);
}

void OnSavedBtnDelete(lv_event_t* e) {
    delete static_cast<SavedActionCtx*>(lv_event_get_user_data(e));
}

void OnClearSaved(lv_event_t* /*e*/) {
    SsidManager::GetInstance().Clear();
    s_saved_page = 0;
    RebuildListPage();
    SetStatus(Lang::Strings::NETWORK_CLEARED_OK);
}

int NearbyPageCount() {
    if (s_scan_results.empty()) {
        return 1;
    }
    const int ps = std::max(1, s_nearby_page_size);
    return static_cast<int>((s_scan_results.size() + ps - 1) / ps);
}

int SavedPageCount() {
    const size_t n = SsidManager::GetInstance().GetSsidList().size();
    if (n == 0) {
        return 1;
    }
    const int ps = std::max(1, s_saved_page_size);
    return static_cast<int>((n + ps - 1) / ps);
}

void RebuildNearbyPage() {
    if (s_ui.list == nullptr) {
        return;
    }
    lv_obj_clean(s_ui.list);

    if (s_ui.clear_btn != nullptr) {
        lv_obj_add_flag(s_ui.clear_btn, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_scan_in_progress) {
        lv_obj_t* hint = lv_label_create(s_ui.list);
        lv_label_set_text(hint, Lang::Strings::NETWORK_SCANNING);
        lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
        // 页码保持占位高度，避免底栏变矮后列表区虚高、末行压住刷新
        if (s_ui.page_lbl != nullptr) {
            lv_label_set_text(s_ui.page_lbl, "1 / 1");
        }
        return;
    }

    if (s_scan_results.empty()) {
        lv_obj_t* hint = lv_label_create(s_ui.list);
        lv_label_set_text(hint, Lang::Strings::NETWORK_NEARBY_EMPTY);
        lv_obj_set_width(hint, LV_PCT(100));
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
        if (s_ui.page_lbl != nullptr) {
            lv_label_set_text(s_ui.page_lbl, "1 / 1");
        }
        return;
    }

    const int pages = NearbyPageCount();
    if (s_nearby_page < 0) {
        s_nearby_page = 0;
    }
    if (s_nearby_page >= pages) {
        s_nearby_page = pages - 1;
    }
    if (s_ui.page_lbl != nullptr) {
        char foot[24];
        snprintf(foot, sizeof(foot), "%d / %d", s_nearby_page + 1, pages);
        lv_label_set_text(s_ui.page_lbl, foot);
    }

    const int ps = std::max(1, s_nearby_page_size);
    const int start = s_nearby_page * ps;
    const int end = std::min(start + ps, static_cast<int>(s_scan_results.size()));
    for (int i = start; i < end; ++i) {
        const auto& ap = s_scan_results[static_cast<size_t>(i)];
        lv_obj_t* row = lv_obj_create(s_ui.list);
        Strip(row);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_black(), 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(row);

        auto* ctx = new NearbyClickCtx{};
        strlcpy(ctx->ssid, ap.ssid.c_str(), sizeof(ctx->ssid));
        ctx->authmode = static_cast<int>(ap.authmode);
        lv_obj_add_event_cb(row, OnNearbyItemClicked, LV_EVENT_CLICKED, ctx);
        lv_obj_add_event_cb(row, OnNearbyItemDelete, LV_EVENT_DELETE, ctx);

        lv_obj_t* left = lv_label_create(row);
        char title[80];
        snprintf(title, sizeof(title), "%s %s", ap.ssid.c_str(), AuthLabel(ap.authmode));
        lv_label_set_text(left, title);
        lv_obj_set_style_text_font(left, fontpack_lv_font_ui(), 0);
        lv_obj_clear_flag(left, LV_OBJ_FLAG_CLICKABLE);
        StyleWifiNameRow(row, left);

        char meta[24];
        snprintf(meta, sizeof(meta), "%ddBm", ap.rssi);
        lv_obj_t* right = lv_label_create(row);
        lv_label_set_text(right, meta);
        lv_obj_set_style_text_font(right, fontpack_lv_font_ui(), 0);
        lv_obj_clear_flag(right, LV_OBJ_FLAG_CLICKABLE);
    }
}

void RebuildSavedPage() {
    if (s_ui.list == nullptr) {
        return;
    }
    lv_obj_clean(s_ui.list);

    const auto& list = SsidManager::GetInstance().GetSsidList();
    if (s_ui.clear_btn != nullptr) {
        if (list.empty()) {
            lv_obj_add_flag(s_ui.clear_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_ui.clear_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (list.empty()) {
        lv_obj_t* hint = lv_label_create(s_ui.list);
        lv_label_set_text(hint, Lang::Strings::NETWORK_SAVED_EMPTY);
        lv_obj_set_width(hint, LV_PCT(100));
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
        if (s_ui.page_lbl != nullptr) {
            lv_label_set_text(s_ui.page_lbl, "1 / 1");
        }
        return;
    }

    const int pages = SavedPageCount();
    if (s_saved_page < 0) {
        s_saved_page = 0;
    }
    if (s_saved_page >= pages) {
        s_saved_page = pages - 1;
    }
    if (s_ui.page_lbl != nullptr) {
        char foot[24];
        snprintf(foot, sizeof(foot), "%d / %d", s_saved_page + 1, pages);
        lv_label_set_text(s_ui.page_lbl, foot);
    }

    const int ps = std::max(1, s_saved_page_size);
    const int start = s_saved_page * ps;
    const int end = std::min(start + ps, static_cast<int>(list.size()));
    for (int i = start; i < end; ++i) {
        const auto& item = list[static_cast<size_t>(i)];
        lv_obj_t* row = lv_obj_create(s_ui.list);
        Strip(row);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_black(), 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 6, 0);

        lv_obj_t* name = lv_label_create(row);
        char ttext[96];
        if (i == 0) {
            snprintf(ttext, sizeof(ttext), Lang::Strings::NETWORK_DEFAULT_FMT, item.ssid.c_str());
        } else {
            snprintf(ttext, sizeof(ttext), "%s", item.ssid.c_str());
        }
        lv_label_set_text(name, ttext);
        lv_obj_set_style_text_font(name, fontpack_lv_font_ui(), 0);
        StyleWifiNameRow(row, name);

        if (i != 0) {
            auto* def_ctx = new SavedActionCtx{i};
            lv_obj_t* def_btn = MakeBtn(row, Lang::Strings::NETWORK_SET_DEFAULT, 88, kSavedActionBtnH,
                                        OnSavedSetDefault, def_ctx);
            lv_obj_add_event_cb(def_btn, OnSavedBtnDelete, LV_EVENT_DELETE, def_ctx);
        }

        auto* del_ctx = new SavedActionCtx{i};
        lv_obj_t* del_btn =
            MakeBtn(row, Lang::Strings::NETWORK_DELETE, 72, kSavedActionBtnH, OnSavedRemove, del_ctx);
        lv_obj_add_event_cb(del_btn, OnSavedBtnDelete, LV_EVENT_DELETE, del_ctx);
    }
}

void RebuildListPage() {
    if (s_tab == Tab::kNearby) {
        RebuildNearbyPage();
    } else {
        RebuildSavedPage();
    }
}

void SwitchTab(Tab tab) {
    if (s_tab == tab) {
        return;
    }
    s_tab = tab;
    StyleTabBtn(s_ui.nearby_tab_btn, s_tab == Tab::kNearby);
    StyleTabBtn(s_ui.saved_tab_btn, s_tab == Tab::kSaved);
    if (s_ui.scan_btn != nullptr) {
        if (s_tab == Tab::kNearby) {
            lv_obj_clear_flag(s_ui.scan_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ui.scan_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    RebuildListPage();
}

void OnNearbyTab(lv_event_t* /*e*/) { SwitchTab(Tab::kNearby); }
void OnSavedTab(lv_event_t* /*e*/) { SwitchTab(Tab::kSaved); }
void OnScanClicked(lv_event_t* /*e*/) { ScheduleScan(); }

// ---- Password page（全屏）+ 自定义方键键盘 ----

enum class KbAction : uint8_t {
    kChar = 0,
    kBackspace,
    kShift,
    kModeEn,
    kModeSym,
    kSpace,
    kEmpty,
};

struct KbKeyDesc {
    const char* label;  // 显示
    const char* insert; // 插入文本；功能键可为 nullptr
    KbAction action;
};

void RefreshPasswordDisplay() {
    if (s_ui.pwd_lbl == nullptr) {
        return;
    }
    if (s_password[0] == '\0') {
        lv_label_set_text(s_ui.pwd_lbl, Lang::Strings::NETWORK_PWD_PLACEHOLDER);
        lv_obj_set_style_text_opa(s_ui.pwd_lbl, LV_OPA_50, 0);
    } else {
        lv_label_set_text(s_ui.pwd_lbl, s_password);
        lv_obj_set_style_text_opa(s_ui.pwd_lbl, LV_OPA_COVER, 0);
    }
}

void AppendToPassword(const char* text) {
    if (s_ui.pwd_lbl == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }
    const size_t cur = std::strlen(s_password);
    const size_t add = std::strlen(text);
    if (add == 0 || cur + add > kMaxPasswordLen) {
        return;
    }
    std::memcpy(s_password + cur, text, add + 1);
    RefreshPasswordDisplay();
}

void BackspacePassword() {
    size_t len = std::strlen(s_password);
    if (len == 0) {
        return;
    }
    // UTF-8：退掉末尾完整码点（中文标点等）
    do {
        --len;
    } while (len > 0 && (static_cast<unsigned char>(s_password[len]) & 0xC0) == 0x80);
    s_password[len] = '\0';
    RefreshPasswordDisplay();
}

void ClearPassword() {
    s_password[0] = '\0';
    RefreshPasswordDisplay();
}

void OnBackspaceLongPressed(lv_event_t* /*e*/) {
    ClearPassword();
}

void FillKbLayout(KbKeyDesc out[kKbRows * kKbCols]) {
    for (int i = 0; i < kKbRows * kKbCols; ++i) {
        out[i] = {"", nullptr, KbAction::kEmpty};
    }

    auto put = [&](int idx, const char* label, const char* insert, KbAction act) {
        if (idx < 0 || idx >= kKbRows * kKbCols) {
            return;
        }
        out[idx] = {label, insert, act};
    };

    constexpr int kCharSlots = 6 * kKbCols;  // 前 6 行

    if (s_kb_mode == KbMode::kEn) {
        // 数字 + 字母 a–z（共 36 格）
        static char bufs[kCharSlots][2];
        int n = 0;
        for (char c = '1'; c <= '9' && n < kCharSlots; ++c, ++n) {
            bufs[n][0] = c;
            bufs[n][1] = '\0';
            put(n, bufs[n], bufs[n], KbAction::kChar);
        }
        if (n < kCharSlots) {
            bufs[n][0] = '0';
            bufs[n][1] = '\0';
            put(n, bufs[n], bufs[n], KbAction::kChar);
            ++n;
        }
        for (char c = 'a'; c <= 'z' && n < kCharSlots; ++c, ++n) {
            bufs[n][0] = s_kb_upper ? static_cast<char>(c - 'a' + 'A') : c;
            bufs[n][1] = '\0';
            put(n, bufs[n], bufs[n], KbAction::kChar);
        }
    } else {
        // 符：ASCII + 中文标点合并
        static const char* const sym[] = {
            "!", "@", "#", "$", "%", "^",
            "&", "*", "(", ")", "-", "_",
            "=", "+", "[", "]", "{", "}",
            "\\", "|", ";", ":", "'", "\"",
            ",", ".", "<", ">", "/", "?",
            "`", "~", "，", "。", "？", "！",
            "、", "；", "：", "…", "“", "”",
            "‘", "’", "（", "）", "【", "】",
            "《", "》", "—", "·", "￥", "～",
        };
        const int nsym = static_cast<int>(sizeof(sym) / sizeof(sym[0]));
        for (int i = 0; i < nsym && i < kCharSlots; ++i) {
            put(i, sym[i], sym[i], KbAction::kChar);
        }
    }

    // 底行：英 符 大小写 空格 空格 删
    const int base = 6 * kKbCols;
    put(base + 0, "英", nullptr, KbAction::kModeEn);
    put(base + 1, "符", nullptr, KbAction::kModeSym);
    put(base + 2, s_kb_upper ? "大" : "小", nullptr, KbAction::kShift);
    put(base + 3, "␣", " ", KbAction::kSpace);
    put(base + 4, "␣", " ", KbAction::kSpace);
    put(base + 5, "删", nullptr, KbAction::kBackspace);
}

/** 字符/空格/退格：按下即写入（跟盖板键「按下即 Click」、拨号盘跟手一致）。 */
void OnCustomKeyPressed(lv_event_t* e) {
    const auto action = static_cast<KbAction>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_obj_t* lbl = (btn != nullptr) ? lv_obj_get_child(btn, 0) : nullptr;
    const char* label = (lbl != nullptr) ? lv_label_get_text(lbl) : nullptr;

    switch (action) {
        case KbAction::kBackspace:
            BackspacePassword();
            break;
        case KbAction::kSpace:
            AppendToPassword(" ");
            break;
        case KbAction::kChar:
            if (label != nullptr) {
                AppendToPassword(label);
            }
            break;
        case KbAction::kShift:
        case KbAction::kModeEn:
        case KbAction::kModeSym:
        case KbAction::kEmpty:
        default:
            break;
    }
}

/** 切布局：松手再建盘，避免 PRESSED 里 lv_obj_clean 删掉当前按键。 */
void OnCustomKeyClicked(lv_event_t* e) {
    const auto action = static_cast<KbAction>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));

    switch (action) {
        case KbAction::kShift:
            s_kb_upper = !s_kb_upper;
            RebuildCustomKeyboard();
            break;
        case KbAction::kModeEn:
            s_kb_mode = KbMode::kEn;
            RebuildCustomKeyboard();
            break;
        case KbAction::kModeSym:
            s_kb_mode = KbMode::kSym;
            RebuildCustomKeyboard();
            break;
        default:
            break;
    }
}

void StyleKbKey(lv_obj_t* btn, lv_obj_t* lbl, KbAction action, bool active_mode) {
    const bool highlight = active_mode || action == KbAction::kShift;
    if (highlight && (action == KbAction::kModeEn || action == KbAction::kModeSym ||
                      (action == KbAction::kShift && s_kb_upper))) {
        lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    }
}

void LayoutCustomKeyboardSquares() {
    if (s_ui.pwd_keyboard == nullptr) {
        return;
    }
    const lv_coord_t w = lv_obj_get_content_width(s_ui.pwd_keyboard);
    const lv_coord_t h = lv_obj_get_content_height(s_ui.pwd_keyboard);
    if (w <= 0 || h <= 0) {
        return;
    }
    const lv_coord_t cell_w = (w - kKbGap * (kKbCols - 1)) / kKbCols;
    const lv_coord_t cell_h = (h - kKbGap * (kKbRows - 1)) / kKbRows;
    const lv_coord_t side = (cell_w < cell_h) ? cell_w : cell_h;
    if (side <= 0) {
        return;
    }
    const lv_coord_t total_w = side * kKbCols + kKbGap * (kKbCols - 1);
    const lv_coord_t total_h = side * kKbRows + kKbGap * (kKbRows - 1);
    const lv_coord_t ox = (w - total_w) / 2;
    const lv_coord_t oy = (h - total_h) / 2;

    for (int r = 0; r < kKbRows; ++r) {
        for (int c = 0; c < kKbCols; ++c) {
            lv_obj_t* key = s_kb_keys[r * kKbCols + c];
            if (key == nullptr) {
                continue;
            }
            lv_obj_set_size(key, side, side);
            lv_obj_set_pos(key, ox + c * (side + kKbGap), oy + r * (side + kKbGap));
        }
    }
}

void OnKbAreaSizeChanged(lv_event_t* /*e*/) {
    LayoutCustomKeyboardSquares();
}

void RebuildCustomKeyboard() {
    if (s_ui.pwd_keyboard == nullptr) {
        return;
    }
    lv_obj_clean(s_ui.pwd_keyboard);
    for (int i = 0; i < kKbRows * kKbCols; ++i) {
        s_kb_keys[i] = nullptr;
    }

    KbKeyDesc layout[kKbRows * kKbCols];
    FillKbLayout(layout);

    const lv_font_t* font = KbFont();
    for (int i = 0; i < kKbRows * kKbCols; ++i) {
        const KbKeyDesc& d = layout[i];
        lv_obj_t* btn = lv_obj_create(s_ui.pwd_keyboard);
        Strip(btn);
        s_kb_keys[i] = btn;
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, lv_color_black(), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        void* ud = reinterpret_cast<void*>(static_cast<uintptr_t>(d.action));
        // 输入键按下即上屏；切 英/符/大小写 仍 CLICKED，防止重建时销毁按中的键
        if (d.action == KbAction::kChar || d.action == KbAction::kSpace ||
            d.action == KbAction::kBackspace) {
            lv_obj_add_event_cb(btn, OnCustomKeyPressed, LV_EVENT_PRESSED, ud);
        } else if (d.action == KbAction::kShift || d.action == KbAction::kModeEn ||
                   d.action == KbAction::kModeSym) {
            lv_obj_add_event_cb(btn, OnCustomKeyClicked, LV_EVENT_CLICKED, ud);
        }
        if (d.action == KbAction::kBackspace) {
            // 与电话拨号页一致：短按删一字，长按清空
            lv_obj_add_event_cb(btn, OnBackspaceLongPressed, LV_EVENT_LONG_PRESSED, nullptr);
        }

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, d.label != nullptr ? d.label : "");
        if (font != nullptr) {
            lv_obj_set_style_text_font(lbl, font, 0);
        }
        lv_obj_center(lbl);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

        const bool mode_on =
            (d.action == KbAction::kModeEn && s_kb_mode == KbMode::kEn) ||
            (d.action == KbAction::kModeSym && s_kb_mode == KbMode::kSym);
        StyleKbKey(btn, lbl, d.action, mode_on);
    }
    LayoutCustomKeyboardSquares();
}

void OnPwdConnect(lv_event_t* /*e*/) {
    ScheduleConnect(s_pending_ssid, s_password);
}

void OnPwdCancel(lv_event_t* /*e*/) { ClosePasswordPage(); }

void ClosePasswordPage() {
    if (s_ui.pwd_page != nullptr) {
        lv_obj_add_flag(s_ui.pwd_page, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.list_panel != nullptr) {
        lv_obj_clear_flag(s_ui.list_panel, LV_OBJ_FLAG_HIDDEN);
    }
    ClearPassword();
    s_kb_mode = KbMode::kEn;
    s_kb_upper = false;
}

void BuildPasswordPage(lv_obj_t* parent, lv_coord_t top_y) {
    lv_obj_t* page = lv_obj_create(parent);
    Strip(page);
    s_ui.pwd_page = page;
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES - top_y);
    lv_obj_align(page, LV_ALIGN_TOP_MID, 0, top_y);
    lv_obj_set_style_bg_color(page, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(page, 12, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(page, 10, 0);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);

    const lv_font_t* ui = UiFont();

    lv_obj_t* top = lv_obj_create(page);
    Strip(top);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_height(top, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(top, 10, 0);

    s_ui.pwd_ssid_lbl = lv_label_create(top);
    lv_label_set_text(s_ui.pwd_ssid_lbl, "");
    lv_obj_set_width(s_ui.pwd_ssid_lbl, LV_PCT(100));
    lv_label_set_long_mode(s_ui.pwd_ssid_lbl, LV_LABEL_LONG_DOT);
    if (ui != nullptr) {
        lv_obj_set_style_text_font(s_ui.pwd_ssid_lbl, ui, 0);
    }

    lv_obj_t* hint = lv_label_create(top);
    lv_label_set_text(hint, Lang::Strings::NETWORK_PWD_HINT);
    if (ui != nullptr) {
        lv_obj_set_style_text_font(hint, ui, 0);
    }

    // 与拨号号码行一致：label + 缓冲，按下即可刷字，无 textarea 光标/布局开销
    lv_obj_t* box = lv_obj_create(top);
    Strip(box);
    s_ui.pwd_box = box;
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_height(box, 56);
    lv_obj_set_style_bg_color(box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_black(), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_pad_hor(box, 10, 0);
    lv_obj_set_style_pad_ver(box, 10, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_ui.pwd_lbl = lv_label_create(box);
    lv_obj_set_width(s_ui.pwd_lbl, LV_PCT(100));
    // 墨水屏勿用滚动动画（会连刷）；超长直接裁切
    lv_label_set_long_mode(s_ui.pwd_lbl, LV_LABEL_LONG_CLIP);
    if (ui != nullptr) {
        lv_obj_set_style_text_font(s_ui.pwd_lbl, ui, 0);
    }
    lv_obj_set_style_text_color(s_ui.pwd_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.pwd_lbl, LV_OBJ_FLAG_CLICKABLE);
    s_password[0] = '\0';
    RefreshPasswordDisplay();

    lv_obj_t* row = lv_obj_create(top);
    Strip(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 48);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    MakeBtn(row, Lang::Strings::NETWORK_CANCEL, 140, 44, OnPwdCancel);
    MakeBtn(row, Lang::Strings::NETWORK_CONNECT, 140, 44, OnPwdConnect);

    // 自定义键盘区域：6×6 均分大方键
    lv_obj_t* kb = lv_obj_create(page);
    Strip(kb);
    s_ui.pwd_keyboard = kb;
    lv_obj_set_width(kb, LV_PCT(100));
    lv_obj_set_flex_grow(kb, 1);
    lv_obj_set_style_bg_color(kb, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(kb, OnKbAreaSizeChanged, LV_EVENT_SIZE_CHANGED, nullptr);

    s_kb_mode = KbMode::kEn;
    s_kb_upper = false;
    RebuildCustomKeyboard();
}

void OpenPasswordPage(const std::string& ssid, wifi_auth_mode_t authmode) {
    if (s_ui.pwd_page == nullptr || s_ui.pwd_ssid_lbl == nullptr) {
        return;
    }
    s_pending_ssid = ssid;
    s_pending_authmode = authmode;
    s_kb_mode = KbMode::kEn;
    s_kb_upper = false;

    char ttext[96];
    snprintf(ttext, sizeof(ttext), Lang::Strings::NETWORK_CONNECT_TO_FMT, ssid.c_str());
    lv_label_set_text(s_ui.pwd_ssid_lbl, ttext);

    ClearPassword();

    RebuildCustomKeyboard();

    if (s_ui.list_panel != nullptr) {
        lv_obj_add_flag(s_ui.list_panel, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(s_ui.pwd_page, LV_OBJ_FLAG_HIDDEN);
    // 显示后再按实际尺寸均分方键
    LayoutCustomKeyboardSquares();
}

void CloseStatusOverlay() {
    if (s_ui.status_overlay != nullptr && lv_obj_is_valid(s_ui.status_overlay)) {
        lv_obj_delete(s_ui.status_overlay);
    }
    s_ui.status_overlay = nullptr;
    s_ui.status_msg = nullptr;
}

void OpenStatusOverlay(const char* msg) {
    if (s_ui.screen == nullptr) {
        return;
    }
    CloseStatusOverlay();
    lv_obj_t* mask = lv_obj_create(s_ui.screen);
    Strip(mask);
    lv_obj_set_size(mask, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(mask, 0, 0);
    ScreenApplyDotBackdrop(mask);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    s_ui.status_overlay = mask;

    lv_obj_t* card = lv_obj_create(mask);
    Strip(card);
    lv_obj_set_size(card, LV_HOR_RES - 48, 160);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 16, 0);

    lv_obj_t* lbl = lv_label_create(card);
    s_ui.status_msg = lbl;
    lv_label_set_text(lbl, msg != nullptr ? msg : "");
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
}

void SetStatusOverlayText(const char* msg) {
    if (s_ui.status_msg != nullptr) {
        lv_label_set_text(s_ui.status_msg, msg != nullptr ? msg : "");
    }
}

void RebootTask(void* /*arg*/) {
    vTaskDelay(pdMS_TO_TICKS(200));
    Application::GetInstance().Reboot();
    vTaskDelete(nullptr);
}

void RestartTimerCb(lv_timer_t* /*timer*/) {
    s_restart_remaining--;
    if (s_restart_remaining > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_NET_REBOOT_COUNT_FMT, s_restart_headline.c_str(),
                 s_restart_remaining);
        SetStatusOverlayText(buf);
        return;
    }
    if (s_restart_timer != nullptr) {
        lv_timer_delete(s_restart_timer);
        s_restart_timer = nullptr;
    }
    SetStatusOverlayText(Lang::Strings::SETTINGS_NET_REBOOTING);
    xTaskCreate(RebootTask, "net_reboot", 2048, nullptr, 5, nullptr);
}

void OpenRestartCountdown(const std::string& headline) {
    s_restart_headline = headline;
    s_restart_remaining = kRestartCountdownSec;
    char buf[128];
    snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_NET_REBOOT_COUNT_FMT, s_restart_headline.c_str(),
             s_restart_remaining);
    OpenStatusOverlay(buf);
    if (s_restart_timer != nullptr) {
        lv_timer_delete(s_restart_timer);
    }
    s_restart_timer = lv_timer_create(RestartTimerCb, 1000, nullptr);
}

void OnScreenUnload(lv_event_t* /*e*/) {
    ESP_LOGI(TAG, "unload");
    s_screen_active = false;
    if (s_restart_timer != nullptr) {
        lv_timer_delete(s_restart_timer);
        s_restart_timer = nullptr;
    }
    if (s_fail_close_timer != nullptr) {
        lv_timer_delete(s_fail_close_timer);
        s_fail_close_timer = nullptr;
    }
    ClosePasswordPage();
    CloseStatusOverlay();
    WifiTeardownForScreen();
    for (int i = 0; i < kKbRows * kKbCols; ++i) {
        s_kb_keys[i] = nullptr;
    }
    s_ui = {};
    s_scan_results.clear();
    s_scan_in_progress = false;
    s_connect_in_progress = false;
}

}  // namespace

lv_obj_t* NetworkScreen::Create() {
    s_screen_active = true;
    s_scan_in_progress = false;
    s_connect_in_progress = false;
    s_scan_results.clear();
    s_tab = Tab::kNearby;
    s_nearby_page = 0;
    s_saved_page = 0;
    s_nearby_page_size = kPageSizeFallback;
    s_saved_page_size = kPageSizeFallback;
    s_ui = {};

    lv_obj_t* scr = lv_obj_create(nullptr);
    s_ui.screen = scr;
    Strip(scr);
    lv_obj_set_size(scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(scr, OnScreenUnload, LV_EVENT_SCREEN_UNLOADED, nullptr);

    auto status = ScreenCreateStatusBar(scr);

    lv_obj_t* body = lv_obj_create(scr);
    Strip(body);
    s_ui.list_panel = body;
    lv_obj_set_size(body, LV_HOR_RES, LV_VER_RES - status.height);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_pad_all(body, 12, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, 8, 0);

    lv_obj_t* title = lv_label_create(body);
    lv_label_set_text(title, Lang::Strings::NETWORK_TITLE);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);

    s_ui.status_lbl = lv_label_create(body);
    lv_label_set_text(s_ui.status_lbl, "");
    lv_obj_set_width(s_ui.status_lbl, LV_PCT(100));
    lv_label_set_long_mode(s_ui.status_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(s_ui.status_lbl, fontpack_lv_font_ui(), 0);

    lv_obj_t* tab_row = lv_obj_create(body);
    Strip(tab_row);
    lv_obj_set_width(tab_row, LV_PCT(100));
    lv_obj_set_height(tab_row, 48);
    lv_obj_set_flex_flow(tab_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(tab_row, 8, 0);

    s_ui.nearby_tab_btn = MakeBtn(tab_row, Lang::Strings::NETWORK_TAB_NEARBY, 120, 44, OnNearbyTab);
    s_ui.saved_tab_btn = MakeBtn(tab_row, Lang::Strings::NETWORK_TAB_SAVED, 120, 44, OnSavedTab);
    StyleTabBtn(s_ui.nearby_tab_btn, true);
    StyleTabBtn(s_ui.saved_tab_btn, false);

    s_ui.list = lv_obj_create(body);
    Strip(s_ui.list);
    lv_obj_set_width(s_ui.list, LV_PCT(100));
    lv_obj_set_flex_grow(s_ui.list, 1);
    lv_obj_set_flex_flow(s_ui.list, LV_FLEX_FLOW_COLUMN);

    // 底栏纵向：刷新/清空居中在上，页码居中在下
    lv_obj_t* foot = lv_obj_create(body);
    Strip(foot);
    lv_obj_set_width(foot, LV_PCT(100));
    lv_obj_set_height(foot, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(foot, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(foot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(foot, 6, 0);

    lv_obj_t* action_row = lv_obj_create(foot);
    Strip(action_row);
    lv_obj_set_width(action_row, LV_PCT(100));
    lv_obj_set_height(action_row, 44);
    lv_obj_set_flex_flow(action_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(action_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_ui.scan_btn = MakeBtn(action_row, Lang::Strings::NETWORK_SCAN, 100, 40, OnScanClicked);
    s_ui.clear_btn = MakeBtn(action_row, Lang::Strings::NETWORK_CLEAR_ALL, 100, 40, OnClearSaved);
    lv_obj_add_flag(s_ui.clear_btn, LV_OBJ_FLAG_HIDDEN);

    s_ui.page_lbl = lv_label_create(foot);
    // 先占位再量列表高：空字符串会让底栏偏矮，多算出一条压住刷新按钮
    lv_label_set_text(s_ui.page_lbl, "1 / 1");
    lv_obj_set_width(s_ui.page_lbl, LV_PCT(100));
    lv_obj_set_style_text_align(s_ui.page_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_ui.page_lbl, fontpack_lv_font_ui(), 0);

    BuildPasswordPage(scr, status.height);

    RefreshPageSizes();
    RebuildListPage();

    ScreenSetIsHome(false);
    VkKey_AttachScreen(scr, kScreenId, VkKeyScreenDesc{NetworkScreen::Create, NetworkScreen::OnVkKey});

    // 进入后自动扫一次
    ScheduleScan();
    return scr;
}

bool NetworkScreen::OnVkKey(const char* key_name) {
    if (key_name == nullptr || !ScreenAlive()) {
        return false;
    }
    // 盖板键在 touch_feed 任务回调，禁止同步 lv_obj_clean / delete（会与 LVGL 线程抢 event 链崩溃）
    if (s_ui.status_overlay != nullptr) {
        if (std::strcmp(key_name, "vk_prev") == 0 || std::strcmp(key_name, "vk_home") == 0) {
            if (s_restart_timer != nullptr) {
                return true;  // 倒计时重启中禁止退出
            }
            ScreenLvAsync([](void*) {
                if (ScreenAlive()) {
                    CloseStatusOverlay();
                }
            });
            return true;
        }
        return true;
    }
    if (IsPasswordPageOpen()) {
        if (std::strcmp(key_name, "vk_prev") == 0 || std::strcmp(key_name, "vk_home") == 0) {
            ScreenLvAsync([](void*) {
                if (ScreenAlive()) {
                    ClosePasswordPage();
                }
            });
            return true;
        }
        return true;  // 密码页吞掉翻页键
    }
    if (std::strcmp(key_name, "vk_prev") == 0) {
        if (s_tab == Tab::kNearby) {
            if (s_nearby_page > 0) {
                --s_nearby_page;
                PostRebuildList();
                return true;
            }
        } else if (s_saved_page > 0) {
            --s_saved_page;
            PostRebuildList();
            return true;
        }
        return false;  // 首页交给默认返回
    }
    if (std::strcmp(key_name, "vk_next") == 0) {
        if (s_tab == Tab::kNearby) {
            if (s_nearby_page + 1 < NearbyPageCount()) {
                ++s_nearby_page;
                PostRebuildList();
            }
        } else if (s_saved_page + 1 < SavedPageCount()) {
            ++s_saved_page;
            PostRebuildList();
        }
        return true;
    }
    return false;
}
