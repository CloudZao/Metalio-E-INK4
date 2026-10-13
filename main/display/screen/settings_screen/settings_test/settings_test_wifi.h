#pragma once

#include "lvgl.h"

/** @brief 调度一次 WiFi 扫描（测试行） */
void SettingsTestWifi_ScheduleScan();
/** @brief 关闭 WiFi 结果弹层 */
void SettingsTestWifi_ClosePopup();
/** @brief 离页拆除 WiFi 扫描/弹层资源 */
void SettingsTestWifi_Teardown();
/** @brief WiFi 测试行点击：打开弹层并扫网 */
void SettingsTestWifi_OnRowClicked(lv_event_t* e);
