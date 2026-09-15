#pragma once

#include "lvgl.h"

// 设置 → 测试 → 电池测试：电量计 / 充电芯片实时诊断。
class SettingsTestBatteryScreen {
public:
    static lv_obj_t* Create();
};
