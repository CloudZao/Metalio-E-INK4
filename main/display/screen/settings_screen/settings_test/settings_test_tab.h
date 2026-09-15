#pragma once

#include "lvgl.h"

// 设置 → 测试：入口页。
// 顶部「网络信号」实时显示 WiFi RSSI / 4G CSQ（后台采样 + 串口同步）；
// 其下四个入口：自动测试 / 触摸测试 / 电池测试 / 老化测试。
void SettingsTestTab_Build(lv_obj_t* page);
void SettingsTestTab_Reset();
void SettingsTestTab_OnActivated();
void SettingsTestTab_OnDeactivated();
