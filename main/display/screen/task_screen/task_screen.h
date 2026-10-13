#pragma once

#include "lvgl.h"

// 每日清单页：仅展示 checklist_cache，避免在页面中重复拉取数据。
class TaskScreen {
public:
    /** @brief 创建每日清单页 */
    static lv_obj_t* Create();

    /** @brief 当前页面是否为每日清单页 */
    static bool IsActive();

    /** @brief 后台刷新 checklist_cache（不要求前台） */
    static void RequestCacheRefresh();

    /** @brief 联网成功后同步 checklist_cache */
    static void EnsureCacheSynced();

    /** @brief 从待机恢复后，若仍在清单页则从缓存重刷 */
    static void OnResumeFromStandby();
};
