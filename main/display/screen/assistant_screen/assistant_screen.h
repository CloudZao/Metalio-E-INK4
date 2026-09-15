#pragma once

#include "lvgl.h"

// 百问 AI 页面：管理会话内容、PPT 状态和按键语义。
class AssistantScreen {
public:
    static lv_obj_t* Create();

    // 当前页面是否在前台；后台消息会被忽略。
    static bool IsActive();

    // PTT 是否仍保持按下；用于统一监听和松手判定。
    static bool IsPttHeld();

    // 状态栏是否显示录音波形。
    static bool IsPttWaveVisible();

    // 同步当前 PTT/监听状态到状态栏。
    static void SyncPttOverlay();

    // 异步打开页面；已在前台则忽略。
    static void RequestOpen();

    // 向会话流追加消息，需在持 LVGL 锁时调用。
    static void AddMessage(const char* role, const char* content);

    static void SetEmotion(const char* emotion);
};
