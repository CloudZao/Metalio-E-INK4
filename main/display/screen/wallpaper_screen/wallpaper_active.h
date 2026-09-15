#pragma once

#include <cstddef>
#include <string>

// 壁纸启用状态：NVS 仅保存文件名，UI 读取内存缓存。
// 不要在 LVGL 任务中访问 NVS；缓存由 hydrate / Set* 维护。
namespace wallpaper {

// 当前启用的关机壁纸名；未 hydrate 或文件缺失时返回空。
std::string GetActiveFilename();

// 当前启用的待机壁纸名；未 hydrate 或文件缺失时返回空。
std::string GetStandbyFilename();

bool IsActiveFilename(const char* filename);
bool IsStandbyFilename(const char* filename);

// 设置关机壁纸；仅在非 LVGL 任务中调用。
bool SetActiveFromFile(const char* path_or_name, std::string& err_out);

// 设置待机壁纸；仅在非 LVGL 任务中调用。
bool SetStandbyFromFile(const char* path_or_name, std::string& err_out);

// 清除关机壁纸启用状态，并恢复默认关机图。
void ClearShutdownWallpaper();

// 清除待机壁纸启用状态，并恢复默认待机页。
void ClearStandbyWallpaper();

// 删除壁纸时，同步清理所有引用该文件的启用状态。
void ClearActiveIfMatches(const char* filename);

// 解析当前启用的关机壁纸路径；文件存在时返回 true。
bool TryResolveActiveWallpaperPath(char* out_path, size_t out_len);

// 解析当前启用的待机壁纸路径；文件存在时返回 true。
bool TryResolveStandbyWallpaperPath(char* out_path, size_t out_len);

using HydrateDoneFn = void (*)(void* user);
// 从 NVS 重新灌入关机/待机缓存；已就绪时只回调。
void RequestHydrateFromNvs(HydrateDoneFn done, void* user);

// 同步灌入缓存；仅允许在非 LVGL 任务中调用。
void HydrateFromNvsNow();

}  // namespace wallpaper
