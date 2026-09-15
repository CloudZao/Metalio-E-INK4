#pragma once

#include "lvgl.h"

// WiFi 连接页：扫描周边网络、保存已连接网络并处理重连/清理。
class NetworkScreen {
public:
    static lv_obj_t* Create();
    static bool OnVkKey(const char* key_name);
};
