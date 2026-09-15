#pragma once

#include "dual_network_board.h"
#include "lvgl.h"
#include "nt26_board.h"

#include <cstdint>
#include <string>
#include <vector>

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>

// 设置 → 测试 Tab 内部共享类型与状态（仅 settings_test/ 内使用）。

constexpr const char* kSettingsTestIconPass = "A:ic_app_test_pass.spng";
constexpr const char* kSettingsTestIconFail = "A:ic_app_test_fail.spng";
constexpr lv_coord_t kSettingsTestRowH = 56;
constexpr lv_coord_t kSettingsTestIconSz = 28;
constexpr lv_coord_t kSettingsTestTitleW = 152;  // 容纳 CX25601N / 4G·内置 等单行名称
constexpr int kSettingsTestSimExternal = 0;
constexpr int kSettingsTestSimInternal = 1;
constexpr uint32_t kSettingsTestPollPeriodMs = 800;
constexpr uint32_t kSettingsTestAutoStartDelayMs = 600;
constexpr uint32_t kSettingsTestCellWaitPollMs = 500;
constexpr uint32_t kSettingsTestCellWaitMaxMs = 30000;
constexpr uint32_t kSettingsTestBitScanDone = 1u;
constexpr int kSettingsTestWifiPageSize = 4;
constexpr lv_coord_t kSettingsTestWifiPopupH = 380;

struct SettingsTestRow {
    lv_obj_t* row = nullptr;
    lv_obj_t* icon = nullptr;
    lv_obj_t* value = nullptr;
};

struct SettingsTestApItem {
    std::string ssid;
    int8_t rssi = -127;
    wifi_auth_mode_t authmode = WIFI_AUTH_OPEN;
};

enum class SettingsTestCellState {
    Idle,
    TestingInternal,
    TestingExternal,
};

struct SettingsTestUi {
    lv_obj_t* page = nullptr;
    lv_obj_t* root_scr = nullptr;
    SettingsTestRow bq27220;
    SettingsTestRow pcf8563;
    SettingsTestRow cx25601;
    SettingsTestRow sdcard;
    SettingsTestRow sc7a20h;
    SettingsTestRow audio;
    SettingsTestRow wifi;
    SettingsTestRow sim_int;
    SettingsTestRow sim_ext;
    SettingsTestRow camera;

    // SC7A20H：成功读到一次角度后冻结显示，不再随 poll 刷新。
    bool sc7a20h_captured = false;

    lv_obj_t* wifi_mask = nullptr;
    lv_obj_t* wifi_status = nullptr;
    lv_obj_t* wifi_list = nullptr;
    lv_obj_t* wifi_page_lbl = nullptr;
    lv_obj_t* wifi_rescan_btn = nullptr;

    lv_timer_t* poll_timer = nullptr;
    lv_timer_t* cell_start_timer = nullptr;
    lv_timer_t* cell_retry_timer = nullptr;

    bool alive = false;
    bool active = false;

    bool wifi_scan_busy = false;
    bool wifi_stack_owned = false;
    std::vector<SettingsTestApItem> wifi_aps;
    int wifi_page = 0;
    esp_netif_t* wifi_netif = nullptr;

    SettingsTestCellState cell_state = SettingsTestCellState::Idle;
    bool cell_auto = false;
    bool cell_busy = false;
    uint32_t last_cfun_ms = 0;
};

SettingsTestUi& SettingsTest_Ui();

// 页内后台任务 / lv_async_call 回写 UI 前必须检查；离页时先 BeginShutdown 再 Teardown。
bool SettingsTest_IsLive();
void SettingsTest_BeginShutdown();

/**
 * 非 LVGL 线程投递 UI：先持 esp_lv_adapter_lock 再 lv_async_call。
 * 禁止裸 lv_async_call（与 lv_timer_handler 竞态会搞坏定时器链表 → Cache/MMU 崩溃）。
 * @return 是否已入队；失败时调用方须释放 user_data（若堆分配）
 */
bool SettingsTest_PostLvAsync(void (*cb)(void*), void* user_data = nullptr);

void SettingsTest_SetRowStatus(SettingsTestRow& row, bool pass);
void SettingsTest_SetRowValue(SettingsTestRow& row, const char* text, bool error);
SettingsTestRow SettingsTest_CreateRow(lv_obj_t* parent, const char* title,
                                       lv_event_cb_t row_click_cb);

DualNetworkBoard* SettingsTest_GetDualBoard();
Nt26Board* SettingsTest_GetNt26Board();
bool SettingsTest_IsCellNetwork();
