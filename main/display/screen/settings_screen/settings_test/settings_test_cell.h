#pragma once

#include "lvgl.h"

/** @brief 启动双卡自动测试序列 */
void SettingsTestCell_StartAutoSequence();
/** @brief 停止自动开测定时器 */
void SettingsTestCell_StopStartTimer();
/** @brief 停止建链重试定时器 */
void SettingsTestCell_StopRetryTimer();
