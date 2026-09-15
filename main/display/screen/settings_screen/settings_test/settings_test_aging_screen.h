#pragma once

#include "lvgl.h"

/**
 * 设置 → 测试 → 老化测试
 *
 * 职责边界：
 * - UI：全屏计数（LVGL 定时器）
 * - 音频：EnsureAudio + 主循环 Schedule 播 clip；播完约 2.5s 后再隔 5s 循环
 * - 电源：DeviceState=AudioTesting 开 PA；PowerNeed::StandbyInhibit 挡无操作待机
 * - 马达：震 0.5s → 停 10s 循环；退页必关
 * - 门禁：IsActive() 供电源短按/BOOT/音量查询；盖板返回仍可退出
 */
class SettingsTestAgingScreen {
public:
    static lv_obj_t* Create();
    /** 老化页占用中（含进页后、退页 TearDown 前） */
    static bool IsActive();
};
