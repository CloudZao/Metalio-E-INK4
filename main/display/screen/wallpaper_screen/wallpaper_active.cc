#include "wallpaper_screen/wallpaper_active.h"

#include "SdCardManager.hpp"
#include "screen_common.h"
#include "sd_paths.h"
#include "settings.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <sys/stat.h>
#include "assets/lang_config.h"

namespace wallpaper {
namespace {

constexpr const char* TAG = "WpActive";
constexpr const char* kNvsNs = "wallpaper";
constexpr const char* kNvsKeyShutdown = "shutdown";
constexpr const char* kNvsKeyStandby = "standby";
constexpr size_t kNameMax = 95;

enum class Slot : uint8_t { Shutdown = 0, Standby = 1, Count = 2 };

char s_names[static_cast<size_t>(Slot::Count)][kNameMax + 1] = {};
std::atomic<bool> s_cache_ready{false};
std::atomic<bool> s_hydrate_busy{false};

struct HydrateWork {
    HydrateDoneFn done = nullptr;
    void* user = nullptr;
};

const char* NvsKeyFor(Slot slot) {
    return slot == Slot::Standby ? kNvsKeyStandby : kNvsKeyShutdown;
}

const char* SlotTag(Slot slot) {
    return slot == Slot::Standby ? "standby" : "shutdown";
}

bool FileExistsRegular(const char* path) {
    struct stat st {};
    return path != nullptr && stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

std::string AbsPathForName(const std::string& name) {
    std::string path = SD_PATH_WALLPAPER;
    if (!path.empty() && path.back() != '/') {
        path.push_back('/');
    }
    path.append(name);
    return path;
}

// 仅允许单段、无路径穿越的文件名。
bool IsSafeWallpaperBasename(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    const size_t n = std::strlen(name);
    if (n > kNameMax) {
        return false;
    }
    if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (c < 0x20 || c == 0x7F) {
            return false;
        }
        if (c == '/' || c == '\\' || c == ':' || c == '\0') {
            return false;
        }
    }
    return true;
}

std::string BasenameOf(const char* path_or_name) {
    if (path_or_name == nullptr || path_or_name[0] == '\0') {
        return {};
    }
    const char* slash = std::strrchr(path_or_name, '/');
    const char* base = slash != nullptr ? slash + 1 : path_or_name;
    if (!IsSafeWallpaperBasename(base)) {
        return {};
    }
    return base;
}

void SetSlotCache(Slot slot, const char* name) {
    const size_t i = static_cast<size_t>(slot);
    if (name == nullptr || name[0] == '\0') {
        s_names[i][0] = '\0';
    } else {
        std::snprintf(s_names[i], sizeof(s_names[i]), "%s", name);
    }
}

void MarkCacheReady() {
    s_cache_ready.store(true, std::memory_order_release);
}

void PersistSlotToNvs(Slot slot, const char* name) {
    Settings settings(kNvsNs, true);
    const char* key = NvsKeyFor(slot);
    if (name == nullptr || name[0] == '\0') {
        settings.EraseKey(key);
    } else {
        settings.SetString(key, name);
    }
}

// 读取 NVS 值并做合法性校验；非法值返回空并要求清理。
std::string ReadSlotNameFromNvs(Slot slot, bool* should_erase) {
    if (should_erase != nullptr) {
        *should_erase = false;
    }
    Settings settings(kNvsNs, false);
    const std::string name = settings.GetString(NvsKeyFor(slot), "");
    if (name.empty()) {
        return {};
    }
    if (!IsSafeWallpaperBasename(name.c_str())) {
        if (should_erase != nullptr) {
            *should_erase = true;
        }
        return {};
    }
    return name;
}

void LoadAllFromNvsIntoCache() {
    // 先灌入两槽；若 SD 未挂载则延迟标记缓存已就绪。
    bool defer_ready = false;
    for (uint8_t s = 0; s < static_cast<uint8_t>(Slot::Count); ++s) {
        const Slot slot = static_cast<Slot>(s);
        bool erase = false;
        const std::string name = ReadSlotNameFromNvs(slot, &erase);
        if (erase) {
            PersistSlotToNvs(slot, nullptr);
            SetSlotCache(slot, "");
            continue;
        }
        if (name.empty()) {
            SetSlotCache(slot, "");
            continue;
        }
        const std::string path = AbsPathForName(name);
        if (FileExistsRegular(path.c_str())) {
            SetSlotCache(slot, name.c_str());
            ESP_LOGI(TAG, "hydrate %s=%s", SlotTag(slot), name.c_str());
            continue;
        }
        if (SdCardManager::GetInstance().IsMounted()) {
            ESP_LOGW(TAG, "hydrate %s NVS=%s missing on SD → clear", SlotTag(slot), name.c_str());
            PersistSlotToNvs(slot, nullptr);
            SetSlotCache(slot, "");
        } else {
            SetSlotCache(slot, "");
            defer_ready = true;
            ESP_LOGW(TAG, "hydrate SD unmounted, keep NVS %s=%s", SlotTag(slot), name.c_str());
        }
    }
    if (!defer_ready) {
        MarkCacheReady();
    }
}

void InvokeDoneAsync(HydrateDoneFn done, void* user) {
    if (done == nullptr) {
        return;
    }
    auto* cb = new HydrateWork{done, user};
    if (!ScreenLvAsync(
            [](void* p) {
                auto* c = static_cast<HydrateWork*>(p);
                if (c->done != nullptr) {
                    c->done(c->user);
                }
                delete c;
            },
            cb)) {
        delete cb;
    }
}

void HydrateTask(void* arg) {
    auto* work = static_cast<HydrateWork*>(arg);
    LoadAllFromNvsIntoCache();
    const HydrateDoneFn done = work != nullptr ? work->done : nullptr;
    void* user = work != nullptr ? work->user : nullptr;
    delete work;
    s_hydrate_busy.store(false, std::memory_order_release);
    InvokeDoneAsync(done, user);
    vTaskDelete(nullptr);
}

void WaitThenDoneTask(void* arg) {
    auto* work = static_cast<HydrateWork*>(arg);
    for (int i = 0; i < 200 && !s_cache_ready.load(std::memory_order_acquire); ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    const HydrateDoneFn done = work != nullptr ? work->done : nullptr;
    void* user = work != nullptr ? work->user : nullptr;
    delete work;
    InvokeDoneAsync(done, user);
    vTaskDelete(nullptr);
}

std::string GetSlotFilename(Slot slot) {
    if (!s_cache_ready.load(std::memory_order_acquire)) {
        return {};
    }
    const char* name = s_names[static_cast<size_t>(slot)];
    if (name[0] == '\0') {
        return {};
    }
    // 文件可能已被删；此处只返回空，不直接清理 NVS。
    const std::string path = AbsPathForName(name);
    if (!FileExistsRegular(path.c_str())) {
        return {};
    }
    return name;
}

bool SetSlotFromFile(Slot slot, const char* path_or_name, std::string& err_out) {
    err_out.clear();
    const std::string name = BasenameOf(path_or_name);
    if (name.empty()) {
        err_out = Lang::Strings::WALLPAPER_NAME_INVALID;
        return false;
    }

    const std::string src = AbsPathForName(name);
    const std::string prefix = std::string(SD_PATH_WALLPAPER) + "/";
    if (src.rfind(prefix, 0) != 0) {
        err_out = Lang::Strings::WALLPAPER_PATH_INVALID;
        return false;
    }
    if (!FileExistsRegular(src.c_str())) {
        err_out = Lang::Strings::WALLPAPER_FILE_MISSING;
        return false;
    }

    // 若尚未 hydrate，先灌入两槽再更新当前槽，避免覆盖另一槽状态。
    if (!s_cache_ready.load(std::memory_order_acquire)) {
        LoadAllFromNvsIntoCache();
    }
    SetSlotCache(slot, name.c_str());
    MarkCacheReady();
    PersistSlotToNvs(slot, name.c_str());
    ESP_LOGI(TAG, "%s wallpaper -> %s (NVS only)", SlotTag(slot), name.c_str());
    return true;
}

void ClearSlot(Slot slot) {
    if (!s_cache_ready.load(std::memory_order_acquire)) {
        LoadAllFromNvsIntoCache();
    }
    SetSlotCache(slot, "");
    MarkCacheReady();
    PersistSlotToNvs(slot, nullptr);
    ESP_LOGI(TAG, "cleared %s wallpaper (user)", SlotTag(slot));
}

void ClearSlotIfMatches(Slot slot, const char* filename, bool* any_cleared) {
    if (filename == nullptr || filename[0] == '\0') {
        return;
    }

    bool match = false;
    if (s_cache_ready.load(std::memory_order_acquire)) {
        match = (std::strcmp(s_names[static_cast<size_t>(slot)], filename) == 0);
    }
    // 再对一下 NVS 原文，避免只清缓存漏清 NVS
    {
        Settings settings(kNvsNs, false);
        if (settings.GetString(NvsKeyFor(slot), "") == filename) {
            match = true;
        }
    }
    if (!match) {
        return;
    }

    SetSlotCache(slot, "");
    PersistSlotToNvs(slot, nullptr);
    ESP_LOGI(TAG, "cleared %s wallpaper (%s)", SlotTag(slot), filename);
    if (any_cleared != nullptr) {
        *any_cleared = true;
    }
}

bool TryResolveSlotPath(Slot slot, char* out_path, size_t out_len) {
    if (out_path == nullptr || out_len < 8) {
        return false;
    }
    out_path[0] = '\0';

    std::string name;
    const char* cached = s_names[static_cast<size_t>(slot)];
    if (s_cache_ready.load(std::memory_order_acquire) && cached[0] != '\0' &&
        IsSafeWallpaperBasename(cached)) {
        name = cached;
    } else {
        bool erase = false;
        name = ReadSlotNameFromNvs(slot, &erase);
        // 解析路径：不在这里 erase（瞬时失败不应丢配置）；仅 hydrate 清脏
        (void)erase;
    }
    if (name.empty() || !IsSafeWallpaperBasename(name.c_str())) {
        return false;
    }

    const std::string path = AbsPathForName(name);
    if (path.size() + 1 > out_len) {
        ESP_LOGW(TAG, "%s path too long", SlotTag(slot));
        return false;
    }
    if (!FileExistsRegular(path.c_str())) {
        ESP_LOGW(TAG, "%s path missing: %s", SlotTag(slot), path.c_str());
        return false;
    }
    std::snprintf(out_path, out_len, "%s", path.c_str());
    return true;
}

}  // namespace

std::string GetActiveFilename() {
    return GetSlotFilename(Slot::Shutdown);
}

std::string GetStandbyFilename() {
    return GetSlotFilename(Slot::Standby);
}

bool IsActiveFilename(const char* filename) {
    if (filename == nullptr || filename[0] == '\0') {
        return false;
    }
    const std::string active = GetActiveFilename();
    return !active.empty() && active == filename;
}

bool IsStandbyFilename(const char* filename) {
    if (filename == nullptr || filename[0] == '\0') {
        return false;
    }
    const std::string active = GetStandbyFilename();
    return !active.empty() && active == filename;
}

bool SetActiveFromFile(const char* path_or_name, std::string& err_out) {
    return SetSlotFromFile(Slot::Shutdown, path_or_name, err_out);
}

bool SetStandbyFromFile(const char* path_or_name, std::string& err_out) {
    return SetSlotFromFile(Slot::Standby, path_or_name, err_out);
}

void ClearShutdownWallpaper() {
    ClearSlot(Slot::Shutdown);
}

void ClearStandbyWallpaper() {
    ClearSlot(Slot::Standby);
}

void ClearActiveIfMatches(const char* filename) {
    // 删除文件后，同步清理所有指向它的启用项。
    const bool was_ready = s_cache_ready.load(std::memory_order_acquire);
    bool any = false;
    ClearSlotIfMatches(Slot::Shutdown, filename, &any);
    ClearSlotIfMatches(Slot::Standby, filename, &any);
    // 未 hydrate 时不要标记 ready，以免把另一槽状态覆盖为空。
    if (any && was_ready) {
        MarkCacheReady();
    }
}

bool TryResolveActiveWallpaperPath(char* out_path, size_t out_len) {
    return TryResolveSlotPath(Slot::Shutdown, out_path, out_len);
}

bool TryResolveStandbyWallpaperPath(char* out_path, size_t out_len) {
    return TryResolveSlotPath(Slot::Standby, out_path, out_len);
}

void RequestHydrateFromNvs(HydrateDoneFn done, void* user) {
    if (s_cache_ready.load(std::memory_order_acquire)) {
        InvokeDoneAsync(done, user);
        return;
    }

    auto* work = new HydrateWork{done, user};
    if (s_hydrate_busy.exchange(true, std::memory_order_acq_rel)) {
        if (xTaskCreate(WaitThenDoneTask, "wp_hyd_wait", 3072, work, 5, nullptr) != pdPASS) {
            delete work;
            InvokeDoneAsync(done, user);
        }
        return;
    }

    if (xTaskCreate(HydrateTask, "wp_hydrate", 4096, work, 5, nullptr) != pdPASS) {
        delete work;
        s_hydrate_busy.store(false, std::memory_order_release);
        ESP_LOGE(TAG, "hydrate task create failed");
        InvokeDoneAsync(done, user);
    }
}

void HydrateFromNvsNow() {
    if (s_cache_ready.load(std::memory_order_acquire)) {
        return;
    }
    if (s_hydrate_busy.exchange(true, std::memory_order_acq_rel)) {
        for (int i = 0; i < 200 && !s_cache_ready.load(std::memory_order_acquire); ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        return;
    }
    LoadAllFromNvsIntoCache();
    s_hydrate_busy.store(false, std::memory_order_release);
}

}  // namespace wallpaper
