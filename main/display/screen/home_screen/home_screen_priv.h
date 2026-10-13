#pragma once

#include "home_screen.h"

#include <cstdint>

#include <lvgl.h>

#define HOME_STATUS_BAR_H 45
#define HOME_APPS_PAD_TOP 16
#define HOME_APPS_PAD_BOT 16
#define HOME_APPS_LIFT 48
#define HOME_APP_CARD_W 116
#define HOME_APP_CARD_H 136
#define HOME_APP_CARD_MARGIN_H 38
#define HOME_APP_CARD_GAP_COL 28
#define HOME_APP_CARD_GAP_ROW 41
#define HOME_APP_CARD_ICON_PAD_TOP 10
#define HOME_APP_CARD_NAME_PAD_BOT 10
#define HOME_APP_CARD_BORDER_W 4
#define HOME_PAGE_INDICATOR_H 36
#define HOME_APPS_VISIBLE_ROWS 2
#define HOME_SLASH_ROWS 3
#define HOME_SLASH_SIDE_PAD 16
#define HOME_SLASH_ROW_GAP 10
#define HOME_SLASH_BOTTOM_PAD HOME_SLASH_ROW_GAP
#define HOME_SLASH_SLANT 22
#define HOME_SLASH_COL_GAP HOME_SLASH_ROW_GAP
#define HOME_SLASH_ICON 70
#define HOME_SLASH_PAD_OUTER 18
#define HOME_SLASH_PAD_V 8
#define HOME_SLASH_PAD_SLANT 10
#define HOME_SLASH_PAD_COL 14
#define HOME_SLASH_NAME_NUDGE_R 12
#define HOME_SLASH_BORDER_W 1
#define HOME_APP_CARD_STYLE_DEFAULT HomeScreen::kCardStyleBorder

constexpr const char* TAG = "HomeScreen";
constexpr const char* kScreenId = "home";
constexpr lv_coord_t kIconSize = 70;
constexpr lv_coord_t kCardRadius = 20;
constexpr int kColsPerRow = 3;
constexpr int kRowsPerPage = 4;
constexpr int kAppsPerPage = kColsPerRow * kRowsPerPage;
constexpr int kDitherW = 8;
constexpr int kDitherH = 8;
constexpr uint8_t kBayer4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};
constexpr int kBayerThreshold = 2;

struct AppEntry {
    const char* icon_path;
    const char* (*name)();
    lv_obj_t* (*create)();
};

extern uint8_t Home_gray_dither_l8[kDitherW * kDitherH];
extern lv_image_dsc_t Home_gray_dither_img;
extern bool Home_gray_dither_ready;
extern const AppEntry kApps[];
extern const AppEntry kSlashApps[];
extern const int kTotalApps;
extern const int kSlashAppCount;
extern lv_obj_t* Home_apps;
extern lv_obj_t* Home_page_lbl;
extern lv_obj_t* Home_scr;
extern int Home_page;
extern bool Home_slash_layout;
extern lv_coord_t Home_slash_apps_h;
extern int Home_card_style;
extern bool Home_card_style_ready;

/** @brief 灰网点纹理图 */
const lv_image_dsc_t* Home_GrayDitherImg();
/** @brief 按样式设置应用卡片外观 */
void Home_ApplyCardStyle(lv_obj_t* cell, int style);
/** @brief 应用名：清单 */
const char* Home_AppNameTask();
/** @brief 应用名：百问 */
const char* Home_AppNameAssistant();
/** @brief 应用名：书库 */
const char* Home_AppNameBook();
/** @brief 应用名：壁纸 */
const char* Home_AppNameWallpaper();
/** @brief 应用名：云端 */
const char* Home_AppNameCloud();
/** @brief 应用名：设置 */
const char* Home_AppNameSettings();
/** @brief 创建经典布局应用格 */
lv_obj_t* Home_CreateAppCell(lv_obj_t* parent, const AppEntry& entry, int card_style);
/** @brief 创建斜切布局应用格 */
void Home_CreateSlashAppCell(lv_obj_t* row, const AppEntry& entry, int row_i, int col, lv_coord_t row_h,
                             lv_coord_t cell_w);
/** @brief 填充斜切布局应用页 */
void Home_FillSlashAppsPage();
