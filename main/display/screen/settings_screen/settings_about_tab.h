#pragma once

#include "lvgl.h"

/** @brief 构建设置→关于页（系统信息列表） */
void SettingsAboutTab_Build(lv_obj_t* page);
/** @brief 重置关于页 UI/异步检测状态 */
void SettingsAboutTab_Reset();
