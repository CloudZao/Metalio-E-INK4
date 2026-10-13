#pragma once

#include "task_screen_priv.h"

/** @brief 解析清单 JSON 为条目列表 */
bool Task_ParseItemsJson(const std::string& body, std::vector<TaskItem>& out, std::string& err);
/** @brief 从 checklist_cache 填充列表 UI */
void Task_ApplyFromCache();
/** @brief 将当前条目发布到 checklist_cache */
void Task_PublishChecklistCache();
/** @brief HTTP JSON 请求（method + url） */
bool Task_HttpJson(const char* method, const std::string& url, std::string& body_out, std::string& err_out);
/** @brief LV 异步：应用删除结果 */
void Task_AsyncApplyDelete(void* p);
/** @brief 拉取清单后台任务 */
void Task_FetchTask(void* arg);
/** @brief 完成条目后台任务 */
void Task_CompleteTask(void* arg);
/** @brief 调度删除指定下标条目 */
void Task_ScheduleDelete(int idx);
/** @brief LV 异步：应用拉取结果（须在 LVGL 任务） */
void Task_AsyncApplyFetch(void* p);
/** @brief 调度完成指定下标条目 */
void Task_ScheduleComplete(int idx);
/** @brief LV 异步：应用完成结果 */
void Task_AsyncApplyComplete(void* p);
/** @brief 删除条目后台任务 */
void Task_DeleteTask(void* arg);
