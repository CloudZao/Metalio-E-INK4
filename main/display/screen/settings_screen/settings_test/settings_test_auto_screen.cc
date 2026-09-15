#include "settings_test_auto_screen.h"

#include "settings_test_audio.h"
#include "settings_test_camera.h"
#include "settings_test_cell.h"
#include "settings_test_common.h"
#include "settings_test_sensors.h"
#include "settings_test_wifi.h"

#include "assets/lang_config.h"
#include "cx25601n.h"
#include "fontpack_lvgl.h"
#include "screen_common.h"
#include "vk_key_handler.h"

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr const char* TAG = "SettingsTestAuto";
constexpr const char* kScreenId = "settings_test_auto";

void StartAutoTests() {
    auto& ui = SettingsTest_Ui();
    ui.active = true;
    ui.cell_state = SettingsTestCellState::Idle;
    ui.cell_auto = false;
    ui.last_cfun_ms = 0;

    ui.sc7a20h_captured = false;
    SettingsTest_SetRowValue(ui.bq27220, Lang::Strings::SETTINGS_TEST_DETECTING, false);
    SettingsTest_SetRowValue(ui.pcf8563, Lang::Strings::SETTINGS_TEST_DETECTING, false);
    SettingsTest_SetRowValue(ui.cx25601, Lang::Strings::SETTINGS_TEST_DETECTING, false);
    SettingsTest_SetRowValue(ui.sdcard, Lang::Strings::SETTINGS_TEST_DETECTING, false);
    SettingsTest_SetRowValue(ui.sc7a20h, Lang::Strings::SETTINGS_TEST_DETECTING, false);
    SettingsTest_SetRowValue(ui.wifi, Lang::Strings::SETTINGS_TEST_SCANNING, false);
    SettingsTest_SetRowValue(ui.sim_int, Lang::Strings::SETTINGS_TEST_WAITING, false);
    SettingsTest_SetRowValue(ui.sim_ext, Lang::Strings::SETTINGS_TEST_WAITING, false);

    /* USB 摄像头供电：进页开 OTG Boost 约 5V/1A */
    if (cx25601n_is_ready()) {
        esp_err_t otg = cx25601n_enable_otg(true);
        if (otg != ESP_OK) {
            ESP_LOGW(TAG, "OTG enable failed: %s", esp_err_to_name(otg));
        }
    } else {
        ESP_LOGW(TAG, "CX25601N not ready, skip OTG");
    }

    SettingsTestSensors_Poll();
    SettingsTestSensors_StartPollTimer();
    SettingsTestWifi_ScheduleScan();
    SettingsTestCell_StartAutoSequence();
}

void ShowRestartHintAndReboot() {
    // 退页 DELETE 时原 screen 已在拆；提示画在 layer_top，给墨水屏一点刷新时间
    lv_obj_t* top = lv_layer_top();
    if (top != nullptr) {
        lv_obj_t* mask = lv_obj_create(top);
        lv_obj_remove_style_all(mask);
        lv_obj_set_size(mask, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(mask, 0, 0);
        lv_obj_set_style_bg_color(mask, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(mask, LV_OPA_COVER, 0);
        lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(mask);
        lv_label_set_text(lbl, Lang::Strings::SETTINGS_TEST_REBOOTING);
        lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_center(lbl);
    }
    ESP_LOGI(TAG, "auto test exit: showing restart hint, then esp_restart");
    vTaskDelay(pdMS_TO_TICKS(1200));
    esp_restart();
}

void OnScreenDeleted(lv_event_t* /*e*/) {
    SettingsTest_BeginShutdown();
    SettingsTestSensors_StopPollTimer();
    SettingsTestCell_StopStartTimer();
    SettingsTestCell_StopRetryTimer();
    SettingsTestWifi_ClosePopup();
    SettingsTestWifi_Teardown();
    SettingsTestAudio_Teardown();
    SettingsTestCamera_Teardown();
    if (cx25601n_is_ready()) {
        esp_err_t otg = cx25601n_enable_otg(false);
        if (otg != ESP_OK) {
            ESP_LOGW(TAG, "OTG disable failed: %s", esp_err_to_name(otg));
        }
    }
    SettingsTest_Ui() = {};
    // USB Host 卸栈在本板会 abort；退出自动测试直接重启，干净恢复 USJ/PHY
    ShowRestartHintAndReboot();
}

}  // namespace

lv_obj_t* SettingsTestAutoScreen::Create() {
    ESP_LOGI(TAG, "create auto test screen");

    auto& ui = SettingsTest_Ui();
    ui = {};

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, OnScreenDeleted, LV_EVENT_DELETE, nullptr);

    EpdStatusBar status = ScreenCreateStatusBar(scr);

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_HOR_RES, LV_VER_RES - status.height);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(body, 14, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(body);
    lv_label_set_text(title, Lang::Strings::SETTINGS_TEST_AUTO);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* exit_hint = lv_label_create(body);
    lv_label_set_text(exit_hint, Lang::Strings::SETTINGS_TEST_EXIT_REBOOT);
    lv_obj_set_style_text_font(exit_hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(exit_hint, lv_color_black(), 0);
    lv_obj_set_style_pad_bottom(exit_hint, 8, 0);
    lv_obj_clear_flag(exit_hint, LV_OBJ_FLAG_CLICKABLE);

    ui.page = body;
    ui.root_scr = scr;
    ui.alive = true;

    ui.bq27220 = SettingsTest_CreateRow(body, "BQ27220", nullptr);
    ui.pcf8563 = SettingsTest_CreateRow(body, "PCF8563", nullptr);
    ui.cx25601 = SettingsTest_CreateRow(body, "CX25601N", nullptr);
    ui.sdcard = SettingsTest_CreateRow(body, Lang::Strings::SETTINGS_TEST_SDCARD, nullptr);
    ui.sc7a20h = SettingsTest_CreateRow(body, "SC7A20H", nullptr);
    SettingsTestAudio_BuildRow(body);
    ui.wifi = SettingsTest_CreateRow(body, "WiFi", SettingsTestWifi_OnRowClicked);
    ui.sim_int = SettingsTest_CreateRow(body, Lang::Strings::SETTINGS_TEST_CELL_INT, nullptr);
    ui.sim_ext = SettingsTest_CreateRow(body, Lang::Strings::SETTINGS_TEST_CELL_EXT, nullptr);
    SettingsTestCamera_BuildRow(body);

    ScreenSetIsHome(false);
    VkKey_AttachScreen(scr, kScreenId, VkKeyScreenDesc{SettingsTestAutoScreen::Create});

    StartAutoTests();
    SettingsTestAudio_OnLoad();
    SettingsTestCamera_OnLoad();
    return scr;
}
