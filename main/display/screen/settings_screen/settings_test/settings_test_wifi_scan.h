#pragma once

#include "settings_test_wifi_priv.h"

/** @brief 作废进行中的扫描会话代际 */
void SettingsTestWifi_InvalidateInFlightScan();
/** @brief 销毁扫描会话并释放事件/锁 */
void SettingsTestWifi_DestroyScanSession(ScanSession* session);
/** @brief WiFi 扫描后台任务入口 */
void SettingsTestWifi_WifiScanTask(void* arg);
/** @brief LV 异步：扫描完成回写 UI */
void SettingsTestWifi_AsyncWifiScanDone(void* p);
/** @brief 投递扫描完成消息到 LVGL 线程 */
void SettingsTestWifi_PostWifiScanDone(bool success, int count);
/** @brief 调度一次 WiFi 扫描（内部实现） */
void SettingsTestWifi_ScheduleWifiScan();
