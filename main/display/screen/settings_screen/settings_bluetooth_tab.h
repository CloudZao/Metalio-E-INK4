#pragma once

#include "lvgl.h"

// 设置页蓝牙 Tab：构建页面、重置状态和切换生命周期。
void SettingsBluetoothTab_Build(lv_obj_t* page);
void SettingsBluetoothTab_Reset();
void SettingsBluetoothTab_OnActivated();
void SettingsBluetoothTab_OnDeactivated();
