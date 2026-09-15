#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 盖板 vk_prev / vk_next 长按连翻：按下后每 1s 连续翻 ±10 页。
typedef bool (*VkPageRepeatStepFn)(int page_delta);

// 停止当前连翻状态。
void VkPageRepeatStop(void);

// 若 key 是 vk_prev / vk_next，则启动连翻；达到边界时自动停止。
bool VkPageRepeatTryStart(const char* key, VkPageRepeatStepFn step);

// 松手时停止连翻，返回 true 表示已消费。
bool VkPageRepeatOnPressUp(const char* key);

bool VkPageRepeatIsActive(void);

#ifdef __cplusplus
}
#endif
