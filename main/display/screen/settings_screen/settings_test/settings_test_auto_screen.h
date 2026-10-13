#pragma once

#include "lvgl.h"

/** @brief 设置→测试→自动测试：全屏自检（传感器 / WiFi / 双卡 ping） */
class SettingsTestAutoScreen {
public:
    /** @brief 创建自动化测试页 */
    static lv_obj_t* Create();
};
