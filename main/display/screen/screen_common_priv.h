#pragma once

#include "screen_common.h"
#include "vk_key_handler.h"

#include "lv_adapter_display.h"

#include <cstdint>

constexpr const char* TAG = "ScreenCommon";
constexpr const char* kHomeScreen = "home"; // 首页屏名
constexpr int kMaxBack = 8; // 返回栈深度
constexpr lv_coord_t kStatusTextNudgeY = 4; // 顶栏文字垂直微调
constexpr uint64_t kPaintCoalescePendingFallbackUs = 120000; // 合并重绘兜底超时

extern bool ScreenCommon_on_home; // 是否在首页
extern ScreenFactory ScreenCommon_back_stack[kMaxBack]; // 返回栈
extern int ScreenCommon_back_depth; // 返回栈深度

/** @brief 清空返回栈 */
void ScreenCommon_ClearBackStack();
/** @brief 压入返回工厂 */
void ScreenCommon_PushBackFactory(ScreenFactory factory);
