#pragma once

#include "lvgl.h"

#include "fontpack_lvgl.h"

#include <cstdint>


// 设置页共用布局常量（逻辑分辨率约 480x800）
constexpr lv_coord_t kSettingsBodyPad = 10;
// 英文 Language/Bluetooth 等长词；118 会裁字
constexpr lv_coord_t kSettingsTabW = 120;
constexpr lv_coord_t kSettingsTabH = 56;
constexpr lv_coord_t kSettingsTabGap = 8;
constexpr lv_coord_t kSettingsSplitLineW = 2;
constexpr lv_coord_t kSettingsContentPad = 14;
constexpr lv_coord_t kSettingsOptionH = 64;
constexpr lv_coord_t kSettingsOptionGap = 12;
constexpr lv_coord_t kSettingsBorderW = 2;

// 可选中按钮：黑底白字 / 白底黑字描边
void SettingsStyleSelectable(lv_obj_t* btn, lv_obj_t* lbl, bool selected);

// 整行可选中选项（网络 / 主题等）
lv_obj_t* SettingsCreateSelectableOption(lv_obj_t* parent, const char* title, lv_event_cb_t cb,
                                         intptr_t user_data);
