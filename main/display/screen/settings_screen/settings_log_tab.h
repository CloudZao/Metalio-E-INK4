#pragma once

#include "lvgl.h"

/** @brief 构建设置→日志页（开关 + 上传入口） */
void SettingsLogTab_Build(lv_obj_t* page);
/** @brief 重置日志页；若在上传则中断并拆全屏进度 */
void SettingsLogTab_Reset();
