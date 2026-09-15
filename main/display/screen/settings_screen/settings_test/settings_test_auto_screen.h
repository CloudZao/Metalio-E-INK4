#pragma once

#include "lvgl.h"

// 设置 → 测试 → 自动测试：全屏自检页（传感器 / WiFi / 双卡 ping）。
class SettingsTestAutoScreen {
public:
    static lv_obj_t* Create();
};
