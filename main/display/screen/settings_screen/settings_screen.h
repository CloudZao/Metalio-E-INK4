#pragma once

#include "lvgl.h"

// 设置主壳：左侧 Tab 菜单 + 右侧内容页，关于 Tab 作为末项保留。
class SettingsScreen {
public:
    static lv_obj_t* Create();
    static lv_obj_t* CreateTest();
    static void ReloadAfterLanguageChange();
};
