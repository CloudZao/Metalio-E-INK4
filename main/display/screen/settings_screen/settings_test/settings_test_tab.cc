#include "settings_test_tab.h"

#include "settings_test_aging_screen.h"
#include "settings_test_auto_screen.h"
#include "settings_test_battery_screen.h"
#include "settings_test_common.h"
#include "settings_test_touch_screen.h"
#include "settings_common.h"

#include "assets/lang_config.h"
#include "fontpack_lvgl.h"
#include "screen_common.h"
#include "wifi_station.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>

#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr const char* TAG = "SettingsTestTab";
/** 产测入口：信号刷新周期。4G 走 AT+CSQ，不宜过密以免与其它 AT 抢 modem mutex。 */
constexpr uint32_t kSignalPollMs = 2000;
constexpr lv_coord_t kSignalPanelH = 56;
constexpr lv_coord_t kSignalTitleW = 120;
constexpr size_t kSignalTextMax = 48;

/**
 * 测试 Tab 信号条：与自动测试页 SettingsTestUi 生命周期解耦。
 *
 * 链路：
 *   lv_timer（LVGL）→ 若空闲则 xTaskCreate 采样 → ESP_LOGI 串口
 *                    → SettingsTest_PostLvAsync 回写 label（仅文案变化时 set_text，减墨水屏局刷）
 *
 * 约束：
 * - 禁止在 LVGL 任务里调 Nt26Board::GetSignalStrength（内部 AT+CSQ 可阻塞数百 ms）
 * - 离 Tab / Reset 先停表、抬 generation、清 alive，丢弃迟到的 async，避免写已删控件
 * - sample_busy 保证同时最多一个采样任务，失败路径必须清 busy，防卡死
 */
struct SignalUiMsg {
    uint32_t generation = 0;
    char text[kSignalTextMax] = {};
};

lv_obj_t* s_signal_value = nullptr;
lv_timer_t* s_signal_timer = nullptr;
std::atomic<bool> s_signal_alive{false};
std::atomic<uint32_t> s_signal_generation{0};
std::atomic<bool> s_signal_busy{false};
char s_last_signal_text[kSignalTextMax] = {};

void OnAutoTestClicked(lv_event_t* /*e*/) {
    ESP_LOGI(TAG, "open auto test screen");
    ScreenNavigateTo(SettingsTestAutoScreen::Create);
}

void OnTouchTestClicked(lv_event_t* /*e*/) {
    ESP_LOGI(TAG, "open touch test screen");
    ScreenNavigateTo(SettingsTestTouchScreen::Create);
}

void OnBatteryTestClicked(lv_event_t* /*e*/) {
    ESP_LOGI(TAG, "open battery test screen");
    ScreenNavigateTo(SettingsTestBatteryScreen::Create);
}

void OnAgingTestClicked(lv_event_t* /*e*/) {
    ESP_LOGI(TAG, "open aging test screen");
    ScreenNavigateTo(SettingsTestAgingScreen::Create);
}

void StopSignalTimer() {
    if (s_signal_timer != nullptr) {
        lv_timer_delete(s_signal_timer);
        s_signal_timer = nullptr;
    }
}

void InvalidateSignalSession() {
    s_signal_alive.store(false, std::memory_order_release);
    s_signal_generation.fetch_add(1, std::memory_order_acq_rel);
    StopSignalTimer();
    s_signal_value = nullptr;
    s_last_signal_text[0] = '\0';
}

void FormatWifiSignal(char* ui, size_t ui_len, char* serial, size_t serial_len) {
    auto& wifi = WifiStation::GetInstance();
    if (!wifi.IsConnected()) {
        std::snprintf(ui, ui_len, "%s", Lang::Strings::SETTINGS_TEST_SIGNAL_WIFI_DISCONNECTED);
        std::snprintf(serial, serial_len, "@@@信号  | 网络: WiFi | 未连接");
        return;
    }

    // 勿用 WifiStation::GetRssi()：其内部 ESP_ERROR_CHECK，断连竞态会 abort。
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        std::snprintf(ui, ui_len, "%s", Lang::Strings::SETTINGS_TEST_SIGNAL_WIFI_READ_FAIL);
        std::snprintf(serial, serial_len, "@@@信号  | 网络: WiFi | RSSI: 读取失败");
        return;
    }

    const int rssi = static_cast<int>(ap.rssi);
    std::snprintf(ui, ui_len, Lang::Strings::SETTINGS_TEST_SIGNAL_WIFI_RSSI_FMT, rssi);
    std::snprintf(serial, serial_len, "@@@信号  | 网络: WiFi | RSSI: %d dBm", rssi);
}

void FormatCellSignal(char* ui, size_t ui_len, char* serial, size_t serial_len) {
    Nt26Board* nt26 = SettingsTest_GetNt26Board();
    if (nt26 == nullptr) {
        std::snprintf(ui, ui_len, "%s", Lang::Strings::SETTINGS_TEST_SIGNAL_CELL_UNAVAIL);
        std::snprintf(serial, serial_len, "@@@信号  | 网络: 4G | 模组不可用");
        return;
    }

    // 可能阻塞 AT；仅允许在采样任务线程调用。
    const int csq = nt26->GetSignalStrength();
    if (csq == 99 || csq < 0) {
        std::snprintf(ui, ui_len, "%s", Lang::Strings::SETTINGS_TEST_SIGNAL_CELL_CSQ_UNKNOWN);
        std::snprintf(serial, serial_len, "@@@信号  | 网络: 4G | CSQ: 未知");
        return;
    }

    std::snprintf(ui, ui_len, Lang::Strings::SETTINGS_TEST_SIGNAL_CELL_CSQ_FMT, csq);
    std::snprintf(serial, serial_len, "@@@信号  | 网络: 4G | CSQ: %2d", csq);
}

void SampleSignalText(char* ui, size_t ui_len, char* serial, size_t serial_len) {
    DualNetworkBoard* dual = SettingsTest_GetDualBoard();
    if (dual == nullptr) {
        std::snprintf(ui, ui_len, "%s", Lang::Strings::SETTINGS_TEST_SIGNAL_BOARD_UNSUP);
        std::snprintf(serial, serial_len, "@@@信号  | 网络: 未知 | DualNetwork 不可用");
        return;
    }

    if (dual->GetNetworkType() == NetworkType::WIFI) {
        FormatWifiSignal(ui, ui_len, serial, serial_len);
    } else {
        FormatCellSignal(ui, ui_len, serial, serial_len);
    }
}

void ApplySignalUi(void* p) {
    auto* msg = static_cast<SignalUiMsg*>(p);
    if (msg == nullptr) {
        return;
    }

    const bool live = s_signal_alive.load(std::memory_order_acquire) &&
                      msg->generation == s_signal_generation.load(std::memory_order_acquire) &&
                      s_signal_value != nullptr && lv_obj_is_valid(s_signal_value);
    if (live) {
        if (std::strncmp(s_last_signal_text, msg->text, kSignalTextMax) != 0) {
            lv_label_set_text(s_signal_value, msg->text);
            std::strncpy(s_last_signal_text, msg->text, kSignalTextMax - 1);
            s_last_signal_text[kSignalTextMax - 1] = '\0';
        }
    }
    delete msg;
}

void SignalSampleTask(void* /*arg*/) {
    const uint32_t gen = s_signal_generation.load(std::memory_order_acquire);

    char ui[kSignalTextMax] = {};
    char serial[80] = {};
    SampleSignalText(ui, sizeof(ui), serial, sizeof(serial));
    ESP_LOGI(TAG, "%s", serial);

    if (!s_signal_alive.load(std::memory_order_acquire) ||
        gen != s_signal_generation.load(std::memory_order_acquire)) {
        s_signal_busy.store(false, std::memory_order_release);
        vTaskDelete(nullptr);
        return;
    }

    auto* msg = new (std::nothrow) SignalUiMsg{};
    if (msg == nullptr) {
        ESP_LOGW(TAG, "signal ui msg alloc failed");
        s_signal_busy.store(false, std::memory_order_release);
        vTaskDelete(nullptr);
        return;
    }
    msg->generation = gen;
    std::strncpy(msg->text, ui, sizeof(msg->text) - 1);
    msg->text[sizeof(msg->text) - 1] = '\0';

    // busy 在投递成功后仍由本任务清掉：下一拍可再采样；迟到 Apply 靠 generation 丢弃。
    if (!SettingsTest_PostLvAsync(ApplySignalUi, msg)) {
        ESP_LOGW(TAG, "PostLvAsync signal ui failed");
        delete msg;
    }
    s_signal_busy.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

void KickSignalSample() {
    if (!s_signal_alive.load(std::memory_order_acquire)) {
        return;
    }
    bool expected = false;
    if (!s_signal_busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }
    const BaseType_t ok =
        xTaskCreate(SignalSampleTask, "stest_sig", 4096, nullptr, 5, nullptr);
    if (ok != pdPASS) {
        ESP_LOGW(TAG, "signal sample task create failed");
        s_signal_busy.store(false, std::memory_order_release);
    }
}

void OnSignalPollTimer(lv_timer_t* /*t*/) {
    KickSignalSample();
}

lv_obj_t* CreateSignalPanel(lv_obj_t* page) {
    lv_obj_t* row = lv_obj_create(page);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, kSignalPanelH);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_black(), 0);
    lv_obj_set_style_pad_hor(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(row);
    lv_label_set_text(title, Lang::Strings::SETTINGS_TEST_SIGNAL_TITLE);
    lv_obj_set_width(title, kSignalTitleW);
    lv_obj_set_height(title, lv_font_get_line_height(fontpack_lv_font_ui()));
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* value = lv_label_create(row);
    lv_label_set_text(value, Lang::Strings::SETTINGS_TEST_SIGNAL_READING);
    lv_obj_set_flex_grow(value, 1);
    lv_obj_set_height(value, lv_font_get_line_height(fontpack_lv_font_ui()));
    lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(value, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(value, lv_color_black(), 0);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_clear_flag(value, LV_OBJ_FLAG_CLICKABLE);
    return value;
}

}  // namespace

void SettingsTestTab_Reset() {
    InvalidateSignalSession();
}

void SettingsTestTab_OnDeactivated() {
    InvalidateSignalSession();
}

void SettingsTestTab_OnActivated() {
    if (s_signal_value == nullptr) {
        return;
    }
    s_signal_alive.store(true, std::memory_order_release);
    StopSignalTimer();
    s_signal_timer = lv_timer_create(OnSignalPollTimer, kSignalPollMs, nullptr);
    KickSignalSample();
}

void SettingsTestTab_Build(lv_obj_t* page) {
    // Build 可能在 TearDown 后再次进入：先清旧会话，避免脏指针挂到新 page。
    InvalidateSignalSession();
    s_signal_value = CreateSignalPanel(page);

    SettingsCreateSelectableOption(page, Lang::Strings::SETTINGS_TEST_AUTO, OnAutoTestClicked, 0);
    SettingsCreateSelectableOption(page, Lang::Strings::SETTINGS_TEST_TOUCH, OnTouchTestClicked, 0);
    SettingsCreateSelectableOption(page, Lang::Strings::SETTINGS_TEST_BATTERY, OnBatteryTestClicked, 0);
    SettingsCreateSelectableOption(page, Lang::Strings::SETTINGS_TEST_AGING, OnAgingTestClicked, 0);
}
