#pragma once

#include "lvgl.h"

/** @brief 构建设置→测试入口页（信号与四个入口） */
void SettingsTestTab_Build(lv_obj_t* page);
/** @brief 重置测试入口页（作废信号采样会话） */
void SettingsTestTab_Reset();
/** @brief 测试 Tab 激活：启动信号采样 */
void SettingsTestTab_OnActivated();
/** @brief 测试 Tab 停用：停止信号采样 */
void SettingsTestTab_OnDeactivated();
