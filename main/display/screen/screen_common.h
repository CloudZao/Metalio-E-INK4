#pragma once

#include "lvgl.h"

#include <atomic>

struct EpdStatusBar {
    lv_obj_t* bar = nullptr; // 顶栏容器
    lv_obj_t* overlay = nullptr; // 顶栏覆盖层
    lv_obj_t* network_label = nullptr; // 网络图标/文案
    lv_obj_t* mute_label = nullptr; // 静音图标
    lv_obj_t* battery_pct_label = nullptr; // 电量百分比
    lv_obj_t* battery_label = nullptr; // 电池图标
    lv_obj_t* status_label = nullptr; // 中间状态文案
    lv_obj_t* notification_label = nullptr; // 通知文案
    lv_obj_t* low_battery_popup = nullptr; // 低电提示
    lv_coord_t height = 0; // 顶栏高度
};

/** @brief 创建通用状态栏并绑定控件 */
EpdStatusBar ScreenCreateStatusBar(lv_obj_t* scr);
/** @brief 替换加载新屏（勿在此 Reset detail_cover） */
void ScreenLoadReplace(lv_obj_t* new_scr);
/** @brief 导航到工厂函数创建的新屏 */
void ScreenNavigateTo(lv_obj_t* (*create)());
/** @brief 返回上一屏 */
void ScreenNavigateBack();
/** @brief 标记当前是否为首页 */
void ScreenSetIsHome(bool is_home);
/** @brief 当前是否为首页 */
bool ScreenIsHome();
/** @brief 立即回到首页 */
void ScreenGoHome();
/** @brief 异步请求回首页 */
void ScreenRequestHome();
/** @brief 异步请求返回上一屏 */
void ScreenRequestBack();

/** @brief 非 LVGL 线程投递 UI 任务 */
bool ScreenLvAsync(void (*cb)(void*), void* user_data = nullptr);
/** @brief 紧急投递 UI 任务（优先于普通队列） */
bool ScreenLvAsyncUrgent(void (*cb)(void*), void* user_data = nullptr);

struct ScreenPaintCoalesce {
    std::atomic<uint32_t> seq{0}; // 请求序号
    std::atomic<uint32_t> done{0}; // 已完成序号
    std::atomic<bool> queued{false}; // 是否已入队
    void (*paint)() = nullptr; // 实际绘制回调
    void* defer_timer = nullptr; // 防抖定时器
};

/** @brief 合并多次重绘请求 */
void ScreenPaintCoalesceRequest(ScreenPaintCoalesce* c);
/** @brief 停手后再刷：点间隙抬起后延迟一次全刷 */
void ScreenPaintCoalesceRequestDebounced(ScreenPaintCoalesce* c, uint64_t delay_us);
/** @brief 重置合并重绘状态 */
void ScreenPaintCoalesceReset(ScreenPaintCoalesce* c);

/** @brief 为对象应用点状装饰底纹 */
void ScreenApplyDotBackdrop(lv_obj_t* obj);
