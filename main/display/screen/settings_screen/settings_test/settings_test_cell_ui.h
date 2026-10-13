#pragma once

#include "settings_test_cell_priv.h"

/** @brief 启动指定 SIM 槽蜂窝自检任务 */
void SettingsTestCell_StartCellTest(int slot);
/** @brief 建链失败后的重试定时器回调 */
void SettingsTestCell_OnCellRetryTimer(lv_timer_t* t);
/** @brief 停止自动开测定时器（内部） */
void SettingsTestCell_StopCellStartTimer();
/** @brief 异步投递单槽测试结果到 UI */
void SettingsTestCell_AsyncCellDone(void* p);
/** @brief 自动序列：轮询是否可开测下一槽 */
void SettingsTestCell_OnCellAutoStartPoll(lv_timer_t* t);
/** @brief 调度指定槽稍后重试建链 */
void SettingsTestCell_ScheduleCellRetry(int slot);
/** @brief 蜂窝自检 FreeRTOS 任务入口 */
void SettingsTestCell_CellTestTask(void* arg);
/** @brief 停止建链重试定时器（内部） */
void SettingsTestCell_StopCellRetryTimerInternal();
/** @brief 将结果消息投递到 LVGL 任务 */
void SettingsTestCell_PostCellDone(SettingsTestCellDoneMsg* msg);
