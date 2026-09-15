#pragma once

#include "lvgl.h"

// 每日清单页：仅展示 checklist_cache，避免在页面中重复拉取数据。
class TaskScreen {
public:
    static lv_obj_t* Create();

    // 当前页面是否为每日清单页。
    static bool IsActive();

    // 后台刷新 checklist_cache，不要求当前页一定处于前台。
    static void RequestCacheRefresh();

    // 在联网成功后同步 checklist_cache。
    static void EnsureCacheSynced();

    // 从缓存重载当前页面列表，适用于无网络状态。
    static void ReloadFromCache();

    // 从待机恢复后，若仍在清单页则重新从缓存刷新。
    static void OnResumeFromStandby();
};
