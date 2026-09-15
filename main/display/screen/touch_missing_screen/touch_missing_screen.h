#pragma once

#include "lvgl.h"

// 触摸 IC 检测失败时的提示页（全屏居中文案）。
class TouchMissingScreen {
public:
    static lv_obj_t* Create();
};
