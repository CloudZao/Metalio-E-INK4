// task_screen.cc — Create / VK / lifecycle
#include "task_screen.h"
#include "task_vk.h"
#include "task_render.h"
#include "task_list.h"
#include "task_cache.h"
#include "task_screen_priv.h"

#include "assets/lang_config.h"
#include "checklist_cache.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "power_policy.h"
#include "screen_common.h"
#include "vk_key_handler.h"
#include "vk_page_repeat.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>

#include <esp_log.h>

TaskUiState& Task_State() {
    static TaskUiState s;
    return s;
}

bool Task_PageRepeatStep(int page_delta) {
    if (page_delta == 0 || Task_State().dialog_open.load(std::memory_order_acquire)) {
        return false;
    }
    const int last = std::max(0, Task_ListPageCount() - 1);
    int next = Task_State().list_page + page_delta;
    if (next < 0) {
        next = 0;
    } else if (next > last) {
        next = last;
    }
    if (next == Task_State().list_page) {
        return false;
    }
    Task_State().list_page = next;
    Task_RequestRebuildListPage();
    return page_delta < 0 ? Task_State().list_page > 0 : Task_State().list_page < last;
}

bool Task_OnVkKeyLongPress(const char* key) {
    return VkPageRepeatTryStart(key, Task_PageRepeatStep);
}

bool Task_OnVkKeyPressUp(const char* key) {
    return VkPageRepeatOnPressUp(key);
}

bool Task_OnVkKey(const char* key) {
    if (key == nullptr) {
        return false;
    }
    // 弹窗打开时：盖板键在 touch_feed，禁止同步改 LVGL（连续点返回会卡死）
    if (Task_State().dialog_open.load(std::memory_order_acquire)) {
        if (std::strcmp(key, "vk_prev") == 0) {
            ScreenLvAsync([](void*) {
                if (Task_State().screen_alive) {
                    Task_HideActionDialog();
                }
            });
            return true;
        }
        if (std::strcmp(key, "vk_next") == 0) {
            return true;
        }
        return false;
    }
    // 多选：非首页翻上一页；首页 vk_prev 取消多选（勿出栈回首页）
    if (std::strcmp(key, "vk_prev") == 0) {
        if (Task_State().list_page > 0) {
            --Task_State().list_page;
            Task_RequestRebuildListPage();
            return true;
        }
        if (Task_State().multi) {
            if (Task_State().busy.load()) {
                return true;
            }
            ScreenLvAsync([](void*) {
                if (Task_State().screen_alive) {
                    Task_ExitMultiMode(true);
                }
            });
            return true;
        }
        return false;
    }
    if (std::strcmp(key, "vk_next") == 0) {
        if (Task_State().list_page + 1 < Task_ListPageCount()) {
            ++Task_State().list_page;
            Task_RequestRebuildListPage();
        }
        return true;
    }
    return false;
}

void Task_OnScreenUnloaded(lv_event_t* e) {
    lv_obj_t* scr = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (scr != Task_State().ui.screen) {
        return;
    }
    Task_CancelStatusClearTimer();
    Task_State().screen_alive = false;
    ScreenPaintCoalesceReset(&Task_State().task_paint);
    ScreenPaintCoalesceReset(&Task_State().task_check_paint);
    Task_State().ui = {};
    Task_State().items.clear();
    Task_State().selected.clear();
    Task_State().multi = false;
    Task_State().suppress_row_click_until_us = 0;
    Task_State().suppress_row_click_idx = -1;
    Task_State().tab = 0;
    Task_State().list_page = 0;
    Task_State().pending_action_idx = -1;
    Task_State().status_text[0] = '\0';
    Task_State().empty_override[0] = '\0';
    Task_State().dialog_open.store(false, std::memory_order_release);
}

void Task_OnScreenLoaded(lv_event_t* e) {
    lv_obj_t* scr = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (scr != Task_State().ui.screen || !Task_State().screen_alive) {
        return;
    }
    // 只读 cache；无缓存显示空态，不在本页主动 GET
    Task_ApplyFromCache();
}

void Task_ShowActionDialog(int idx) {
    if (idx < 0 || idx >= static_cast<int>(Task_State().items.size())) {
        return;
    }
    Task_EnsureActionDialog();
    if (Task_State().ui.dialog == nullptr) {
        return;
    }
    Task_State().pending_action_idx = idx;
    const TaskItem& item = Task_State().items[static_cast<size_t>(idx)];
    if (Task_State().ui.dialog_msg != nullptr) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "「%s」", item.title);
        lv_label_set_text(Task_State().ui.dialog_msg, buf);
    }
    // 已完成任务不再显示「完成待办」
    if (Task_State().ui.dialog_complete_btn != nullptr) {
        if (Task_ItemIsDone(item)) {
            lv_obj_add_flag(Task_State().ui.dialog_complete_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(Task_State().ui.dialog_complete_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_clear_flag(Task_State().ui.dialog, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(Task_State().ui.dialog);
    Task_State().dialog_open.store(true, std::memory_order_release);
}

bool Task_AnyWorkerBusy() {
    return Task_State().fetch_task != nullptr || Task_State().delete_task != nullptr ||
           Task_State().complete_task != nullptr || Task_State().batch_task != nullptr || Task_State().busy.load();
}

void Task_AcquireFetchNet() {
    if (!Task_State().fetch_net_held.exchange(true)) {
        PowerPolicy::GetInstance().Acquire(PowerNeed::OtaDownload);
    }
}

void Task_ReleaseFetchNet() {
    if (Task_State().fetch_net_held.exchange(false)) {
        PowerPolicy::GetInstance().Release(PowerNeed::OtaDownload);
    }
}

lv_obj_t* TaskScreen::Create() {
    ESP_LOGI(TAG, "launch TaskScreen");
    Task_State().ui = {};
    Task_State().screen_alive = true;
    Task_State().items.clear();
    Task_State().selected.clear();
    Task_State().multi = false;
    Task_State().suppress_row_click_until_us = 0;
    Task_State().suppress_row_click_idx = -1;
    Task_State().tab = 0;
    Task_State().list_page = 0;
    Task_State().pending_action_idx = -1;
    Task_State().busy.store(false);
    Task_State().dialog_open.store(false, std::memory_order_release);
    Task_State().status_text[0] = '\0';
    Task_State().empty_override[0] = '\0';
    Task_State().last_user_sync_us = 0;
    Task_CancelStatusClearTimer();

    ScreenSetIsHome(false);

    lv_obj_t* scr = lv_obj_create(nullptr);
    Task_State().ui.screen = scr;
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, Task_Font(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    Task_DisableScroll(scr);
    lv_obj_add_event_cb(scr, Task_OnScreenLoaded, LV_EVENT_SCREEN_LOADED, nullptr);
    lv_obj_add_event_cb(scr, Task_OnScreenUnloaded, LV_EVENT_SCREEN_UNLOADED, nullptr);

    EpdStatusBar status = ScreenCreateStatusBar(scr);
    const lv_coord_t body_h = LV_VER_RES - status.height;

    Task_State().list_per_page =
        (body_h - kPad * 2 - 28 - kTabH - kRowGap - kHeaderH - kRowGap - kFooterOutsideH -
         kListFramePad * 2 - kListFrameBorderW * 2 - kActionRowH - kRowGap) /
        (Task_EstimatedTwoLineRowH() + kRowGap);
    if (Task_State().list_per_page < 3) {
        Task_State().list_per_page = 3;
    }

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_HOR_RES, body_h);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(body, kPad, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(body, kRowGap, 0);
    Task_DisableScroll(body);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* tab_row = lv_obj_create(body);
    lv_obj_remove_style_all(tab_row);
    lv_obj_set_width(tab_row, kTabW);
    lv_obj_set_height(tab_row, kTabH);
    lv_obj_set_style_bg_color(tab_row, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(tab_row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tab_row, lv_color_black(), 0);
    lv_obj_set_style_border_width(tab_row, kTabBorderW, 0);
    lv_obj_set_style_radius(tab_row, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(tab_row, true, 0);
    lv_obj_set_flex_flow(tab_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tab_row, kTabInset, 0);
    lv_obj_set_style_pad_column(tab_row, 0, 0);
    Task_DisableScroll(tab_row);
    lv_obj_clear_flag(tab_row, LV_OBJ_FLAG_CLICKABLE);

    Task_MakeTabBtn(tab_row, Lang::Strings::TASK_TAB_TODO, 0);
    Task_MakeTabBtn(tab_row, Lang::Strings::TASK_TAB_DONE, 1);
    Task_RefreshTabUi();

    lv_obj_t* header_row = lv_obj_create(body);
    lv_obj_remove_style_all(header_row);
    lv_obj_set_width(header_row, lv_pct(100));
    lv_obj_set_height(header_row, kHeaderH);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header_row, kHeaderGap, 0);
    Task_DisableScroll(header_row);
    lv_obj_clear_flag(header_row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* star_wrap = lv_obj_create(header_row);
    lv_obj_remove_style_all(star_wrap);
    lv_obj_set_size(star_wrap, kHeaderIcon, kHeaderIcon);
    lv_obj_set_style_bg_color(star_wrap, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(star_wrap, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(star_wrap, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(star_wrap, 0, 0);
    Task_DisableScroll(star_wrap);
    lv_obj_clear_flag(star_wrap, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* star = lv_label_create(star_wrap);
    lv_label_set_text(star, "\xE2\x98\x85");  // ★
    lv_obj_set_style_text_color(star, lv_color_white(), 0);
    lv_obj_set_style_text_font(star, Task_Font(), 0);
    lv_obj_center(star);
    lv_obj_clear_flag(star, LV_OBJ_FLAG_CLICKABLE);

    Task_State().ui.section_title = lv_label_create(header_row);
    lv_label_set_text(Task_State().ui.section_title, Lang::Strings::TASK_TITLE_TODO);
    lv_obj_set_style_text_font(Task_State().ui.section_title, Task_TitleFont(), 0);
    lv_obj_set_style_text_color(Task_State().ui.section_title, lv_color_black(), 0);
    lv_obj_clear_flag(Task_State().ui.section_title, LV_OBJ_FLAG_CLICKABLE);

    // 与「日程待办 / 已完成」同行稍后：完成中、已删除等
    Task_State().ui.status_lbl = lv_label_create(header_row);
    lv_label_set_text(Task_State().ui.status_lbl, "");
    lv_obj_set_style_pad_left(Task_State().ui.status_lbl, kStatusGap, 0);
    lv_obj_set_style_text_font(Task_State().ui.status_lbl, Task_Font(), 0);
    lv_obj_set_style_text_color(Task_State().ui.status_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(Task_State().ui.status_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(Task_State().ui.status_lbl, LV_OBJ_FLAG_HIDDEN);

    // 外框：包住待办列表 + 同步/批量操作；页码在框外（同传输页）
    Task_State().ui.list_frame = lv_obj_create(body);
    lv_obj_remove_style_all(Task_State().ui.list_frame);
    lv_obj_set_width(Task_State().ui.list_frame, LV_HOR_RES - kPad * 2);
    lv_obj_set_flex_grow(Task_State().ui.list_frame, 1);
    lv_obj_set_style_bg_color(Task_State().ui.list_frame, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(Task_State().ui.list_frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(Task_State().ui.list_frame, kListFrameBorderW, 0);
    lv_obj_set_style_border_color(Task_State().ui.list_frame, lv_color_black(), 0);
    lv_obj_set_style_radius(Task_State().ui.list_frame, kListFrameRadius, 0);
    lv_obj_set_style_pad_all(Task_State().ui.list_frame, kListFramePad, 0);
    lv_obj_set_flex_flow(Task_State().ui.list_frame, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(Task_State().ui.list_frame, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(Task_State().ui.list_frame, 0, 0);
    Task_DisableScroll(Task_State().ui.list_frame);
    lv_obj_clear_flag(Task_State().ui.list_frame, LV_OBJ_FLAG_CLICKABLE);

    Task_State().ui.list_host = lv_obj_create(Task_State().ui.list_frame);
    lv_obj_remove_style_all(Task_State().ui.list_host);
    lv_obj_set_width(Task_State().ui.list_host, lv_pct(100));
    lv_obj_set_flex_grow(Task_State().ui.list_host, 1);
    lv_obj_set_flex_flow(Task_State().ui.list_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(Task_State().ui.list_host, kRowGap, 0);
    Task_DisableScroll(Task_State().ui.list_host);

    Task_State().ui.footer = lv_obj_create(Task_State().ui.list_frame);
    lv_obj_remove_style_all(Task_State().ui.footer);
    lv_obj_set_width(Task_State().ui.footer, lv_pct(100));
    lv_obj_set_height(Task_State().ui.footer, kActionRowH);
    lv_obj_set_style_bg_opa(Task_State().ui.footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(Task_State().ui.footer, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(Task_State().ui.footer, kSyncDividerW, 0);
    lv_obj_set_style_border_color(Task_State().ui.footer, lv_color_black(), 0);
    lv_obj_set_flex_flow(Task_State().ui.footer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(Task_State().ui.footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    Task_DisableScroll(Task_State().ui.footer);
    lv_obj_clear_flag(Task_State().ui.footer, LV_OBJ_FLAG_CLICKABLE);

    // 分割线 + 居中「刷新」下划线（多选时隐藏，换 multi_bar）
    Task_State().ui.sync_btn = lv_obj_create(Task_State().ui.footer);
    lv_obj_remove_style_all(Task_State().ui.sync_btn);
    lv_obj_set_width(Task_State().ui.sync_btn, lv_pct(100));
    lv_obj_set_height(Task_State().ui.sync_btn, kSyncBtnH);
    lv_obj_set_style_bg_opa(Task_State().ui.sync_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(Task_State().ui.sync_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(Task_State().ui.sync_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    Task_DisableScroll(Task_State().ui.sync_btn);
    lv_obj_add_flag(Task_State().ui.sync_btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(Task_State().ui.sync_btn);
    lv_obj_add_event_cb(Task_State().ui.sync_btn, Task_OnSyncClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* sync_text = lv_obj_create(Task_State().ui.sync_btn);
    lv_obj_remove_style_all(sync_text);
    lv_obj_set_size(sync_text, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_border_side(sync_text, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(sync_text, kSyncUnderlineH, 0);
    lv_obj_set_style_border_color(sync_text, lv_color_black(), 0);
    lv_obj_set_style_pad_bottom(sync_text, 2, 0);
    lv_obj_set_style_pad_hor(sync_text, kSyncUnderlinePadHor, 0);
    lv_obj_set_style_bg_opa(sync_text, LV_OPA_TRANSP, 0);
    Task_DisableScroll(sync_text);
    lv_obj_clear_flag(sync_text, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* sync_lbl = lv_label_create(sync_text);
    lv_label_set_text(sync_lbl, Lang::Strings::TASK_REFRESH);
    lv_obj_set_style_text_font(sync_lbl, Task_Font(), 0);
    lv_obj_set_style_text_color(sync_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(sync_lbl, LV_OBJ_FLAG_CLICKABLE);

    Task_State().ui.multi_bar = lv_obj_create(Task_State().ui.footer);
    lv_obj_remove_style_all(Task_State().ui.multi_bar);
    lv_obj_set_width(Task_State().ui.multi_bar, lv_pct(100));
    lv_obj_set_height(Task_State().ui.multi_bar, kSyncBtnH);
    lv_obj_set_style_bg_opa(Task_State().ui.multi_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(Task_State().ui.multi_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(Task_State().ui.multi_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(Task_State().ui.multi_bar, 10, 0);
    Task_DisableScroll(Task_State().ui.multi_bar);
    lv_obj_clear_flag(Task_State().ui.multi_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(Task_State().ui.multi_bar, LV_OBJ_FLAG_HIDDEN);

    // 与「刷新」同款：下划线文字；多项间居中「·」
    auto make_multi_action = [](lv_obj_t* parent, const char* text, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_obj_create(parent);
        lv_obj_remove_style_all(btn);
        lv_obj_set_height(btn, kSyncBtnH);
        lv_obj_set_width(btn, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_hor(btn, 4, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        Task_DisableScroll(btn);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* text_wrap = lv_obj_create(btn);
        lv_obj_remove_style_all(text_wrap);
        lv_obj_set_size(text_wrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_border_side(text_wrap, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(text_wrap, kSyncUnderlineH, 0);
        lv_obj_set_style_border_color(text_wrap, lv_color_black(), 0);
        lv_obj_set_style_pad_bottom(text_wrap, 2, 0);
        lv_obj_set_style_pad_hor(text_wrap, kSyncUnderlinePadHor, 0);
        lv_obj_set_style_bg_opa(text_wrap, LV_OPA_TRANSP, 0);
        Task_DisableScroll(text_wrap);
        lv_obj_clear_flag(text_wrap, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(text_wrap);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, Task_Font(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        return btn;
    };
    auto make_multi_dot = [](lv_obj_t* parent) {
        lv_obj_t* dot = lv_obj_create(parent);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, kMultiDotSize, kMultiDotSize);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        Task_DisableScroll(dot);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        return dot;
    };
    make_multi_action(Task_State().ui.multi_bar, Lang::Strings::COMMON_CANCEL, Task_OnMultiCancel);
    make_multi_dot(Task_State().ui.multi_bar);
    make_multi_action(Task_State().ui.multi_bar, Lang::Strings::COMMON_SELECT_ALL, Task_OnMultiSelectAll);
    make_multi_dot(Task_State().ui.multi_bar);
    make_multi_action(Task_State().ui.multi_bar, Lang::Strings::COMMON_REMOVE, Task_OnMultiDelete);
    Task_State().ui.multi_complete_sep = make_multi_dot(Task_State().ui.multi_bar);
    Task_State().ui.multi_complete_btn = make_multi_action(Task_State().ui.multi_bar, Lang::Strings::TASK_COMPLETE, Task_OnMultiComplete);

    Task_State().ui.page_lbl = lv_label_create(body);
    lv_label_set_text(Task_State().ui.page_lbl, "0 / 0");
    lv_obj_set_width(Task_State().ui.page_lbl, LV_HOR_RES - kPad * 2);
    lv_obj_set_height(Task_State().ui.page_lbl, kFooterH);
    lv_obj_set_style_text_align(Task_State().ui.page_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(Task_State().ui.page_lbl, Task_Font(), 0);
    lv_obj_clear_flag(Task_State().ui.page_lbl, LV_OBJ_FLAG_CLICKABLE);

    VkKey_AttachScreen(scr, kScreenId,
                       VkKeyScreenDesc{TaskScreen::Create, Task_OnVkKey, nullptr, nullptr, nullptr,
                                       nullptr, Task_OnVkKeyLongPress, Task_OnVkKeyPressUp});
    return scr;
}

bool TaskScreen::IsActive() {
    return Task_State().screen_alive && Task_State().ui.screen != nullptr;
}
