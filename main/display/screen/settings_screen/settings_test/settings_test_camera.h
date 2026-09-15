#pragma once

#include "lvgl.h"

/**
 * 自动测试「摄像头」行：
 * - 进页探测 UVC（地址+分辨率 ID）
 * - 点击行任意处 / 「拍照」按钮 → 抓一帧并浮层显示（无实时预览）
 */
void SettingsTestCamera_BuildRow(lv_obj_t* parent);
void SettingsTestCamera_OnLoad();
void SettingsTestCamera_Teardown();
