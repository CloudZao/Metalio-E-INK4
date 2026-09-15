#pragma once

#include "lvgl.h"

// 壁纸管理界面：显示 SD 卡图片、预览并设置关机/待机壁纸。
class WallpaperScreen {
public:
    static lv_obj_t* Create();
};
