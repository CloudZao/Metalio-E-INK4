#pragma once

#include "task_screen_priv.h"

/** @brief 刷新可见行勾选标记 */
void Task_PatchVisibleRowCheckMarks();
/** @brief 点击待办/已办 Tab */
void Task_OnTabClicked(lv_event_t* e);
/** @brief 禁用对象滚动 */
void Task_DisableScroll(lv_obj_t* obj);
/** @brief 刷新 Tab 选中外观 */
void Task_RefreshTabUi();
/** @brief 按尺寸查找行内子控件 */
lv_obj_t* Task_FindRowChildBySize(lv_obj_t* row, lv_coord_t w, lv_coord_t h);
/** @brief 设置空态覆盖文案 */
void Task_SetEmptyOverride(const char* msg);
/** @brief 创建 Tab 按钮 */
lv_obj_t* Task_MakeTabBtn(lv_obj_t* parent, const char* text, int tab);
/** @brief 清单正文字体 */
const lv_font_t* Task_Font();
/** @brief 请求重建当前列表页 */
void Task_RequestRebuildListPage();
/** @brief 条目是否已完成 */
bool Task_ItemIsDone(const TaskItem& item);
/** @brief 正文字体行高 */
lv_coord_t Task_FontLineH();
/** @brief 设置 Tab 按钮选中样式 */
void Task_StyleTabBtn(lv_obj_t* btn, lv_obj_t* lbl, bool selected);
/** @brief 多选已选数量 */
int Task_SelectedCount();
/** @brief 隐藏操作对话框 */
void Task_HideActionDialog();
/** @brief 刷新单行勾选标记 */
void Task_PatchRowCheckMark(int index);
/** @brief 格式化计划日期时间文案 */
void Task_FormatPlanMeta(char* out, size_t out_sz, const TaskItem& item);
/** @brief 切换条目多选状态 */
void Task_ToggleItemSelected(int idx);
/** @brief 列表总页数 */
int Task_ListPageCount();
/** @brief 请求合并刷新勾选绘制 */
void Task_RequestCheckMarksPaint();
/** @brief 当前 Tab 过滤后的条目数 */
int Task_FilteredCount();
/** @brief 条目是否被多选选中 */
bool Task_ItemSelected(int idx);
/** @brief 设置状态栏提示（可选自动清除） */
void Task_SetStatusText(const char* text, bool auto_clear);
/** @brief 重建当前页列表行 */
void Task_RebuildListPage();
/** @brief 取消状态栏自动清除定时器 */
void Task_CancelStatusClearTimer();
/** @brief 按计划时间新→旧排序 */
void Task_SortItemsByPlanDesc(std::vector<TaskItem>& items);
/** @brief 两行行高估算（标题+计划） */
lv_coord_t Task_EstimatedTwoLineRowH();
/** @brief 清单标题字体 */
const lv_font_t* Task_TitleFont();
/** @brief 条目是否属于当前 Tab */
bool Task_ItemInCurrentTab(const TaskItem& item);
/** @brief 同步多选数组尺寸与条目数 */
void Task_SyncSelectedSize();
/** @brief 条目是否有计划时间 */
bool Task_ItemHasPlan(const TaskItem& item);
/** @brief 按全局下标查找列表行 */
lv_obj_t* Task_FindListRow(int index);
/** @brief 刷新状态标签 */
void Task_RefreshStatusLbl();
/** @brief 刷新页码标签 */
void Task_RefreshPageLbl();
