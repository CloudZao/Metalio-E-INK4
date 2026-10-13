#pragma once

#include "task_screen_priv.h"

/** @brief 释放拉取期间占用的保网 */
void Task_ReleaseFetchNet();
/** @brief 屏卸载：停任务、清绑定 */
void Task_OnScreenUnloaded(lv_event_t* e);
/** @brief 拉取前硬占网，结果上屏后再 Release */
void Task_AcquireFetchNet();
/** @brief 取清单页 UI 状态 */
TaskUiState& Task_State();
/** @brief 是否有后台 worker 忙碌 */
bool Task_AnyWorkerBusy();
/** @brief 盖板键长按连翻 */
bool Task_OnVkKeyLongPress(const char* key);
/** @brief 显示单条操作对话框 */
void Task_ShowActionDialog(int idx);
/** @brief 盖板键抬起停止连翻 */
bool Task_OnVkKeyPressUp(const char* key);
/** @brief 盖板键短按翻页 */
bool Task_OnVkKey(const char* key);
/** @brief 连翻步进回调 */
bool Task_PageRepeatStep(int page_delta);
/** @brief 屏加载：绑定键与刷新 */
void Task_OnScreenLoaded(lv_event_t* e);
