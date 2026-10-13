#pragma once

#include "lvgl.h"

/** @brief 周期刷新传感器测试行 */
void SettingsTestSensors_Poll();
/** @brief 启动传感器轮询定时器 */
void SettingsTestSensors_StartPollTimer();
/** @brief 停止传感器轮询定时器 */
void SettingsTestSensors_StopPollTimer();
