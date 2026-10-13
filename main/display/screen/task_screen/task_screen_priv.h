#pragma once

#include "task_screen.h"

#include "screen_common.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

constexpr const char* TAG = "TaskScreen";
constexpr const char* kScreenId = "task";

constexpr lv_coord_t kPad = 12;
constexpr lv_coord_t kBorderW = 2;
constexpr lv_coord_t kRowPad = 10;     // row pad_all，与 Task_RebuildListPage 一致
constexpr lv_coord_t kRowGap = 8;
constexpr lv_coord_t kRowLineGap = 6;
constexpr lv_coord_t kFooterH = 36;       // 框外页码行高
constexpr lv_coord_t kActionH = 48;
constexpr lv_coord_t kTabH = 48;
constexpr lv_coord_t kTabW = 280;      // 顶部分段条宽度（居中）
constexpr lv_coord_t kTabBorderW = 3;  // 外圈黑边（略宽于列表描边）
constexpr lv_coord_t kTabInset = 3;    // 内缩，让选中胶囊两侧圆弧可见
constexpr lv_coord_t kHeaderH = 36;    // 「日程待办」标题行高
constexpr lv_coord_t kHeaderIcon = 32; // 星标圆直径
constexpr lv_coord_t kHeaderGap = 8;   // 星标与标题间距
constexpr int kTabCount = 2;
constexpr int kHttpTimeoutMs = 15000;
constexpr int kMaxItems = 64;
constexpr int kWorkerStack = 10 * 1024;
constexpr int kBatchWorkerStack = 16 * 1024; // 批量多次 HTTP，栈放 SPIRAM
constexpr int64_t kSyncThrottleUs = 3000000; // 手动同步最短间隔 3s
constexpr lv_coord_t kSyncBtnH = 44;
constexpr lv_coord_t kSyncDividerW = 1;   // 底栏顶部分割线
constexpr lv_coord_t kSyncUnderlineH = 1; // 「刷新」/多选动作下划线
constexpr lv_coord_t kSyncUnderlinePadHor = 5; // 下划线两端各外延；共比字宽加长 10px
constexpr lv_coord_t kMultiDotSize = 6;        // 分隔圆点直径（约 · 的 2 倍）
constexpr lv_coord_t kListFrameBorderW = 2;
constexpr lv_coord_t kListFrameRadius = 10;
constexpr lv_coord_t kListFramePad = 10;
constexpr lv_coord_t kActionRowH = kSyncBtnH; // 框内同步/批量行高（同传输 kFooterBlockH）
constexpr lv_coord_t kFooterOutsideH = kFooterH; // 框外仅页码；与框间距靠 body pad_row
constexpr lv_coord_t kCheckSize = 28; // 多选行尾勾选框
constexpr lv_coord_t kCheckGap = 10;
constexpr int64_t kSuppressRowClickUs = 400000; // 400ms：只挡该行松手，不挡其它行连点
constexpr uint32_t kStatusClearMs = 2000;
constexpr lv_coord_t kStatusGap = 12;  // 标题与提示间距

struct TaskItem {
    char id[48] = {};
    char title[96] = {};
    char plan_date[16] = {};
    char plan_time[16] = {};
    char status[16] = {};
};

struct UiWidgets {
    lv_obj_t* screen = nullptr;
    lv_obj_t* tab_btns[kTabCount] = {};
    lv_obj_t* tab_lbls[kTabCount] = {};
    lv_obj_t* section_title = nullptr;
    lv_obj_t* status_lbl = nullptr;  // 与标题同行，稍后显示完成中/已删除等
    lv_obj_t* list_host = nullptr;
    lv_obj_t* list_empty = nullptr;
    lv_obj_t* list_frame = nullptr;          // 列表+操作外框（页码在框外）
    lv_obj_t* footer = nullptr;             // 框内同步/批量操作区
    lv_obj_t* page_lbl = nullptr;
    lv_obj_t* sync_btn = nullptr;
    lv_obj_t* multi_bar = nullptr;           // 多选底栏（框内）
    lv_obj_t* multi_complete_sep = nullptr;  // 「完成」前圆点（已完成 Tab 一并隐藏）
    lv_obj_t* multi_complete_btn = nullptr;  // 批量完成（已完成 Tab 隐藏）
    lv_obj_t* dialog = nullptr;
    lv_obj_t* dialog_msg = nullptr;
    lv_obj_t* dialog_complete_btn = nullptr;
};

struct FetchResultMsg {
    bool ok = false;
    char err[80] = {};
    std::vector<TaskItem> items;
};

struct DeleteResultMsg {
    bool ok = false;
    char err[80] = {};
    char id[48] = {};
};

struct CompleteResultMsg {
    bool ok = false;
    char err[80] = {};
    char id[48] = {};
};

enum class BatchOp : uint8_t { kDelete = 0, kComplete };

struct BatchResultMsg {
    BatchOp op = BatchOp::kDelete;
    int ok_n = 0;
    int fail_n = 0;
    char err[80] = {};
    char ok_ids[kMaxItems][48] = {};
};

struct BatchWork {
    BatchOp op = BatchOp::kDelete;
    int id_n = 0;
    char ids[kMaxItems][48] = {};
};

struct TaskUiState {
    UiWidgets ui{};
    bool screen_alive = false;
    std::vector<TaskItem> items;
    std::vector<uint8_t> selected; // 与 items 等长；1=多选选中
    bool multi = false;
    int64_t suppress_row_click_until_us = 0; // 仅吞长按进多选同一行的松手假 CLICKED
    int suppress_row_click_idx = -1;
    int tab = 0;  // 0=待办 1=已完成
    int list_page = 0;
    int list_per_page = 5;
    int pending_action_idx = -1;
    TaskHandle_t fetch_task = nullptr;
    TaskHandle_t delete_task = nullptr;
    TaskHandle_t complete_task = nullptr;
    TaskHandle_t batch_task = nullptr;
    std::atomic<bool> busy{false};
    std::atomic<bool> dialog_open{false};
    std::atomic<bool> fetch_net_held{false}; // Fetch 硬占网至结果上屏后再 Release
    char status_text[80] = {};
    char empty_override[80] = {};
    lv_timer_t* status_clear_timer = nullptr;
    int64_t last_user_sync_us = 0;
    ScreenPaintCoalesce task_paint{};
    ScreenPaintCoalesce task_check_paint{};
};

/** @brief 取清单页 UI 状态 */
TaskUiState& Task_State();

/** @brief 清单正文字体 */
const lv_font_t* Task_Font();
/** @brief 清单标题字体 */
const lv_font_t* Task_TitleFont();
/** @brief 两行行高估算（标题+计划） */
lv_coord_t Task_EstimatedTwoLineRowH();
/** @brief 禁用对象滚动 */
void Task_DisableScroll(lv_obj_t* obj);
/** @brief 条目是否已完成 */
bool Task_ItemIsDone(const TaskItem& item);
/** @brief 条目是否属于当前 Tab */
bool Task_ItemInCurrentTab(const TaskItem& item);
/** @brief 按计划时间新→旧排序 */
void Task_SortItemsByPlanDesc(std::vector<TaskItem>& items);
/** @brief 当前 Tab 过滤后的条目数 */
int Task_FilteredCount();
/** @brief 取消状态栏自动清除定时器 */
void Task_CancelStatusClearTimer();
/** @brief 设置状态栏提示（可选自动清除） */
void Task_SetStatusText(const char* text, bool auto_clear = false);
/** @brief 设置空态覆盖文案 */
void Task_SetEmptyOverride(const char* msg);
/** @brief 隐藏操作对话框 */
void Task_HideActionDialog();
/** @brief 是否有后台 worker 忙碌 */
bool Task_AnyWorkerBusy();
/** @brief 拉取前硬占网 */
void Task_AcquireFetchNet();
/** @brief 释放拉取保网 */
void Task_ReleaseFetchNet();
/** @brief 将当前条目发布到 checklist_cache */
void Task_PublishChecklistCache();
/** @brief 从 checklist_cache 填充列表 UI */
void Task_ApplyFromCache();
/** @brief 列表总页数 */
int Task_ListPageCount();
/** @brief 同步多选数组尺寸 */
void Task_SyncSelectedSize();
/** @brief 多选已选数量 */
int Task_SelectedCount();
/** @brief 切换条目多选状态 */
void Task_ToggleItemSelected(int idx);
/** @brief 刷新底栏模式 */
void Task_RefreshFooterMode();
/** @brief 退出多选 */
void Task_ExitMultiMode(bool rebuild);
/** @brief 退出多选（可清状态） */
void Task_ExitMultiModeEx(bool rebuild, bool clear_status);
/** @brief 重建当前页列表 */
void Task_RebuildListPage();
/** @brief 显示单条操作对话框 */
void Task_ShowActionDialog(int idx);
/** @brief 确保操作对话框已创建 */
void Task_EnsureActionDialog();
/** @brief 调度删除指定下标 */
void Task_ScheduleDelete(int idx);
/** @brief 调度完成指定下标 */
void Task_ScheduleComplete(int idx);
/** @brief 刷新 Tab 选中外观 */
void Task_RefreshTabUi();
/** @brief HTTP JSON 请求 */
bool Task_HttpJson(const char* method, const std::string& url, std::string& body_out, std::string& err_out);
/** @brief 创建 Tab 按钮 */
lv_obj_t* Task_MakeTabBtn(lv_obj_t* parent, const char* text, int tab);
/** @brief 请求重建列表页 */
void Task_RequestRebuildListPage();
/** @brief 请求合并刷新勾选绘制 */
void Task_RequestCheckMarksPaint();
