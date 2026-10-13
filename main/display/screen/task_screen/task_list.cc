// task_list.cc — split from parent .cc
#include "task_screen_priv.h"

#include "task_list.h"
#include "api_endpoints.h"
#include "api_http.h"
#include "assets/lang_config.h"
#include "board.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "power_policy.h"
#include "screen_common.h"

#include <cJSON.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>

void Task_RefreshFooterMode() {
    if (Task_State().ui.footer == nullptr) {
        return;
    }
    if (Task_State().ui.page_lbl != nullptr) {
        lv_obj_clear_flag(Task_State().ui.page_lbl, LV_OBJ_FLAG_HIDDEN);
    }
    if (Task_State().multi) {
        if (Task_State().ui.sync_btn != nullptr) {
            lv_obj_add_flag(Task_State().ui.sync_btn, LV_OBJ_FLAG_HIDDEN);
        }
        if (Task_State().ui.multi_bar != nullptr) {
            lv_obj_clear_flag(Task_State().ui.multi_bar, LV_OBJ_FLAG_HIDDEN);
        }
        if (Task_State().ui.multi_complete_btn != nullptr) {
            if (Task_State().tab == 1) {
                lv_obj_add_flag(Task_State().ui.multi_complete_btn, LV_OBJ_FLAG_HIDDEN);
                if (Task_State().ui.multi_complete_sep != nullptr) {
                    lv_obj_add_flag(Task_State().ui.multi_complete_sep, LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                lv_obj_clear_flag(Task_State().ui.multi_complete_btn, LV_OBJ_FLAG_HIDDEN);
                if (Task_State().ui.multi_complete_sep != nullptr) {
                    lv_obj_clear_flag(Task_State().ui.multi_complete_sep, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), Lang::Strings::TASK_SELECTED_FMT, Task_SelectedCount());
        Task_SetStatusText(buf);
    } else {
        if (Task_State().ui.multi_bar != nullptr) {
            lv_obj_add_flag(Task_State().ui.multi_bar, LV_OBJ_FLAG_HIDDEN);
        }
        if (Task_State().ui.sync_btn != nullptr) {
            lv_obj_clear_flag(Task_State().ui.sync_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void Task_ExitMultiModeEx(bool rebuild, bool clear_status) {
    if (!Task_State().multi && Task_State().selected.empty()) {
        if (rebuild) {
            Task_RequestRebuildListPage();
        }
        return;
    }
    Task_State().multi = false;
    Task_State().suppress_row_click_until_us = 0;
    Task_State().suppress_row_click_idx = -1;
    Task_State().selected.assign(Task_State().items.size(), 0);
    if (clear_status) {
        Task_SetStatusText("");
    }
    Task_RefreshFooterMode();
    if (rebuild) {
        Task_RequestRebuildListPage();
    }
}

void Task_ExitMultiMode(bool rebuild) {
    Task_ExitMultiModeEx(rebuild, true);
}

void Task_EnterMultiModeSelect(int idx) {
    Task_HideActionDialog();
    Task_State().multi = true;
    Task_SyncSelectedSize();
    Task_State().selected.assign(Task_State().items.size(), 0);
    // 长按进入即勾选当前项
    if (idx >= 0 && idx < static_cast<int>(Task_State().selected.size())) {
        Task_State().selected[static_cast<size_t>(idx)] = 1;
    }
    Task_RefreshFooterMode();
    Task_RequestRebuildListPage();
}

void Task_OnRowLongPressed(lv_event_t* e) {
    if (Task_State().busy.load() || Task_State().dialog_open.load(std::memory_order_acquire)) {
        return;
    }
    const int idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (idx < 0 || idx >= static_cast<int>(Task_State().items.size())) {
        return;
    }
    if (!Task_State().multi) {
        Task_State().suppress_row_click_idx = idx;
        Task_State().suppress_row_click_until_us = esp_timer_get_time() + kSuppressRowClickUs;
        Task_EnterMultiModeSelect(idx);
        return;
    }
    Task_ToggleItemSelected(idx);
    Task_RequestCheckMarksPaint();
}

void Task_OnRowClicked(lv_event_t* e) {
    const int idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (idx == Task_State().suppress_row_click_idx &&
        esp_timer_get_time() < Task_State().suppress_row_click_until_us) {
        return;
    }
    if (Task_State().busy.load()) {
        return;
    }
    if (Task_State().multi) {
        Task_ToggleItemSelected(idx);
        Task_RequestCheckMarksPaint();
        return;
    }
    Task_ShowActionDialog(idx);
}

void Task_OnDialogDelete(lv_event_t* /*e*/) {
    const int idx = Task_State().pending_action_idx;
    Task_HideActionDialog();
    if (idx < 0 || idx >= static_cast<int>(Task_State().items.size())) {
        return;
    }
    Task_ScheduleDelete(idx);
}

void Task_OnDialogComplete(lv_event_t* /*e*/) {
    const int idx = Task_State().pending_action_idx;
    Task_HideActionDialog();
    if (idx < 0 || idx >= static_cast<int>(Task_State().items.size())) {
        return;
    }
    if (Task_ItemIsDone(Task_State().items[static_cast<size_t>(idx)])) {
        Task_SetStatusText(Lang::Strings::TASK_DONE_OK, true);
        return;
    }
    Task_ScheduleComplete(idx);
}

void Task_AsyncApplyBatch(void* p) {
    auto* msg = static_cast<BatchResultMsg*>(p);
    Task_State().busy.store(false);
    if (!Task_State().screen_alive) {
        delete msg;
        return;
    }
    if (msg->ok_n > 0) {
        if (msg->op == BatchOp::kDelete) {
            for (int i = 0; i < msg->ok_n; ++i) {
                for (auto it = Task_State().items.begin(); it != Task_State().items.end(); ++it) {
                    if (std::strcmp(it->id, msg->ok_ids[i]) == 0) {
                        Task_State().items.erase(it);
                        break;
                    }
                }
            }
        } else {
            for (int i = 0; i < msg->ok_n; ++i) {
                for (auto& item : Task_State().items) {
                    if (std::strcmp(item.id, msg->ok_ids[i]) == 0) {
                        strlcpy(item.status, "done", sizeof(item.status));
                        break;
                    }
                }
            }
        }
        Task_PublishChecklistCache();
        Task_SetEmptyOverride(nullptr);
    }

    char buf[64];
    if (msg->ok_n > 0 && msg->fail_n == 0) {
        std::snprintf(buf, sizeof(buf), msg->op == BatchOp::kDelete ? Lang::Strings::TASK_REMOVED_N_FMT : Lang::Strings::TASK_COMPLETED_N_FMT,
                      msg->ok_n);
        Task_SetStatusText(buf, true);
    } else if (msg->ok_n > 0) {
        std::snprintf(buf, sizeof(buf), Lang::Strings::TASK_BATCH_RESULT_FMT, msg->ok_n, msg->fail_n);
        Task_SetStatusText(buf, true);
    } else {
        Task_SetStatusText(msg->err[0] != '\0' ? msg->err : Lang::Strings::TASK_BATCH_FAIL, true);
    }
    Task_ExitMultiModeEx(true, false);
    delete msg;
}

bool Task_BatchHttpOne(BatchOp op, const char* id, std::string& err) {
    if (id == nullptr || id[0] == '\0') {
        err = Lang::Strings::TASK_INVALID;
        return false;
    }
    std::string body;
    bool ok = false;
    if (op == BatchOp::kDelete) {
        ok = Task_HttpJson("DELETE", api::ChecklistItemUrl(id), body, err);
    } else {
        ok = Task_HttpJson("POST", api::ChecklistItemCompleteUrl(id), body, err);
    }
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
                    err = op == BatchOp::kDelete ? Lang::Strings::TASK_DELETE_FAIL : Lang::Strings::TASK_COMPLETE_FAIL;
                }
            }
            cJSON_Delete(root);
        }
    }
    return ok;
}

void Task_BatchTask(void* arg) {
    auto* work = static_cast<BatchWork*>(arg);
    auto* msg = new BatchResultMsg();
    msg->op = work->op;

    if (work->id_n <= 0) {
        msg->fail_n = 1;
        strlcpy(msg->err, work->op == BatchOp::kComplete ? Lang::Strings::TASK_NO_COMPLETABLE : Lang::Strings::TASK_SELECT_FIRST,
                sizeof(msg->err));
    } else {
        // 整批保网（须在 vTaskDelete 前 Release，勿跨 delete 用 RAII）
        {
            PowerNeedHold hold_net(PowerNeed::OtaDownload);
            if (!Board::GetInstance().EnsureNetworkReady()) {
                msg->fail_n = work->id_n;
                strlcpy(msg->err, Lang::Strings::TASK_NET_NOT_READY, sizeof(msg->err));
            } else {
                for (int i = 0; i < work->id_n; ++i) {
                    std::string err;
                    bool ok = Task_BatchHttpOne(work->op, work->ids[i], err);
                    // 连接超时等：重试 1 次
                    if (!ok) {
                        vTaskDelay(pdMS_TO_TICKS(400));
                        Board::GetInstance().EnsureNetworkReady();
                        err.clear();
                        ok = Task_BatchHttpOne(work->op, work->ids[i], err);
                    }
                    if (ok) {
                        if (msg->ok_n < kMaxItems) {
                            strlcpy(msg->ok_ids[msg->ok_n], work->ids[i], sizeof(msg->ok_ids[0]));
                            ++msg->ok_n;
                        }
                    } else {
                        ++msg->fail_n;
                        if (msg->err[0] == '\0') {
                            strlcpy(msg->err, err.empty() ? Lang::Strings::TASK_BATCH_FAIL : err.c_str(),
                                    sizeof(msg->err));
                        }
                    }
                }
            }
        }
    }

    delete work;
    Task_State().batch_task = nullptr;
    if (lv_async_call(Task_AsyncApplyBatch, msg) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "lv_async_call batch failed");
        delete msg;
        Task_State().busy.store(false);
    }
    vTaskDelete(nullptr);
}

void Task_ScheduleBatch(BatchOp op) {
    if (!Task_State().screen_alive || !Task_State().multi) {
        return;
    }
    if (Task_AnyWorkerBusy()) {
        Task_SetStatusText(Lang::Strings::TASK_PLEASE_WAIT, true);
        return;
    }
    auto* work = new BatchWork();
    work->op = op;
    Task_SyncSelectedSize();
    for (size_t i = 0; i < Task_State().items.size() && i < Task_State().selected.size(); ++i) {
        if (Task_State().selected[i] == 0) {
            continue;
        }
        if (op == BatchOp::kComplete && Task_ItemIsDone(Task_State().items[i])) {
            continue;
        }
        if (work->id_n >= kMaxItems) {
            break;
        }
        strlcpy(work->ids[work->id_n], Task_State().items[i].id, sizeof(work->ids[0]));
        ++work->id_n;
    }
    if (work->id_n <= 0) {
        delete work;
        Task_SetStatusText(op == BatchOp::kComplete ? Lang::Strings::TASK_NO_COMPLETABLE : Lang::Strings::TASK_SELECT_FIRST, true);
        return;
    }
    Task_State().busy.store(true);
    Task_SetStatusText(op == BatchOp::kDelete ? Lang::Strings::TASK_BATCH_REMOVING : Lang::Strings::TASK_BATCH_COMPLETING);
    if (xTaskCreatePinnedToCoreWithCaps(Task_BatchTask, "task_batch", kBatchWorkerStack, work,
                                        tskIDLE_PRIORITY + 2, &Task_State().batch_task, 0,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        delete work;
        Task_State().batch_task = nullptr;
        Task_State().busy.store(false);
        Task_SetStatusText(Lang::Strings::TASK_BATCH_START_FAIL, true);
    }
}

void Task_OnMultiCancel(lv_event_t* /*e*/) {
    if (Task_State().busy.load()) {
        return;
    }
    Task_ExitMultiMode(true);
}

void Task_OnMultiSelectAll(lv_event_t* /*e*/) {
    if (Task_State().busy.load() || !Task_State().multi) {
        return;
    }
    Task_SyncSelectedSize();
    const int filtered = Task_FilteredCount();
    if (filtered <= 0) {
        return;
    }
    int selected_in_tab = 0;
    for (size_t i = 0; i < Task_State().items.size() && i < Task_State().selected.size(); ++i) {
        if (Task_ItemInCurrentTab(Task_State().items[i]) && Task_State().selected[i] != 0) {
            ++selected_in_tab;
        }
    }
    const bool clear = (selected_in_tab >= filtered);
    for (size_t i = 0; i < Task_State().items.size() && i < Task_State().selected.size(); ++i) {
        if (Task_ItemInCurrentTab(Task_State().items[i])) {
            Task_State().selected[i] = clear ? 0 : 1;
        }
    }
    Task_RequestCheckMarksPaint();
}

void Task_OnMultiDelete(lv_event_t* /*e*/) {
    Task_ScheduleBatch(BatchOp::kDelete);
}

void Task_OnMultiComplete(lv_event_t* /*e*/) {
    if (Task_State().tab == 1) {
        return;
    }
    Task_ScheduleBatch(BatchOp::kComplete);
}

void Task_OnSyncClicked(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (Task_State().dialog_open.load(std::memory_order_acquire)) {
        return;
    }
    if (Task_AnyWorkerBusy()) {
        Task_SetStatusText(Lang::Strings::TASK_PLEASE_WAIT, true);
        return;
    }
    const int64_t now = esp_timer_get_time();
    if (Task_State().last_user_sync_us != 0 && (now - Task_State().last_user_sync_us) < kSyncThrottleUs) {
        Task_SetStatusText(Lang::Strings::TASK_PLEASE_WAIT, true);
        return;
    }
    if (Board::GetInstance().IsWifiConfigMode()) {
        Task_SetStatusText(Lang::Strings::TASK_NEED_WIFI_CFG, true);
        return;
    }
    Task_State().last_user_sync_us = now;
    Task_SetStatusText(Board::GetInstance().IsNetworkReady() ? Lang::Strings::TASK_REFRESHING : Lang::Strings::TASK_CONNECTING_NET);
    TaskScreen::RequestCacheRefresh();
    if (!Task_State().busy.load()) {
        Task_SetStatusText(Lang::Strings::TASK_REFRESH_START_FAIL, true);
    }
}

lv_obj_t* Task_MakeDialogBtn(lv_obj_t* parent, const char* text, bool filled, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_height(btn, kActionH);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, kBorderW, 0);
    lv_obj_set_style_border_color(btn, lv_color_black(), 0);
    if (filled) {
        lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    }
    Task_DisableScroll(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(btn);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, Task_Font(), 0);
    lv_obj_set_style_text_color(lbl, filled ? lv_color_white() : lv_color_black(), 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    return btn;
}

void Task_OnDialogBackdropClicked(lv_event_t* e) {
    // 仅点遮罩本身关闭；点到卡片/按钮时 target ≠ current_target
    if (lv_event_get_target(e) != lv_event_get_current_target(e)) {
        return;
    }
    Task_HideActionDialog();
}

void Task_EnsureActionDialog() {
    if (Task_State().ui.dialog != nullptr || Task_State().ui.screen == nullptr) {
        return;
    }

    Task_State().ui.dialog = lv_obj_create(Task_State().ui.screen);
    lv_obj_remove_style_all(Task_State().ui.dialog);
    lv_obj_set_size(Task_State().ui.dialog, LV_HOR_RES, LV_VER_RES);
    // 弹框外：网点灰底，点此区域关闭
    ScreenApplyDotBackdrop(Task_State().ui.dialog);
    lv_obj_set_style_layout(Task_State().ui.dialog, LV_LAYOUT_NONE, 0);
    Task_DisableScroll(Task_State().ui.dialog);
    lv_obj_add_flag(Task_State().ui.dialog, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(Task_State().ui.dialog, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(Task_State().ui.dialog, Task_OnDialogBackdropClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* card = lv_obj_create(Task_State().ui.dialog);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, LV_HOR_RES - kPad * 2);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(card, LV_VER_RES - kPad * 2, 0);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, kBorderW, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 12, 0);
    Task_DisableScroll(card);
    // 吸收点击，避免点卡片空白处落到遮罩上误关
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        card, [](lv_event_t* e) { lv_event_stop_bubbling(e); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_center(card);

    Task_State().ui.dialog_msg = lv_label_create(card);
    lv_label_set_text(Task_State().ui.dialog_msg, Lang::Strings::TASK_CHOOSE_ACTION);
    lv_obj_set_width(Task_State().ui.dialog_msg, LV_HOR_RES - kPad * 2 - 40);
    lv_obj_set_height(Task_State().ui.dialog_msg, LV_SIZE_CONTENT);
    lv_label_set_long_mode(Task_State().ui.dialog_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(Task_State().ui.dialog_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(Task_State().ui.dialog_msg, Task_Font(), 0);
    lv_obj_set_style_text_color(Task_State().ui.dialog_msg, lv_color_black(), 0);
    lv_obj_clear_flag(Task_State().ui.dialog_msg, LV_OBJ_FLAG_CLICKABLE);

    // 一行一个：完成（进行中才显示）/ 删除；无返回按钮，点遮罩关闭
    Task_State().ui.dialog_complete_btn = Task_MakeDialogBtn(card, Lang::Strings::TASK_COMPLETE_TODO, true, Task_OnDialogComplete);
    Task_MakeDialogBtn(card, Lang::Strings::TASK_DELETE_TODO, false, Task_OnDialogDelete);
}

