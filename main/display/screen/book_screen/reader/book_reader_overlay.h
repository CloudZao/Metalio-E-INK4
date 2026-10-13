#pragma once

#include <cstdint>

#include <lvgl.h>

constexpr int64_t kFontSuppressRowClickUs = 1200000;  // 设置卡刷行更慢，松手假点击窗更宽
constexpr lv_coord_t kTocCoverW = 194;  // 目录页封面（约原 216×0.9）
constexpr lv_coord_t kTocCoverH = 259;
constexpr lv_coord_t kTocHeadGap = 16;  // 封面与书名间距
constexpr lv_coord_t kTocRowH = 44;     // 目录章行（底部分隔线）
constexpr lv_coord_t kTocRowGap = 6;    // 字号列表行距；目录列表用底线不用此值
constexpr lv_coord_t kTocRowPad = 6;
constexpr lv_coord_t kTocHeadListGap = 12; // 书头与章节列表间距
constexpr lv_coord_t kTocBelowStatus = 12; // 封面与顶栏之间留白
constexpr lv_coord_t kTocLineThin = 1;     // 章间分隔线
constexpr lv_coord_t kTocLineCur = 3;      // 当前章上下分隔线
constexpr lv_coord_t kSheetSidePad = 12;      // 悬浮设置卡片左右边距
constexpr lv_coord_t kSheetInnerPad = 12;
constexpr lv_coord_t kSheetGap = 10;
constexpr lv_coord_t kSheetIconBtn = 36;
constexpr lv_coord_t kSheetRadius = 16;
constexpr lv_coord_t kSheetBorderW = 2;       // 卡片描边（加粗）
constexpr lv_coord_t kFontListRowH = 44;     // 与目录行高一致，列表总高固定
constexpr int kTtfConvertSizeMin = 20;
constexpr int kTtfConvertSizeMax = 40;
constexpr int kTtfConvertSizeDefault = 25;

/** @brief 沉浸阅读：默认藏顶栏，正文占满状态栏区域 */
void SetReadStatusVisible(bool visible);
/** @brief 状态栏中央（时钟位）：有文案则显示，nullptr/空则藏起 */
void SetReadStatusCenterTitle(const char* title);
/** @brief 显示/隐藏排版设置卡 */
void SetReadSettingsSheetVisible(bool visible);
/** @brief 设置卡片打开时点正文只收起，不走分区动作 */
void HideReadChrome();
/** @brief 显示排版设置卡 */
void ShowReadSettingsSheet();
/** @brief 打开目录页 */
void OpenReadToc();
/** @brief 异步隐藏阅读顶栏/设置等 chrome */
void AsyncHideReadChrome(void* user_data);
/** @brief BOOT 短按：正文弹出/收起悬浮排版卡片（须 LVGL 任务，禁止 touch_feed 同步改树）。 */
void ToggleReadSettingsSheetAsync(void* user_data);
