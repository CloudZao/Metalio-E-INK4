#pragma once

#include "lvgl.h"

// 设置 → 功耗：空闲降频 / 保网时长 / 浅睡待机 / 自动关机（含不自动关机）
void SettingsPowerTab_Build(lv_obj_t* page);
void SettingsPowerTab_Reset();
