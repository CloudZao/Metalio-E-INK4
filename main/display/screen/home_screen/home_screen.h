#pragma once

#include "lvgl.h"

// 首页：状态栏 + Hero + 应用宫格；支持多种卡片样式。
class HomeScreen {
public:
    static constexpr int kCardStyleWhite = 0;
    static constexpr int kCardStyleGray = 1;
    static constexpr int kCardStyleBorder = 2;
    static constexpr int kCardStyleSlash = 3;

    static lv_obj_t* Create();

    // 读写首页卡片样式，非法值回落到默认样式。
    static int LoadCardStyle();
    static void SaveCardStyle(int style);
};
