// task_cache.cc — split from parent .cc
#include "task_screen_priv.h"

#include "task_cache.h"
#include "api_endpoints.h"
#include "api_http.h"
#include "assets/lang_config.h"
#include "board.h"
#include "checklist_cache.h"
#include "power_policy.h"
#include "screen_common.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <cJSON.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>

void Task_PublishChecklistCache() {
    checklist_cache_begin();
    for (const auto& item : Task_State().items) {
        checklist_cache_append(item.id, item.title, item.status, item.plan_date, item.plan_time);
    }
    checklist_cache_end();
}

void Task_ApplyFromCache() {
    Task_State().items.clear();
    Task_State().list_page = 0;
    Task_State().multi = false;
    Task_State().suppress_row_click_until_us = 0;
    Task_State().suppress_row_click_idx = -1;
    Task_State().selected.clear();
    if (!checklist_cache_ready()) {
        Task_SetStatusText("");
        Task_SetEmptyOverride(nullptr);
        Task_RebuildListPage();
        return;
    }
    checklist_cache_item_t buf[CHECKLIST_CACHE_MAX_ITEMS];
    const size_t n = checklist_cache_copy_all(buf, CHECKLIST_CACHE_MAX_ITEMS);
    Task_State().items.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        TaskItem item;
        strlcpy(item.id, buf[i].id, sizeof(item.id));
        strlcpy(item.title, buf[i].title, sizeof(item.title));
        strlcpy(item.status, buf[i].status, sizeof(item.status));
        strlcpy(item.plan_date, buf[i].plan_date, sizeof(item.plan_date));
        strlcpy(item.plan_time, buf[i].plan_time, sizeof(item.plan_time));
        Task_State().items.push_back(item);
    }
    Task_SortItemsByPlanDesc(Task_State().items);
    Task_SetEmptyOverride(nullptr);
    Task_SetStatusText("");
    Task_RebuildListPage();
}

bool Task_ParseItemsJson(const std::string& body, std::vector<TaskItem>& out, std::string& err) {
    out.clear();
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        err = Lang::Strings::TASK_PARSE_FAIL;
        return false;
    }
    cJSON* code = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(code) || code->valueint != 0) {
        cJSON* msg = cJSON_GetObjectItemCaseSensitive(root, "msg");
        if (cJSON_IsString(msg) && msg->valuestring != nullptr) {
            err = msg->valuestring;
        } else {
            err = Lang::Strings::TASK_API_ERROR;
        }
        cJSON_Delete(root);
        return false;
    }
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsArray(data)) {
        err = Lang::Strings::TASK_DATA_FORMAT_ERR;
        cJSON_Delete(root);
        return false;
    }

    const cJSON* it = nullptr;
    cJSON_ArrayForEach(it, data) {
        if (!cJSON_IsObject(it) || out.size() >= static_cast<size_t>(kMaxItems)) {
            continue;
        }
        cJSON* id = cJSON_GetObjectItemCaseSensitive(it, "id");
        cJSON* title = cJSON_GetObjectItemCaseSensitive(it, "title");
        if (!cJSON_IsString(id) || id->valuestring == nullptr ||
            !cJSON_IsString(title) || title->valuestring == nullptr) {
            continue;
        }
        TaskItem item;
        strlcpy(item.id, id->valuestring, sizeof(item.id));
        strlcpy(item.title, title->valuestring, sizeof(item.title));

        cJSON* plan_date = cJSON_GetObjectItemCaseSensitive(it, "planDate");
        if (cJSON_IsString(plan_date) && plan_date->valuestring != nullptr) {
            strlcpy(item.plan_date, plan_date->valuestring, sizeof(item.plan_date));
        }
        cJSON* plan_time = cJSON_GetObjectItemCaseSensitive(it, "planTime");
        if (cJSON_IsString(plan_time) && plan_time->valuestring != nullptr) {
            strlcpy(item.plan_time, plan_time->valuestring, sizeof(item.plan_time));
        }
        cJSON* status = cJSON_GetObjectItemCaseSensitive(it, "status");
        if (cJSON_IsString(status) && status->valuestring != nullptr) {
            strlcpy(item.status, status->valuestring, sizeof(item.status));
        } else {
            strlcpy(item.status, "pending", sizeof(item.status));
        }
        out.push_back(item);
    }
    cJSON_Delete(root);
    return true;
}

bool Task_HttpJson(const char* method, const std::string& url, std::string& body_out,
              std::string& err_out) {
    body_out.clear();
    if (url.empty()) {
        err_out = "api not configured";
        ESP_LOGI(TAG, "skip checklist http: cloud endpoints blank");
        return false;
    }
    // 短时联网：等 WiFi → 请求；返回后 Release，WiFi 板可再 pause
    PowerNeedHold hold_net(PowerNeed::OtaDownload);

    if (!Board::GetInstance().EnsureNetworkReady()) {
        err_out = Lang::Strings::TASK_NET_NOT_READY;
        return false;
    }
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        err_out = Lang::Strings::TASK_NO_NETWORK;
        return false;
    }
    auto http = network->CreateHttp(0);
    if (http == nullptr) {
        err_out = Lang::Strings::TASK_CONN_FAIL;
        return false;
    }
    http->SetTimeout(kHttpTimeoutMs);
    api::ApplyCommonHeaders(http);
    // 空 POST/PUT 必须 SetContent("")，否则 HttpClient 走 chunked 且不发结束块，服务端一直等 body
    if (method != nullptr &&
        (std::strcmp(method, "POST") == 0 || std::strcmp(method, "PUT") == 0)) {
        http->SetContent(std::string());
    }
    api::LogHttpRequest(TAG, method, url);
    if (!http->Open(method, url)) {
        err_out = Lang::Strings::TASK_REQUEST_FAIL;
        api::LogHttpResponse(TAG, -1, err_out);
        return false;
    }
    const int status = http->GetStatusCode();
    body_out = http->ReadAll();
    http->Close();
    api::LogHttpResponse(TAG, status, api::RedactClawUrlsForLog(body_out));
    if (status < 200 || status >= 300) {
        err_out = body_out.empty() ? ("HTTP " + std::to_string(status)) : Lang::Strings::TASK_REQUEST_FAIL;
        return false;
    }
    return true;
}

// 必须在 LVGL 任务里执行（经 lv_async_call），禁止 Application::Schedule 直接碰控件。
void Task_AsyncApplyFetch(void* p) {
    auto* msg = static_cast<FetchResultMsg*>(p);
    Task_State().busy.store(false);
    if (!msg->ok) {
        if (Task_State().screen_alive) {
            const char* err = msg->err[0] != '\0' ? msg->err : Lang::Strings::TASK_LOAD_FAIL;
            if (Task_State().items.empty()) {
                // 空列表：只在中间显示一条，勿叠「暂无待办」+「网络未就绪」
                Task_SetStatusText("");
                Task_SetEmptyOverride(err);
            } else {
                Task_SetEmptyOverride(nullptr);
                Task_SetStatusText(err, true);
            }
            Task_RebuildListPage();
        }
        Task_ReleaseFetchNet(); // 结果已上屏（或页已离）：再放网，启动保网计时
        delete msg;
        return;
    }
    if (Task_State().screen_alive) {
        Task_State().items = std::move(msg->items);
        Task_SortItemsByPlanDesc(Task_State().items);
        Task_State().list_page = 0;
        Task_PublishChecklistCache();
        Task_SetEmptyOverride(nullptr);
        Task_ExitMultiModeEx(false, false);
        Task_SetStatusText(Lang::Strings::TASK_REFRESHED, true);
        Task_RebuildListPage();
    } else {
        // 后台刷新：只写 cache
        Task_SortItemsByPlanDesc(msg->items);
        checklist_cache_begin();
        for (const auto& item : msg->items) {
            checklist_cache_append(item.id, item.title, item.status, item.plan_date,
                                   item.plan_time);
        }
        checklist_cache_end();
    }
    Task_ReleaseFetchNet();
    delete msg;
}

void Task_AsyncApplyDelete(void* p) {
    auto* msg = static_cast<DeleteResultMsg*>(p);
    Task_State().busy.store(false);
    if (!Task_State().screen_alive) {
        delete msg;
        return;
    }
    if (!msg->ok) {
        Task_SetStatusText(msg->err[0] != '\0' ? msg->err : Lang::Strings::TASK_DELETE_FAIL, true);
        delete msg;
        return;
    }
    for (auto it = Task_State().items.begin(); it != Task_State().items.end(); ++it) {
        if (std::strcmp(it->id, msg->id) == 0) {
            Task_State().items.erase(it);
            break;
        }
    }
    Task_PublishChecklistCache();
    Task_SetEmptyOverride(nullptr);
    Task_SetStatusText(Lang::Strings::TASK_DELETED, true);
    Task_RebuildListPage();
    delete msg;
}

void Task_AsyncApplyComplete(void* p) {
    auto* msg = static_cast<CompleteResultMsg*>(p);
    Task_State().busy.store(false);
    if (!Task_State().screen_alive) {
        delete msg;
        return;
    }
    if (!msg->ok) {
        Task_SetStatusText(msg->err[0] != '\0' ? msg->err : Lang::Strings::TASK_COMPLETE_FAIL, true);
        delete msg;
        return;
    }
    for (auto& item : Task_State().items) {
        if (std::strcmp(item.id, msg->id) == 0) {
            strlcpy(item.status, "done", sizeof(item.status));
            break;
        }
    }
    Task_PublishChecklistCache();
    Task_SetEmptyOverride(nullptr);
    Task_SetStatusText(Lang::Strings::TASK_COMPLETED, true);
    Task_RebuildListPage();
    delete msg;
}

void Task_FetchTask(void* /*arg*/) {
    // 硬占网至 Task_AsyncApplyFetch：结果上屏后再 Release，启动保网时长
    Task_AcquireFetchNet();
    auto* msg = new FetchResultMsg();
    std::string body;
    std::string err;
    const bool http_ok = Task_HttpJson("GET", api::ChecklistItemsAllUrl(), body, err);
    if (!http_ok) {
        msg->ok = false;
        strlcpy(msg->err, err.c_str(), sizeof(msg->err));
    } else if (!Task_ParseItemsJson(body, msg->items, err)) {
        msg->ok = false;
        strlcpy(msg->err, err.c_str(), sizeof(msg->err));
    } else {
        msg->ok = true;
    }

    Task_State().fetch_task = nullptr;
    if (lv_async_call(Task_AsyncApplyFetch, msg) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "lv_async_call fetch failed");
        delete msg;
        Task_State().busy.store(false);
        Task_ReleaseFetchNet();
    }
    vTaskDelete(nullptr);
}

void Task_DeleteTask(void* arg) {
    char id[48] = {};
    strlcpy(id, static_cast<const char*>(arg), sizeof(id));
    delete[] static_cast<char*>(arg);

    auto* msg = new DeleteResultMsg();
    strlcpy(msg->id, id, sizeof(msg->id));

    std::string body;
    std::string err;
    bool ok = Task_HttpJson("DELETE", api::ChecklistItemUrl(id), body, err);
    if (ok && !body.empty()) {
        cJSON* root = cJSON_Parse(body.c_str());
        if (root != nullptr) {
            cJSON* code = cJSON_GetObjectItemCaseSensitive(root, "code");
            if (!cJSON_IsNumber(code) || code->valueint != 0) {
                ok = false;
                cJSON* m = cJSON_GetObjectItemCaseSensitive(root, "msg");
                if (cJSON_IsString(m) && m->valuestring != nullptr) {
                    err = m->valuestring;
                } else {
                    err = Lang::Strings::TASK_DELETE_FAIL;
                }
            }
            cJSON_Delete(root);
        }
    }
    if (!ok && err.empty()) {
        err = Lang::Strings::TASK_DELETE_FAIL;
    }
    msg->ok = ok;
    if (!ok) {
        strlcpy(msg->err, err.c_str(), sizeof(msg->err));
    }

    Task_State().delete_task = nullptr;
    if (lv_async_call(Task_AsyncApplyDelete, msg) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "lv_async_call delete failed");
        delete msg;
        Task_State().busy.store(false);
    }
    vTaskDelete(nullptr);
}

void Task_CompleteTask(void* arg) {
    char id[48] = {};
    strlcpy(id, static_cast<const char*>(arg), sizeof(id));
    delete[] static_cast<char*>(arg);

    auto* msg = new CompleteResultMsg();
    strlcpy(msg->id, id, sizeof(msg->id));

    std::string body;
    std::string err;
    bool ok = Task_HttpJson("POST", api::ChecklistItemCompleteUrl(id), body, err);
    if (ok && !body.empty()) {
        cJSON* root = cJSON_Parse(body.c_str());
        if (root != nullptr) {
            cJSON* code = cJSON_GetObjectItemCaseSensitive(root, "code");
            if (!cJSON_IsNumber(code) || code->valueint != 0) {
                ok = false;
                cJSON* m = cJSON_GetObjectItemCaseSensitive(root, "msg");
                if (cJSON_IsString(m) && m->valuestring != nullptr) {
                    err = m->valuestring;
                } else {
                    err = Lang::Strings::TASK_COMPLETE_FAIL;
                }
            }
            cJSON_Delete(root);
        }
    }
    if (!ok && err.empty()) {
        err = Lang::Strings::TASK_COMPLETE_FAIL;
    }
    msg->ok = ok;
    if (!ok) {
        strlcpy(msg->err, err.c_str(), sizeof(msg->err));
    }

    Task_State().complete_task = nullptr;
    if (lv_async_call(Task_AsyncApplyComplete, msg) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "lv_async_call complete failed");
        delete msg;
        Task_State().busy.store(false);
    }
    vTaskDelete(nullptr);
}

void Task_ScheduleDelete(int idx) {
    if (!Task_State().screen_alive || idx < 0 || idx >= static_cast<int>(Task_State().items.size())) {
        return;
    }
    if (Task_AnyWorkerBusy()) {
        Task_SetStatusText(Lang::Strings::TASK_PLEASE_WAIT, true);
        return;
    }
    char* id_copy = new char[48];
    strlcpy(id_copy, Task_State().items[static_cast<size_t>(idx)].id, 48);
    Task_State().busy.store(true);
    Task_SetStatusText(Lang::Strings::TASK_DELETING);
    if (xTaskCreatePinnedToCore(Task_DeleteTask, "task_del", kWorkerStack, id_copy,
                                tskIDLE_PRIORITY + 2, &Task_State().delete_task, 0) != pdPASS) {
        delete[] id_copy;
        Task_State().delete_task = nullptr;
        Task_State().busy.store(false);
        Task_SetStatusText(Lang::Strings::TASK_DELETE_START_FAIL, true);
    }
}

void Task_ScheduleComplete(int idx) {
    if (!Task_State().screen_alive || idx < 0 || idx >= static_cast<int>(Task_State().items.size())) {
        return;
    }
    if (Task_AnyWorkerBusy()) {
        Task_SetStatusText(Lang::Strings::TASK_PLEASE_WAIT, true);
        return;
    }
    char* id_copy = new char[48];
    strlcpy(id_copy, Task_State().items[static_cast<size_t>(idx)].id, 48);
    Task_State().busy.store(true);
    Task_SetStatusText(Lang::Strings::TASK_COMPLETING);
    if (xTaskCreatePinnedToCore(Task_CompleteTask, "task_done", kWorkerStack, id_copy,
                                tskIDLE_PRIORITY + 2, &Task_State().complete_task, 0) != pdPASS) {
        delete[] id_copy;
        Task_State().complete_task = nullptr;
        Task_State().busy.store(false);
        Task_SetStatusText(Lang::Strings::TASK_COMPLETE_START_FAIL, true);
    }
}

void TaskScreen::RequestCacheRefresh() {
    if (Task_State().fetch_task != nullptr) {
        return;
    }
    Task_State().busy.store(true);
    if (xTaskCreatePinnedToCore(Task_FetchTask, "task_fetch", kWorkerStack, nullptr,
                                tskIDLE_PRIORITY + 2, &Task_State().fetch_task, 0) != pdPASS) {
        Task_State().fetch_task = nullptr;
        Task_State().busy.store(false);
        ESP_LOGW(TAG, "cache refresh task start failed");
    }
}

void TaskScreen::EnsureCacheSynced() {
    if (!Board::GetInstance().IsNetworkReady()) {
        ESP_LOGW(TAG, "EnsureCacheSynced skip: network not ready");
        return;
    }
    const std::string url = api::ChecklistItemsAllUrl();
    if (url.empty()) {
        ESP_LOGI(TAG, "EnsureCacheSynced skip: cloud endpoints blank");
        return;
    }
    std::vector<TaskItem> items;
    std::string body;
    std::string err;
    if (!Task_HttpJson("GET", url, body, err)) {
        ESP_LOGW(TAG, "EnsureCacheSynced GET failed: %s", err.c_str());
        return;
    }
    if (!Task_ParseItemsJson(body, items, err)) {
        ESP_LOGW(TAG, "EnsureCacheSynced parse failed: %s", err.c_str());
        return;
    }
    Task_SortItemsByPlanDesc(items);
    // 开机路径：只写 cache，不依赖清单页存活
    checklist_cache_begin();
    for (const auto& item : items) {
        checklist_cache_append(item.id, item.title, item.status, item.plan_date, item.plan_time);
    }
    checklist_cache_end();
    ESP_LOGI(TAG, "EnsureCacheSynced ok items=%u", static_cast<unsigned>(items.size()));
    // 开机同步时若已在清单页：投递 LVGL 刷新（勿在 Application 线程碰控件）
    if (IsActive()) {
        if (lv_async_call(
                [](void* /*p*/) {
                    if (Task_State().screen_alive) {
                        Task_ApplyFromCache();
                    }
                },
                nullptr) != LV_RESULT_OK) {
            ESP_LOGW(TAG, "EnsureCacheSynced: UI reload schedule failed");
        }
    }
}

void TaskScreen::OnResumeFromStandby() {
    if (!IsActive()) {
        return;
    }
    ESP_LOGD(TAG, "resume from standby -> reload cache");
    Task_ApplyFromCache();
}
