#pragma once

#include "lvgl.h"

/** @brief 构建设置→网络页（WiFi/4G 与 SIM 切换） */
void SettingsNetworkTab_Build(lv_obj_t* page);
/** @brief 重置网络页 UI/遮罩状态 */
void SettingsNetworkTab_Reset();
/** @brief 当前网络类型显示名（供壳层日志） */
const char* SettingsNetworkTab_CurrentName();
/** @brief 是否为 WiFi 模式（拨号页门禁，仅读当前类型） */
bool SettingsNetworkTab_IsWifi();
/** @brief 非 LVGL 线程从 NVS 刷新 SIM 槽缓存（0=外置，1=内置） */
void SettingsNetworkTab_LoadSimSlotFromNvsSync();
