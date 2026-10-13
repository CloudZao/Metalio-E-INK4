#pragma once

#include "lvgl.h"

// 设置 → 测试 → 电池测试：电量计 / 充电芯片实时诊断。
class SettingsTestBatteryScreen {
public:
    /** @brief 创建电池测试页 */
    static lv_obj_t* Create();
};

/** 待机 overlay：停 1s 轮询，避免醒后与其它 lv_timer 抢 LVGL */
void SettingsTestBatteryScreen_PauseForStandby();
void SettingsTestBatteryScreen_ResumeFromStandby();
