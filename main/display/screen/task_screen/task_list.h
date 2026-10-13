#pragma once

#include "task_screen_priv.h"

/** @brief LV 异步：应用批量操作结果 */
void Task_AsyncApplyBatch(void* p);
/** @brief 创建对话框按钮 */
lv_obj_t* Task_MakeDialogBtn(lv_obj_t* parent, const char* text, bool filled, lv_event_cb_t cb);
/** @brief 进入多选并选中指定行 */
void Task_EnterMultiModeSelect(int idx);
/** @brief 刷新底栏（普通/多选） */
void Task_RefreshFooterMode();
/** @brief 行长按进入多选 */
void Task_OnRowLongPressed(lv_event_t* e);
/** @brief 点击同步/刷新 */
void Task_OnSyncClicked(lv_event_t* e);
/** @brief 对单条执行批量 HTTP 操作 */
bool Task_BatchHttpOne(BatchOp op, const char* id, std::string& err);
/** @brief 多选完成 */
void Task_OnMultiComplete(lv_event_t* e);
/** @brief 确保操作对话框已创建 */
void Task_EnsureActionDialog();
/** @brief 调度批量完成/删除 */
void Task_ScheduleBatch(BatchOp op);
/** @brief 对话框删除 */
void Task_OnDialogDelete(lv_event_t* e);
/** @brief 退出多选（可选重建列表） */
void Task_ExitMultiModeEx(bool rebuild, bool clear_status);
/** @brief 批量操作后台任务 */
void Task_BatchTask(void* arg);
/** @brief 点击对话框背景关闭 */
void Task_OnDialogBackdropClicked(lv_event_t* e);
/** @brief 多选删除 */
void Task_OnMultiDelete(lv_event_t* e);
/** @brief 多选全选 */
void Task_OnMultiSelectAll(lv_event_t* e);
/** @brief 行点击（单选操作或多选切换） */
void Task_OnRowClicked(lv_event_t* e);
/** @brief 退出多选 */
void Task_ExitMultiMode(bool rebuild);
/** @brief 多选取消 */
void Task_OnMultiCancel(lv_event_t* e);
/** @brief 对话框完成 */
void Task_OnDialogComplete(lv_event_t* e);
