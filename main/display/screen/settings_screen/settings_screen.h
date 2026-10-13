#pragma once

#include "lvgl.h"

// 设置主壳：左侧 Tab 菜单 + 右侧内容页，关于 Tab 作为末项保留。
class SettingsScreen {
public:
    /** @brief 创建设置主页面 */
    static lv_obj_t* Create();
    /** @brief 创建设置→测试子页 */
    static lv_obj_t* CreateTest();
    /** @brief 切换语言后重建设置页文案与布局 */
    static void ReloadAfterLanguageChange();
};

/** @brief 待机 overlay 盖住设置页时暂停当前 Tab 定时器/采样（不拆 UI） */
void SettingsScreen_OnEnterStandby();
/** @brief 退出待机后恢复当前 Tab 的 OnActivated 逻辑 */
void SettingsScreen_OnResumeFromStandby();
