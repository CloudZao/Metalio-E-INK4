#pragma once

#include "lvgl.h"

// 设置 → 存储：内存卡状态/容量 + 模拟 U 盘开关
void SettingsStorageTab_Build(lv_obj_t* page);
void SettingsStorageTab_Reset();
