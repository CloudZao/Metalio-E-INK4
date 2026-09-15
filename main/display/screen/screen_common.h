#pragma once

#include "lvgl.h"

#include <atomic>

// 通用状态栏信息，包含网络、电量和通知文本。
struct EpdStatusBar {
    lv_obj_t* bar = nullptr;
    lv_obj_t* overlay = nullptr;
    lv_obj_t* network_label = nullptr;
    lv_obj_t* mute_label = nullptr;
    lv_obj_t* battery_pct_label = nullptr;
    lv_obj_t* battery_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* notification_label = nullptr;
    lv_obj_t* low_battery_popup = nullptr;
    lv_coord_t height = 0;
};

// 通用顶栏和占位页辅助接口。
EpdStatusBar ScreenCreateStatusBar(lv_obj_t* scr);
lv_obj_t* ScreenCreatePlaceholder(const char* screen_id, const char* title);
void ScreenLoadReplace(lv_obj_t* new_scr);
void ScreenNavigateTo(lv_obj_t* (*create)());
void ScreenNavigateBack();
void ScreenSetIsHome(bool is_home);
bool ScreenIsHome();
void ScreenGoHome();
void ScreenRequestHome();
void ScreenRequestBack();

// 非 LVGL 线程投递 UI 任务，避免在触摸/墨水刷屏链路上直接持锁。
bool ScreenLvAsync(void (*cb)(void*), void* user_data = nullptr);
bool ScreenLvAsyncUrgent(void (*cb)(void*), void* user_data = nullptr);

// 多次重绘请求合并，减少无效刷新。
struct ScreenPaintCoalesce {
    std::atomic<uint32_t> seq{0};
    std::atomic<uint32_t> done{0};
    std::atomic<bool> queued{false};
    void (*paint)() = nullptr;
    void* defer_timer = nullptr;
};

void ScreenPaintCoalesceRequest(ScreenPaintCoalesce* c);
void ScreenPaintCoalesceRequestDebounced(ScreenPaintCoalesce* c, uint64_t delay_us);
void ScreenPaintCoalesceReset(ScreenPaintCoalesce* c);

// 页面外层的点状装饰底纹。
void ScreenApplyDotBackdrop(lv_obj_t* obj);
