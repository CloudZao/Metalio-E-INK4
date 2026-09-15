#pragma once

#include "lvgl.h"

// 外置蓝牙音频模块控制页，使用 SimpleUart 与 BT 芯片交互。
class BluetoothScreen {
public:
    static void BuildInto(lv_obj_t* parent);
    static void ResetUi();
    static void OnActivated();
    static void OnDeactivated();
    // 开机默认应用模式 1，适用于 UART 初始化后、UI 未启动时场景。
    static void ApplyDefaultMode();
};
