#pragma once

#include "lvgl.h"

// 设置 → 网络：
// - WiFi / 4G 上网切换（切换后重启）
// - 仅在当前为 WiFi 时显示：配置 WIFI 网络（全屏扫网配网）
// - 仅在当前为 4G 时显示：内置卡 / 外置卡切换
void SettingsNetworkTab_Build(lv_obj_t* page);
void SettingsNetworkTab_Reset();

// 供设置壳层日志使用
const char* SettingsNetworkTab_CurrentName();

// 供拨号页判断：内置卡不可拨打。仅读 RAM 缓存（勿在 LVGL 任务碰 NVS）。
bool SettingsNetworkTab_IsInternalSim();

// 供拨号页判断：WiFi 模式不可拨打（仅读当前网络类型，勿碰 NVS）。
bool SettingsNetworkTab_IsWifi();

// 在非 LVGL 线程从 NVS `network/sim_slot` 刷新缓存（0=外置，1=内置）。
void SettingsNetworkTab_LoadSimSlotFromNvsSync();

// 在非 LVGL 线程预热 SIM 槽位缓存（拨号页 SCREEN_LOADED 可调）。
void SettingsNetworkTab_PrefetchSimSlot();
