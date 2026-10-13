#pragma once

#include "lvgl.h"

#include "fontpack_lvgl.h"

#include <cstdint>

constexpr lv_coord_t kSettingsBodyPad = 10; // 设置页内容区内边距
constexpr lv_coord_t kSettingsTabW = 120; // 侧栏 Tab 宽（容纳英文长词）
constexpr lv_coord_t kSettingsTabH = 56; // 侧栏 Tab 高
constexpr lv_coord_t kSettingsTabGap = 8; // Tab 间距
constexpr lv_coord_t kSettingsSplitLineW = 2; // 分区分割线宽
constexpr lv_coord_t kSettingsContentPad = 14; // 右侧内容区内边距
constexpr lv_coord_t kSettingsOptionH = 64; // 可选中选项行高
constexpr lv_coord_t kSettingsOptionGap = 12; // 选项间距
constexpr lv_coord_t kSettingsBorderW = 2; // 描边宽度

/** @brief 可选中按钮外观：选中黑底白字 / 未选中白底黑字描边 */
void SettingsStyleSelectable(lv_obj_t* btn, lv_obj_t* lbl, bool selected);

/** @brief 创建整行可选中选项（网络 / 主题等） */
lv_obj_t* SettingsCreateSelectableOption(lv_obj_t* parent, const char* title, lv_event_cb_t cb,
                                         intptr_t user_data);
