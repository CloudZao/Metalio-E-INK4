#pragma once

#include "lvgl.h"

// 待机壁纸 UI：全屏显示 NVS「standby」指向的 A2I1（经 L8，与壁纸预览同路径，避本板 I1 白底）。
// 仅由 StandbyScreen 入口在缓存判定有可用待机壁纸时调用。
namespace standby_wallpaper {

/** 在 root 上构建白底 + 图片控件；不读 NVS。 */
void Build(lv_obj_t* root);

/**
 * 后台解析待机壁纸路径并解码，经 lv_async 挂到图片。
 * 须在 Build 之后调用；Dismiss/Teardown 后结果按 epoch 丢弃。
 */
void StartLoad();

/** 解码完成（成功或失败）；Park 前查询，勿固定等 settle。 */
bool IsContentReady();

/** 拆掉图片引用并释放像素；可重复调用。 */
void Teardown();

}  // namespace standby_wallpaper
