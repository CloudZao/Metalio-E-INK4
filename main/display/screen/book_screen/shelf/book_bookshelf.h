#pragma once

#include <cstdint>
#include <string>

#include <lvgl.h>

#include "reader/reader.h"

constexpr lv_coord_t kShelfMultiBtnH = 36;     // 底栏多选行（与页码同槽，不挤书籍）
constexpr lv_coord_t kShelfUnderlineH = 1;
constexpr lv_coord_t kShelfUnderlinePadHor = 5;
constexpr lv_coord_t kShelfMultiDotSize = 6;
constexpr lv_coord_t kShelfCheckSize = 24;     // 封面角勾选
constexpr int64_t kShelfSuppressRowClickUs = 400000; // 只挡长按行松手假 CLICKED
constexpr uint64_t kShelfCheckPaintDebounceUs = 280000;
constexpr lv_coord_t kShelfTitleGap = 6;
constexpr int kShelfCols = 3;
constexpr int kShelfRows = 3;             // 每页固定 3×3
constexpr lv_coord_t kShelfColGap = 12;
constexpr lv_coord_t kShelfRowGap = 16;    // 上下两排间距
constexpr lv_coord_t kShelfHeaderH = 40;   // 「我的书架」+ 封面/列表切换
constexpr lv_coord_t kShelfHeaderGap = 8;  // 标题行与内容区间距
constexpr lv_coord_t kShelfListRowH = 52;  // 列表行高（含底部分隔线）
constexpr lv_coord_t kShelfListIconW = 22;
constexpr lv_coord_t kShelfListColGap = 10;
constexpr lv_coord_t kShelfListTailGap = 8; // 文件名与右侧大小/> 间距
constexpr lv_coord_t kShelfViewToggleH = 34; // 切换外框高（原 30，仅放大）
constexpr lv_coord_t kShelfViewBtnW = 60;    // 单侧按钮宽（原 52）
constexpr lv_coord_t kShelfViewBtnH = 32;    // 单侧按钮高（原 28）

/** @brief 创建书架网格页 */
lv_obj_t* CreateBookshelfScreen();
/** @brief 渲染书架当前页 */
void RenderBookshelfPage();
/** @brief 异步打开书架 */
void OpenBookshelfAsync(void* user_data);

// 列表/删除/多选：见 book_shelf_ops.h
