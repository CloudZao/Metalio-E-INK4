#pragma once

#include "lvgl.h"

// 待机 Overlay：叠加在当前页面之上，不销毁底层页面。
class StandbyScreen {
public:
    static lv_obj_t* Create();

    // 在当前 active screen 上叠加全屏待机页，需在 LVGL 线程调用。
    static void Show();

    // 关闭待机 Overlay，并恢复进入前页面。
    static void Dismiss();

    // Overlay 是否处于显示状态。
    static bool IsActive();

    // 待机内容是否已就绪，可开始渲染和刷屏。
    static bool IsPaintReady();

    // 待机 UI 就绪后，冻结 EPD flush 并全刷当前画面。
    static void EnterEpdSleep();

    // 退出待机前恢复 EPD flush；若未冻结则为空操作。
    static void ExitEpdSleep();

    // BOOT 短按：待机中忽略，不退出 Overlay。
    static bool HandleBootClick();

    // BOOT 长按：满阈值后摘掉 Overlay 并打开百问。
    static bool HandleBootLongPress();

    // 确保当日天气缓存存在；缺失时异步补全。
    static void EnsureWeatherCached();
};
