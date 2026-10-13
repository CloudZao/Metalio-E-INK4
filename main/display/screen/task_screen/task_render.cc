// task_render.cc — split from parent .cc
#include "task_screen_priv.h"

#include "task_list.h"
#include "task_render.h"
#include "assets/lang_config.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "screen_common.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <esp_log.h>
#include <esp_timer.h>

// 清单界面：fontpack 25@2（与书库列表 item 一致）
const lv_font_t* Task_Font() {
    return fontpack_lv_font_ui();
}

// 清单标题：fontpack 30@2（与书库标题一致）
const lv_font_t* Task_TitleFont() {
    const lv_font_t* f = fontpack_lv_font_get(30, 2);
    return f != nullptr ? f : Task_Font();
}

lv_coord_t Task_FontLineH() {
    const lv_font_t* font = Task_Font();
    return (font != nullptr && font->line_height > 0) ? font->line_height : 30;
}

// 标题 + 计划时间两行时的行高估算，用于翻页每页条数
lv_coord_t Task_EstimatedTwoLineRowH() {
    const lv_coord_t line_h = Task_FontLineH();
    const lv_coord_t text_h = line_h * 2 + kRowLineGap;
    return text_h + kRowPad * 2 + kBorderW * 2;
}

void Task_DisableScroll(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

bool Task_ItemIsDone(const TaskItem& item) {
    return std::strcmp(item.status, "done") == 0;
}

bool Task_ItemInCurrentTab(const TaskItem& item) {
    return Task_State().tab == 1 ? Task_ItemIsDone(item) : !Task_ItemIsDone(item);
}

bool Task_ItemHasPlan(const TaskItem& item) {
    return item.plan_date[0] != '\0' || item.plan_time[0] != '\0';
}

// 计划时间新→旧（高到底）；无计划时间的排后面
void Task_SortItemsByPlanDesc(std::vector<TaskItem>& items) {
    std::sort(items.begin(), items.end(), [](const TaskItem& a, const TaskItem& b) {
        const bool ha = Task_ItemHasPlan(a);
        const bool hb = Task_ItemHasPlan(b);
        if (ha != hb) {
            return ha;
        }
        const int d = std::strcmp(a.plan_date, b.plan_date);
        if (d != 0) {
            return d > 0;
        }
        return std::strcmp(a.plan_time, b.plan_time) > 0;
    });
}

int Task_FilteredCount() {
    int n = 0;
    for (const auto& item : Task_State().items) {
        if (Task_ItemInCurrentTab(item)) {
            ++n;
        }
    }
    return n;
}

void Task_FormatPlanMeta(char* out, size_t out_sz, const TaskItem& item) {
    if (out == nullptr || out_sz == 0) {
        return;
    }
    char time_buf[8] = {};
    if (item.plan_time[0] != '\0') {
        std::snprintf(time_buf, sizeof(time_buf), "%.5s", item.plan_time);
    }
    if (item.plan_date[0] != '\0' && time_buf[0] != '\0') {
        std::snprintf(out, out_sz, "%s %s", item.plan_date, time_buf);
    } else if (item.plan_date[0] != '\0') {
        std::snprintf(out, out_sz, "%s", item.plan_date);
    } else if (time_buf[0] != '\0') {
        std::snprintf(out, out_sz, "%s", time_buf);
    } else {
        out[0] = '\0';
    }
}

void Task_CancelStatusClearTimer() {
    if (Task_State().status_clear_timer == nullptr) {
        return;
    }
    lv_timer_delete(Task_State().status_clear_timer);
    Task_State().status_clear_timer = nullptr;
}

static void StatusClearTimerCb(lv_timer_t* /*t*/) {
    Task_State().status_clear_timer = nullptr;
    if (!Task_State().screen_alive) {
        return;
    }
    Task_State().status_text[0] = '\0';
    Task_RefreshStatusLbl();
}

void Task_RefreshStatusLbl() {
    if (Task_State().ui.status_lbl == nullptr) {
        return;
    }
    if (Task_State().status_text[0] == '\0') {
        lv_label_set_text(Task_State().ui.status_lbl, "");
        lv_obj_add_flag(Task_State().ui.status_lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(Task_State().ui.status_lbl, Task_State().status_text);
    lv_obj_clear_flag(Task_State().ui.status_lbl, LV_OBJ_FLAG_HIDDEN);
}

// 成功/错误提示定时收起；忙碌态勿开
void Task_SetStatusText(const char* text, bool auto_clear) {
    Task_CancelStatusClearTimer();
    if (!Task_State().screen_alive || Task_State().ui.status_lbl == nullptr) {
        return;
    }
    const char* t = text != nullptr ? text : "";
    strlcpy(Task_State().status_text, t, sizeof(Task_State().status_text));
    Task_RefreshStatusLbl();
    if (auto_clear && Task_State().status_text[0] != '\0') {
        Task_State().status_clear_timer = lv_timer_create(StatusClearTimerCb, kStatusClearMs, nullptr);
        if (Task_State().status_clear_timer != nullptr) {
            lv_timer_set_repeat_count(Task_State().status_clear_timer, 1);
        }
    }
}

void Task_SetEmptyOverride(const char* msg) {
    if (msg != nullptr && msg[0] != '\0') {
        strlcpy(Task_State().empty_override, msg, sizeof(Task_State().empty_override));
    } else {
        Task_State().empty_override[0] = '\0';
    }
}

void Task_HideActionDialog() {
    Task_State().pending_action_idx = -1;
    Task_State().dialog_open.store(false, std::memory_order_release);
    if (Task_State().ui.dialog != nullptr) {
        lv_obj_add_flag(Task_State().ui.dialog, LV_OBJ_FLAG_HIDDEN);
    }
}

int Task_ListPageCount() {
    const int n = Task_FilteredCount();
    if (n <= 0 || Task_State().list_per_page <= 0) {
        return 1;
    }
    return (n + Task_State().list_per_page - 1) / Task_State().list_per_page;
}

void Task_RefreshPageLbl() {
    if (Task_State().ui.page_lbl == nullptr) {
        return;
    }
    const int filtered = Task_FilteredCount();
    if (filtered <= 0) {
        lv_label_set_text(Task_State().ui.page_lbl, "0 / 0");
        return;
    }
    char page_buf[32];
    std::snprintf(page_buf, sizeof(page_buf), "%d / %d", Task_State().list_page + 1, Task_ListPageCount());
    lv_label_set_text(Task_State().ui.page_lbl, page_buf);
}

void Task_SyncSelectedSize() {
    if (Task_State().selected.size() != Task_State().items.size()) {
        Task_State().selected.assign(Task_State().items.size(), 0);
    }
}

int Task_SelectedCount() {
    Task_SyncSelectedSize();
    int n = 0;
    for (uint8_t v : Task_State().selected) {
        if (v != 0) {
            ++n;
        }
    }
    return n;
}

bool Task_ItemSelected(int idx) {
    Task_SyncSelectedSize();
    return idx >= 0 && idx < static_cast<int>(Task_State().selected.size()) && Task_State().selected[static_cast<size_t>(idx)] != 0;
}

void Task_ToggleItemSelected(int idx) {
    Task_SyncSelectedSize();
    if (idx < 0 || idx >= static_cast<int>(Task_State().selected.size())) {
        return;
    }
    Task_State().selected[static_cast<size_t>(idx)] = Task_State().selected[static_cast<size_t>(idx)] ? 0 : 1;
}

void Task_RebuildListPage() {
    if (!Task_State().screen_alive || Task_State().ui.list_host == nullptr) {
        return;
    }
    lv_obj_clean(Task_State().ui.list_host);
    Task_State().ui.list_empty = nullptr;

    const int filtered = Task_FilteredCount();
    if (filtered <= 0) {
        lv_obj_set_style_layout(Task_State().ui.list_host, LV_LAYOUT_NONE, 0);
        Task_State().ui.list_empty = lv_label_create(Task_State().ui.list_host);
        const char* empty_txt = Task_State().empty_override[0] != '\0'
                                    ? Task_State().empty_override
                                    : (Task_State().tab == 1 ? Lang::Strings::TASK_EMPTY_DONE : Lang::Strings::TASK_EMPTY_TODO);
        lv_label_set_text(Task_State().ui.list_empty, empty_txt);
        lv_obj_set_style_text_font(Task_State().ui.list_empty, Task_Font(), 0);
        lv_obj_set_style_text_color(Task_State().ui.list_empty, lv_color_black(), 0);
        lv_obj_center(Task_State().ui.list_empty);
        lv_obj_clear_flag(Task_State().ui.list_empty, LV_OBJ_FLAG_CLICKABLE);
        Task_RefreshPageLbl();
        return;
    }

    lv_obj_set_style_layout(Task_State().ui.list_host, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(Task_State().ui.list_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(Task_State().ui.list_host, kRowGap, 0);

    const int pages = Task_ListPageCount();
    if (Task_State().list_page < 0) {
        Task_State().list_page = 0;
    }
    if (Task_State().list_page >= pages) {
        Task_State().list_page = pages - 1;
    }
    const int start = Task_State().list_page * Task_State().list_per_page;
    const int end = start + Task_State().list_per_page;
    const lv_coord_t row_inner_w = LV_HOR_RES - kPad * 2 - kListFramePad * 2 -
                                   kListFrameBorderW * 2 - kRowPad * 2 - kBorderW * 2;
    const lv_coord_t text_w =
        Task_State().multi ? (row_inner_w - kCheckSize - kCheckGap) : row_inner_w;

    int match_i = 0;
    for (int i = 0; i < static_cast<int>(Task_State().items.size()); ++i) {
        const TaskItem& item = Task_State().items[static_cast<size_t>(i)];
        if (!Task_ItemInCurrentTab(item)) {
            continue;
        }
        const int slot = match_i++;
        if (slot < start || slot >= end) {
            continue;
        }

        void* idx_ud = reinterpret_cast<void*>(static_cast<intptr_t>(i));
        const bool selected = Task_State().multi && Task_ItemSelected(i);

        lv_obj_t* row = lv_obj_create(Task_State().ui.list_host);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_black(), 0);
        lv_obj_set_style_border_width(row, kBorderW, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_all(row, kRowPad, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, kCheckGap, 0);
        Task_DisableScroll(row);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(row, idx_ud);
        HapticAttachClick(row);
        lv_obj_add_event_cb(row, Task_OnRowClicked, LV_EVENT_CLICKED, idx_ud);
        lv_obj_add_event_cb(row, Task_OnRowLongPressed, LV_EVENT_LONG_PRESSED, idx_ud);

        lv_obj_t* text_col = lv_obj_create(row);
        lv_obj_remove_style_all(text_col);
        lv_obj_set_flex_grow(text_col, 1);
        lv_obj_set_height(text_col, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(text_col, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(text_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(text_col, kRowLineGap, 0);
        Task_DisableScroll(text_col);
        lv_obj_clear_flag(text_col, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* name = lv_label_create(text_col);
        lv_label_set_text(name, item.title);
        lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(name, text_w);
        lv_obj_set_style_text_font(name, Task_Font(), 0);
        lv_obj_set_style_text_color(name, lv_color_black(), 0);
        lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);

        char meta[72];
        Task_FormatPlanMeta(meta, sizeof(meta), item);
        if (meta[0] != '\0') {
            lv_obj_t* hint = lv_label_create(text_col);
            lv_label_set_text(hint, meta);
            lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
            lv_obj_set_width(hint, text_w);
            lv_obj_set_style_text_font(hint, Task_Font(), 0);
            lv_obj_set_style_text_color(hint, lv_color_black(), 0);
            lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
        }

        if (Task_State().multi) {
            // 行尾勾选：白底黑框；选中只画 √，勿实心填充
            lv_obj_t* check = lv_obj_create(row);
            lv_obj_remove_style_all(check);
            lv_obj_set_size(check, kCheckSize, kCheckSize);
            lv_obj_set_style_bg_color(check, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(check, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(check, lv_color_black(), 0);
            lv_obj_set_style_border_width(check, kBorderW, 0);
            lv_obj_set_style_radius(check, 4, 0);
            Task_DisableScroll(check);
            // 点框与点行相同：都走 row CLICKED 切换选中
            lv_obj_clear_flag(check, LV_OBJ_FLAG_CLICKABLE);
            if (selected) {
                lv_obj_t* mark = lv_label_create(check);
                lv_label_set_text(mark, "√");
                lv_obj_set_style_text_font(mark, Task_Font(), 0);
                lv_obj_set_style_text_color(mark, lv_color_black(), 0);
                lv_obj_center(mark);
                lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
            }
        }
    }

    Task_RefreshPageLbl();
    Task_RefreshFooterMode();
}

void Task_StyleTabBtn(lv_obj_t* btn, lv_obj_t* lbl, bool selected) {
    // 选中段本身是完整胶囊（两侧圆弧），中间靠圆弧衔接而非竖线
    lv_obj_set_style_bg_color(btn, selected ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    if (lbl != nullptr) {
        lv_obj_set_style_text_color(lbl, selected ? lv_color_white() : lv_color_black(), 0);
    }
}

void Task_RefreshTabUi() {
    for (int i = 0; i < kTabCount; ++i) {
        Task_StyleTabBtn(Task_State().ui.tab_btns[i], Task_State().ui.tab_lbls[i], Task_State().tab == i);
    }
    if (Task_State().ui.section_title != nullptr) {
        lv_label_set_text(Task_State().ui.section_title, Task_State().tab == 1 ? Lang::Strings::TASK_TITLE_DONE : Lang::Strings::TASK_TITLE_TODO);
    }
}

void Task_OnTabClicked(lv_event_t* e) {
    const int tab = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (tab < 0 || tab >= kTabCount || tab == Task_State().tab) {
        return;
    }
    Task_HideActionDialog();
    Task_ExitMultiMode(false);
    Task_State().tab = tab;
    Task_State().list_page = 0;
    Task_SetEmptyOverride(nullptr);
    ESP_LOGI(TAG, "tab %s", tab == 1 ? Lang::Strings::TASK_TAB_DONE : Lang::Strings::TASK_TAB_TODO);
    Task_RefreshTabUi();
    Task_RebuildListPage();
}

lv_obj_t* Task_MakeTabBtn(lv_obj_t* parent, const char* text, int tab) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, lv_pct(100));
    lv_obj_set_style_pad_all(btn, 0, 0);
    Task_DisableScroll(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(btn);
    lv_obj_add_event_cb(btn, Task_OnTabClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(tab)));

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, Task_Font(), 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    Task_State().ui.tab_btns[tab] = btn;
    Task_State().ui.tab_lbls[tab] = lbl;
    return btn;
}

lv_obj_t* Task_FindListRow(int index) {
    if (Task_State().ui.list_host == nullptr || !lv_obj_is_valid(Task_State().ui.list_host)) {
        return nullptr;
    }
    const uint32_t n = lv_obj_get_child_count(Task_State().ui.list_host);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* row = lv_obj_get_child(Task_State().ui.list_host, i);
        if (row != nullptr &&
            static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(row))) == index) {
            return row;
        }
    }
    return nullptr;
}

lv_obj_t* Task_FindRowChildBySize(lv_obj_t* row, lv_coord_t w, lv_coord_t h) {
    if (row == nullptr) {
        return nullptr;
    }
    const uint32_t n = lv_obj_get_child_count(row);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* c = lv_obj_get_child(row, i);
        if (c != nullptr && lv_obj_get_width(c) == w && lv_obj_get_height(c) == h) {
            return c;
        }
    }
    return nullptr;
}

void Task_PatchRowCheckMark(int index) {
    if (!Task_State().multi) {
        return;
    }
    lv_obj_t* row = Task_FindListRow(index);
    lv_obj_t* check = Task_FindRowChildBySize(row, kCheckSize, kCheckSize);
    if (check == nullptr) {
        Task_RequestRebuildListPage(); // 勾选列尚未建好（进多选 coalesce 未落地）
        return;
    }
    lv_obj_clean(check);
    if (!Task_ItemSelected(index)) {
        return;
    }
    lv_obj_t* mark = lv_label_create(check);
    lv_label_set_text(mark, "√");
    lv_obj_set_style_text_font(mark, Task_Font(), 0);
    lv_obj_set_style_text_color(mark, lv_color_black(), 0);
    lv_obj_center(mark);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
}

void Task_PatchVisibleRowCheckMarks() {
    if (!Task_State().multi || Task_State().ui.list_host == nullptr || !lv_obj_is_valid(Task_State().ui.list_host)) {
        return;
    }
    const uint32_t n = lv_obj_get_child_count(Task_State().ui.list_host);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* row = lv_obj_get_child(Task_State().ui.list_host, i);
        if (row == nullptr) {
            continue;
        }
        Task_PatchRowCheckMark(static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(row))));
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), Lang::Strings::TASK_SELECTED_FMT, Task_SelectedCount());
    Task_SetStatusText(buf);
}

void Task_RequestRebuildListPage() {
    if (Task_State().task_paint.paint == nullptr) {
        Task_State().task_paint.paint = Task_RebuildListPage;
    }
    ScreenPaintCoalesceRequest(&Task_State().task_paint);
}

void Task_RequestCheckMarksPaint() {
    if (Task_State().task_check_paint.paint == nullptr) {
        Task_State().task_check_paint.paint = Task_PatchVisibleRowCheckMarks;
    }
    // 点间隙手指会抬起：勿立刻上屏，停手后再一次刷全部 √
    ScreenPaintCoalesceRequestDebounced(&Task_State().task_check_paint, 280000);
}

