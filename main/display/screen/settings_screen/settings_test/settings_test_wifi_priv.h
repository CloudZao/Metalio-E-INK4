#pragma once

#include "settings_test_common.h"

#include <cstdint>
#include <vector>

#include <esp_event.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

constexpr const char* kTag = "SettingsTestWifi";
constexpr const char* kLeaseOwner = "settings_test";
constexpr int kScanTimeoutMs = 15000;
constexpr uint16_t kMaxApRecords = 64;

struct ScanSession {
    SemaphoreHandle_t mu = nullptr; // 会话互斥
    StaticSemaphore_t mu_buf{}; // 静态互斥存储
    EventGroupHandle_t evt = nullptr; // 扫描完成事件组
    esp_event_handler_instance_t handler = nullptr; // 事件句柄
    std::vector<wifi_ap_record_t> records; // 扫描结果
    bool captured = false; // 是否已捕获结果
    bool lease_held = false; // 是否持有 WiFi 租约
    uint32_t generation = 0; // 会话代际
};

struct SettingsTestScanDoneMsg {
    bool success = false; // 扫描是否成功
    int count = 0; // AP 数量
};

extern uint32_t SettingsTestWifi_scan_generation; // 全局扫描代际

/** @brief 按当前页重建 AP 列表 */
void SettingsTestWifi_RebuildWifiListPage();
/** @brief 调度一次 WiFi 扫描 */
void SettingsTestWifi_ScheduleWifiScan();
/** @brief 将扫描记录填入 UI AP 列表 */
void SettingsTestWifi_FillUiApsFromRecords(const std::vector<wifi_ap_record_t>& records);
/** @brief 确保 WiFi 已启动可供扫描 */
bool SettingsTestWifi_WifiEnsureForScan();
/** @brief 若本页拥有 WiFi 栈则拆除 */
void SettingsTestWifi_WifiTeardownIfOwned();
/** @brief 关闭 WiFi 结果弹层 */
void SettingsTestWifi_CloseWifiPopup();
/** @brief 启用/禁用重新扫描按钮 */
void SettingsTestWifi_SetWifiRescanEnabled(bool enabled);
/** @brief WiFi 测试行点击 */
void SettingsTestWifi_OnWifiRowClicked(lv_event_t* e);
