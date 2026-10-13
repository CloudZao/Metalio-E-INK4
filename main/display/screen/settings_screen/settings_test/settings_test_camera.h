#pragma once

#include "lvgl.h"

/**
 * @brief 自动测试「摄像头」行
 * @note 进页探测 UVC（地址+分辨率 ID）；点击行或「拍照」抓一帧浮层显示（无实时预览）
 */
void SettingsTestCamera_BuildRow(lv_obj_t* parent);
/** @brief 进页：探测 UVC */
void SettingsTestCamera_OnLoad();
/** @brief 离页：关浮层、释放帧 */
void SettingsTestCamera_Teardown();
