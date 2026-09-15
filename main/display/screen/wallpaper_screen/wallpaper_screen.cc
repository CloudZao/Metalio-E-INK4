#include "wallpaper_screen/wallpaper_screen.h"

#include "wallpaper_screen/wallpaper_active.h"

#include "cloud_screen/wallpaper_cloud_library.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "image_util.h"
#include "reader/file_size.h"
#include "reader/text_encoding.h"
#include "reader_types.h"
#include "screen_common.h"
#include "sd_paths.h"
#include "SdCardManager.hpp"
#include "vk_key_handler.h"
#include "vk_page_repeat.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include "assets/lang_config.h"

namespace {

constexpr const char* TAG = "WallpaperScreen";
constexpr const char* kScreenId = "wallpaper";
constexpr const char* kPosixDir = SD_PATH_WALLPAPER;

constexpr lv_coord_t kPad = 16;
constexpr lv_coord_t kBorderW = 2;
constexpr lv_coord_t kFooterH = 36;
constexpr lv_coord_t kActionH = 52;
constexpr lv_coord_t kDeleteH = 48;
constexpr lv_coord_t kPreviewGap = 12; // 设置页纵向间距
constexpr lv_coord_t kWpMultiBtnH = 36;
constexpr lv_coord_t kWpUnderlineH = 1;
constexpr lv_coord_t kWpUnderlinePadHor = 5;
constexpr lv_coord_t kWpMultiDotSize = 6;
constexpr lv_coord_t kWpCheckSize = 24;
constexpr int64_t kWpSuppressRowClickUs = 400000;
constexpr uint64_t kWpCheckPaintDebounceUs = 280000;
constexpr int kGridCols = 3;
constexpr int kGridRows = 3;
constexpr lv_coord_t kGridColGap = 16;
constexpr lv_coord_t kGridRowGap = 20;
constexpr lv_coord_t kCoverRadius = 12;
constexpr lv_coord_t kBtnRadius = 999; // 胶囊
constexpr lv_coord_t kGridFrameBorder = 1; // 网格缩略外框；图须缩进
constexpr int kMaxItems = 64;
// 预览和缩略使用 L8；不要直接用全屏 I1，容易出现白底。
constexpr int kPreviewMaxW = 280;
constexpr int kPreviewMaxH = 420;

// 扣除边框后保留可绘区域。
static inline int FrameInner(lv_coord_t outer, lv_coord_t border) {
    const lv_coord_t inset = 2 * border;
    return static_cast<int>(outer > inset ? outer - inset : 1);
}

enum class UiMode : uint8_t { kList = 0, kPreview = 1 };

struct FileEntry {
    char name[96] = {};
    char title[160] = {};
    char path[192] = {};
    size_t size_bytes = 0;
};

struct UiState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* list_body = nullptr;
    lv_obj_t* list_host = nullptr;
    lv_obj_t* list_empty = nullptr;
    lv_obj_t* page_lbl = nullptr; // 页码；多选时靠右下
    lv_obj_t* multi_bar = nullptr; // 取消 / 全选 / 移除
    lv_obj_t* status_label = nullptr;
    lv_obj_t* preview_body = nullptr;
    lv_obj_t* preview_title = nullptr;
    lv_obj_t* preview_meta = nullptr;
    lv_obj_t* preview_status = nullptr;
    lv_obj_t* preview_img = nullptr;
    lv_obj_t* preview_img_host = nullptr;
    lv_obj_t* enable_shutdown_btn = nullptr;
    lv_obj_t* enable_shutdown_lbl = nullptr;
    lv_obj_t* enable_standby_btn = nullptr;
    lv_obj_t* enable_standby_lbl = nullptr;
    lv_obj_t* delete_btn = nullptr;
    lv_obj_t* delete_lbl = nullptr;
};

UiState s_ui;
bool s_screen_alive = false;
bool s_sd_ready = false;
UiMode s_mode = UiMode::kList;
int s_list_page = 0;
int s_list_per_page = kGridCols * kGridRows;
int s_preview_idx = -1;
bool s_multi = false;
std::vector<uint8_t> s_selected;
int64_t s_suppress_click_until_us = 0;
int s_suppress_click_idx = -1;
lv_coord_t s_body_h = 0;
lv_coord_t s_grid_cell_w = 120;
lv_coord_t s_grid_cover_h = 160;
// 每次 Create / DELETE 递增，异步回调会带 epoch，避免过期结果访问 UAF。
uint32_t s_epoch = 0;

std::vector<FileEntry> s_files;
std::vector<std::unique_ptr<reader::RasterImage>> s_thumbs;
std::string s_shutdown_name;
std::string s_standby_name;
// 设置页的提示文案；为空时不绘制。
char s_delete_meta_hint[80] = {};

reader::RasterImage* s_preview_raster = nullptr;

std::atomic<bool> s_load_busy{false};
std::atomic<bool> s_thumb_busy{false};
std::atomic<bool> s_enable_busy{false};
std::atomic<bool> s_delete_busy{false};
TaskHandle_t s_load_task = nullptr;
TaskHandle_t s_thumb_task = nullptr;
TaskHandle_t s_enable_task = nullptr;
TaskHandle_t s_delete_task = nullptr;

void RebuildListPage();
void RebuildListPageInternal(bool schedule_thumbs);
void RequestWallpaperListRebuild();
void RequestWpCheckMarksPaint();
void RefreshWpFooterMode();
void ExitWpMultiMode(bool rebuild);
void EnterWpMultiModeSelect(int idx);
void PatchWpVisibleCheckMarks();
void ShowMode(UiMode mode);
void ClearPreviewImage();
void DetachRasterUsers();
void UpdateEnableButtonUi();
void UpdateDeleteButtonUi();
void ScheduleThumbFill();
void FillPageThumbsSync(int page);
void OnRowLongPressed(lv_event_t* e);
void OnWpMultiCancel(lv_event_t* e);
void OnWpMultiSelectAll(lv_event_t* e);
void OnWpMultiRemove(lv_event_t* e);
bool OnVkKey(const char* key);

const lv_font_t* UiFont() {
    return fontpack_lv_font_ui();
}

lv_coord_t ContentWidth() {
    return LV_HOR_RES - kPad * 2;
}

// 按像素尽量排成两行，避免长文件名导致首行留白。
std::string LayoutTitleTwoLines(const char* text, const lv_font_t* font, lv_coord_t max_w) {
    if (text == nullptr || text[0] == '\0') {
        return {};
    }
    if (font == nullptr || max_w <= 0) {
        return text;
    }

    const uint8_t* d = reinterpret_cast<const uint8_t*>(text);
    const size_t len = std::strlen(text);
    auto glyph_w = [font](uint32_t cp) -> lv_coord_t {
        lv_coord_t gw = static_cast<lv_coord_t>(lv_font_get_glyph_width(font, cp, 0));
        return gw > 0 ? gw : 1;
    };

    std::string line1;
    size_t i = 0;
    lv_coord_t used = 0;
    while (i < len) {
        uint32_t cp = 0;
        const size_t n = reader::Utf8Next(d + i, len - i, &cp);
        if (n == 0) {
            break;
        }
        const lv_coord_t gw = glyph_w(cp);
        if (used + gw > max_w) {
            break;
        }
        line1.append(text + i, n);
        used += gw;
        i += n;
    }
    if (i >= len) {
        return line1;
    }

    lv_coord_t rest_w = 0;
    for (size_t j = i; j < len;) {
        uint32_t cp = 0;
        const size_t n = reader::Utf8Next(d + j, len - j, &cp);
        if (n == 0) {
            break;
        }
        rest_w += glyph_w(cp);
        j += n;
    }
    if (rest_w <= max_w) {
        return line1 + "\n" + std::string(text + i);
    }

    constexpr const char* kEllipsis = "…";
    uint32_t ell_cp = 0;
    reader::Utf8Next(reinterpret_cast<const uint8_t*>(kEllipsis), 3, &ell_cp);
    const lv_coord_t budget = max_w - glyph_w(ell_cp);

    std::string line2;
    used = 0;
    while (i < len && budget > 0) {
        uint32_t cp = 0;
        const size_t n = reader::Utf8Next(d + i, len - i, &cp);
        if (n == 0) {
            break;
        }
        const lv_coord_t gw = glyph_w(cp);
        if (used + gw > budget) {
            break;
        }
        line2.append(text + i, n);
        used += gw;
        i += n;
    }
    line2.append(kEllipsis);
    return line1 + "\n" + line2;
}

void DisableScroll(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

bool EndsWithIgnoreCase(const char* name, const char* ext) {
    if (name == nullptr || ext == nullptr) {
        return false;
    }
    const size_t nlen = std::strlen(name);
    const size_t elen = std::strlen(ext);
    if (nlen < elen) {
        return false;
    }
    for (size_t i = 0; i < elen; ++i) {
        char a = name[nlen - elen + i];
        char b = ext[i];
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

bool IsListableFile(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) {
        return false;
    }
    if (EndsWithIgnoreCase(name, ".tmp")) {
        return false;
    }
    if (EndsWithIgnoreCase(name, ".meta.json")) {
        return false;
    }
    return true;
}

void RefreshActiveName() {
    s_shutdown_name = wallpaper::GetActiveFilename();
    s_standby_name = wallpaper::GetStandbyFilename();
}

// 将当前启用的待机/关机项排到前面，并按文件名稳定排序。
int ActivePinRank(const FileEntry& e) {
    const bool shutdown_on = !s_shutdown_name.empty() && s_shutdown_name == e.name;
    const bool standby_on = !s_standby_name.empty() && s_standby_name == e.name;
    if (standby_on && shutdown_on) {
        return 0;
    }
    if (standby_on) {
        return 1;
    }
    if (shutdown_on) {
        return 2;
    }
    return 3;
}

void SortFilesActiveFirst() {
    const size_t n = s_files.size();
    if (n <= 1) {
        return;
    }
    if (s_thumbs.size() != n) {
        s_thumbs.resize(n);
    }
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [](size_t a, size_t b) {
        const int ra = ActivePinRank(s_files[a]);
        const int rb = ActivePinRank(s_files[b]);
        if (ra != rb) {
            return ra < rb;
        }
        return std::strcmp(s_files[a].name, s_files[b].name) < 0;
    });
    bool changed = false;
    for (size_t i = 0; i < n; ++i) {
        if (order[i] != i) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        return;
    }
    std::vector<FileEntry> files(n);
    std::vector<std::unique_ptr<reader::RasterImage>> thumbs(n);
    std::vector<uint8_t> selected;
    const bool keep_sel = (s_selected.size() == n);
    if (keep_sel) {
        selected.resize(n);
    }
    for (size_t i = 0; i < n; ++i) {
        files[i] = s_files[order[i]];
        thumbs[i] = std::move(s_thumbs[order[i]]);
        if (keep_sel) {
            selected[i] = s_selected[order[i]];
        }
    }
    s_files = std::move(files);
    s_thumbs = std::move(thumbs);
    if (keep_sel) {
        s_selected = std::move(selected);
    }
}

void OnActiveHydrated(void* /*user*/) {
    if (!s_screen_alive) {
        return;
    }
    RefreshActiveName();
    SortFilesActiveFirst();
    if (s_mode == UiMode::kList) {
        RebuildListPage();
    } else {
        UpdateEnableButtonUi();
    }
}

void StyleActionBtn(lv_obj_t* btn, lv_obj_t* lbl, bool filled, bool enabled) {
    if (btn == nullptr) {
        return;
    }
    lv_obj_set_style_bg_color(btn, filled ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(btn, kBorderW, 0);
    lv_obj_set_style_radius(btn, kBtnRadius, 0);
    if (enabled) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_state(btn, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(btn, LV_STATE_DISABLED);
    }
    if (lbl != nullptr) {
        lv_obj_set_style_text_color(lbl, filled ? lv_color_white() : lv_color_black(), 0);
    }
}

void UpdatePreviewMeta() {
    if (!s_screen_alive || s_ui.preview_meta == nullptr) {
        return;
    }
    lv_obj_clean(s_ui.preview_meta);
    if (s_preview_idx < 0 || s_preview_idx >= static_cast<int>(s_files.size())) {
        return;
    }
    const FileEntry& fe = s_files[static_cast<size_t>(s_preview_idx)];
    const bool shutdown_on = !s_shutdown_name.empty() && s_shutdown_name == fe.name;
    const bool standby_on = !s_standby_name.empty() && s_standby_name == fe.name;

    // 删除状态放在文件大小左侧，不盖预览图
    const char* left_hint = nullptr;
    if (s_delete_busy.load()) {
        left_hint = Lang::Strings::WALLPAPER_DELETING;
    } else if (s_delete_meta_hint[0] != '\0') {
        left_hint = s_delete_meta_hint;
    }
    if (left_hint != nullptr) {
        lv_obj_t* hint = lv_label_create(s_ui.preview_meta);
        lv_label_set_text(hint, left_hint);
        lv_obj_set_style_text_font(hint, UiFont(), 0);
        lv_obj_set_style_text_color(hint, lv_color_black(), 0);
        lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
    }

    auto make_chip = [&](const char* text, bool filled) {
        lv_obj_t* chip = lv_obj_create(s_ui.preview_meta);
        lv_obj_remove_style_all(chip);
        lv_obj_set_height(chip, 32);
        lv_obj_set_width(chip, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(chip, 12, 0);
        lv_obj_set_style_border_width(chip, kBorderW, 0);
        lv_obj_set_style_border_color(chip, lv_color_black(), 0);
        lv_obj_set_style_radius(chip, kBtnRadius, 0);
        lv_obj_set_style_bg_color(chip, filled ? lv_color_black() : lv_color_white(), 0);
        lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
        lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(chip, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(chip);
        lv_obj_t* lbl = lv_label_create(chip);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, UiFont(), 0);
        lv_obj_set_style_text_color(lbl, filled ? lv_color_white() : lv_color_black(), 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    };

    char size_buf[24];
    reader::FormatFileSize(size_buf, sizeof(size_buf), fe.size_bytes);
    make_chip(size_buf, false);
    if (shutdown_on) {
        make_chip(Lang::Strings::WALLPAPER_CHIP_SHUTDOWN, true);
    }
    if (standby_on) {
        make_chip(Lang::Strings::WALLPAPER_CHIP_STANDBY, true);
    }
}

void UpdateEnableButtonUi() {
    if (!s_screen_alive) {
        return;
    }
    UpdatePreviewMeta();
    const bool ok_idx =
        s_preview_idx >= 0 && s_preview_idx < static_cast<int>(s_files.size());
    const char* name =
        ok_idx ? s_files[static_cast<size_t>(s_preview_idx)].name : "";
    const bool busy = s_enable_busy.load() || s_delete_busy.load() || s_load_busy.load();

    auto paint = [&](lv_obj_t* btn, lv_obj_t* lbl, bool is_on, const char* idle_text,
                     const char* on_text) {
        if (btn == nullptr || lbl == nullptr) {
            return;
        }
        if (!ok_idx) {
            lv_label_set_text(lbl, idle_text);
            StyleActionBtn(btn, lbl, false, false);
            return;
        }
        if (is_on) {
            lv_label_set_text(lbl, on_text);
            StyleActionBtn(btn, lbl, true, !busy);
        } else {
            lv_label_set_text(lbl, idle_text);
            StyleActionBtn(btn, lbl, false, !busy);
        }
    };

    const bool shutdown_on =
        ok_idx && !s_shutdown_name.empty() && s_shutdown_name == name;
    const bool standby_on =
        ok_idx && !s_standby_name.empty() && s_standby_name == name;
    paint(s_ui.enable_shutdown_btn, s_ui.enable_shutdown_lbl, shutdown_on, Lang::Strings::WALLPAPER_SET_SHUTDOWN,
          Lang::Strings::WALLPAPER_SHUTDOWN_ON);
    paint(s_ui.enable_standby_btn, s_ui.enable_standby_lbl, standby_on, Lang::Strings::WALLPAPER_SET_STANDBY,
          Lang::Strings::WALLPAPER_STANDBY_ON);
}

void UpdateDeleteButtonUi() {
    if (!s_screen_alive || s_ui.delete_btn == nullptr || s_ui.delete_lbl == nullptr) {
        return;
    }
    const bool ok_idx =
        s_preview_idx >= 0 && s_preview_idx < static_cast<int>(s_files.size());
    const bool busy = s_delete_busy.load() || s_enable_busy.load() || s_load_busy.load();
    StyleActionBtn(s_ui.delete_btn, s_ui.delete_lbl, false, ok_idx && !busy);
}

void CollectWallpapers() {
    std::vector<FileEntry> next_files;
    if (!s_sd_ready) {
        s_files.clear();
        s_thumbs.clear();
        return;
    }
    DIR* dir = opendir(kPosixDir);
    if (dir == nullptr) {
        ESP_LOGW(TAG, "opendir %s failed", kPosixDir);
        s_files.clear();
        s_thumbs.clear();
        return;
    }
    while (next_files.size() < static_cast<size_t>(kMaxItems)) {
        errno = 0;
        dirent* ent = readdir(dir);
        if (ent == nullptr) {
            break;
        }
        if (!IsListableFile(ent->d_name)) {
            continue;
        }
        const size_t name_len = std::strlen(ent->d_name);
        if (name_len == 0 || name_len >= sizeof(FileEntry::name)) {
            continue;
        }
        FileEntry entry;
        std::memcpy(entry.name, ent->d_name, name_len + 1);
        const int n = std::snprintf(entry.path, sizeof(entry.path), "%s/%s", kPosixDir, entry.name);
        if (n < 0 || static_cast<size_t>(n) >= sizeof(entry.path)) {
            continue;
        }
        struct stat st {};
        if (stat(entry.path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
            continue;
        }
        entry.size_bytes = static_cast<size_t>(st.st_size);
        std::string image_name;
        std::string original_filename = entry.name;
        (void)reader::ReadWallpaperMeta(entry.path, image_name, original_filename);
        const std::string title =
            reader::FormatWallpaperItemTitle(image_name, original_filename);
        std::snprintf(entry.title, sizeof(entry.title), "%s", title.c_str());
        next_files.push_back(entry);
    }
    closedir(dir);
    std::sort(next_files.begin(), next_files.end(),
              [](const FileEntry& a, const FileEntry& b) { return std::strcmp(a.name, b.name) < 0; });

    // 按文件名+大小复用已解码缩略图，避免每次进 app 清空再异步刷白
    std::vector<std::unique_ptr<reader::RasterImage>> next_thumbs(next_files.size());
    for (size_t i = 0; i < next_files.size(); ++i) {
        for (size_t j = 0; j < s_files.size(); ++j) {
            if (j >= s_thumbs.size()) {
                break;
            }
            if (std::strcmp(next_files[i].name, s_files[j].name) != 0 ||
                next_files[i].size_bytes != s_files[j].size_bytes) {
                continue;
            }
            next_thumbs[i] = std::move(s_thumbs[j]);
            break;
        }
    }
    s_files = std::move(next_files);
    s_thumbs = std::move(next_thumbs);
    SortFilesActiveFirst();
}

// 先确保本页缩略图完成，再刷屏，避免首帧白底和重复全刷。
void FillPageThumbsSync(int page) {
    if (!s_sd_ready || s_files.empty() || s_list_per_page <= 0) {
        return;
    }
    if (s_thumbs.size() != s_files.size()) {
        s_thumbs.resize(s_files.size());
    }
    if (page < 0) {
        page = 0;
    }
    const int start = page * s_list_per_page;
    if (start >= static_cast<int>(s_files.size())) {
        return;
    }
    const int end = std::min(start + s_list_per_page, static_cast<int>(s_files.size()));
    const int dw = FrameInner(s_grid_cell_w > 0 ? s_grid_cell_w : 120, kGridFrameBorder);
    const int dh = FrameInner(s_grid_cover_h > 0 ? s_grid_cover_h : 160, kGridFrameBorder);
    for (int i = start; i < end; ++i) {
        if (s_thumbs[static_cast<size_t>(i)] != nullptr &&
            !s_thumbs[static_cast<size_t>(i)]->empty()) {
            continue;
        }
        auto img = std::make_unique<reader::RasterImage>();
        if (!reader::DecodeImageFileToL8(s_files[static_cast<size_t>(i)].path, dw, dh, *img) ||
            img->empty()) {
            img->Reset();
        } else {
            img->BindDsc();
        }
        s_thumbs[static_cast<size_t>(i)] = std::move(img);
    }
}

int ListPageCount() {
    if (s_files.empty() || s_list_per_page <= 0) {
        return 1;
    }
    return (static_cast<int>(s_files.size()) + s_list_per_page - 1) / s_list_per_page;
}

void ClampListPage() {
    const int pages = ListPageCount();
    if (s_list_page < 0) {
        s_list_page = 0;
    }
    if (s_list_page >= pages) {
        s_list_page = pages - 1;
    }
}

// 先移除还在引用 RasterImage 的 lv_image，再释放像素缓冲。
void DetachRasterUsers() {
    if (s_ui.preview_img != nullptr) {
        lv_image_set_src(s_ui.preview_img, nullptr);
        lv_obj_add_flag(s_ui.preview_img, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.list_host != nullptr) {
        lv_obj_clean(s_ui.list_host);
        s_ui.list_empty = nullptr;
    }
}

void ClearPreviewImage() {
    if (s_ui.preview_img != nullptr) {
        lv_image_set_src(s_ui.preview_img, nullptr);
        lv_obj_add_flag(s_ui.preview_img, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_preview_raster != nullptr) {
        delete s_preview_raster;
        s_preview_raster = nullptr;
    }
}

struct LoadWork {
    int index = -1;
    uint32_t epoch = 0;
    char path[192] = {};
};

struct LoadResultMsg {
    int index = -1;
    uint32_t epoch = 0;
    bool ok = false;
    char err[80] = {};
    reader::RasterImage* img = nullptr;
};

void ApplyPreviewAsync(void* p) {
    auto* msg = static_cast<LoadResultMsg*>(p);
    s_load_busy.store(false);
    s_load_task = nullptr;
    if (msg == nullptr) {
        return;
    }
    if (!s_screen_alive || msg->epoch != s_epoch) {
        delete msg->img;
        delete msg;
        return;
    }
    if (s_mode != UiMode::kPreview || msg->index != s_preview_idx) {
        delete msg->img;
        delete msg;
        return;
    }

    ClearPreviewImage();
    if (!msg->ok || msg->img == nullptr || msg->img->empty()) {
        if (s_ui.preview_status != nullptr) {
            lv_label_set_text(s_ui.preview_status,
                              msg->err[0] != '\0' ? msg->err : Lang::Strings::WALLPAPER_PREVIEW_FAIL);
            lv_obj_clear_flag(s_ui.preview_status, LV_OBJ_FLAG_HIDDEN);
        }
        UpdateEnableButtonUi();
        UpdateDeleteButtonUi();
        delete msg->img;
        delete msg;
        return;
    }

    s_preview_raster = msg->img;
    msg->img = nullptr;
    s_preview_raster->BindDsc();

    if (s_ui.preview_img != nullptr) {
        lv_image_set_scale(s_ui.preview_img, LV_SCALE_NONE);
        lv_image_set_src(s_ui.preview_img, &s_preview_raster->dsc);
        lv_obj_set_size(s_ui.preview_img, s_preview_raster->width, s_preview_raster->height);
        lv_obj_clear_flag(s_ui.preview_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(s_ui.preview_img);
        lv_obj_invalidate(s_ui.preview_img);
        if (s_ui.preview_img_host != nullptr) {
            lv_obj_invalidate(s_ui.preview_img_host);
        }
        ESP_LOGI(TAG, "preview L8 %ux%u", s_preview_raster->width, s_preview_raster->height);
    }
    if (s_ui.preview_status != nullptr) {
        lv_obj_add_flag(s_ui.preview_status, LV_OBJ_FLAG_HIDDEN);
    }
    UpdateEnableButtonUi();
    UpdateDeleteButtonUi();
    delete msg;
}

void LoadPreviewTask(void* arg) {
    auto* work = static_cast<LoadWork*>(arg);
    auto* msg = new LoadResultMsg{};
    if (work == nullptr) {
        std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::WALLPAPER_FILE_INVALID);
    } else {
        msg->index = work->index;
        msg->epoch = work->epoch;
        auto* img = new reader::RasterImage();
        if (!reader::DecodeImageFileToL8(work->path, FrameInner(kPreviewMaxW, kBorderW),
                                         FrameInner(kPreviewMaxH, kBorderW), *img) ||
            img->empty()) {
            delete img;
            std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::WALLPAPER_DECODE_FAIL);
        } else {
            msg->ok = true;
            msg->img = img;
            ESP_LOGI(TAG, "preview decoded %ux%u from %s", img->width, img->height, work->path);
        }
        delete work;
    }

    if (lv_async_call(ApplyPreviewAsync, msg) != LV_RESULT_OK) {
        delete msg->img;
        delete msg;
        s_load_busy.store(false);
        s_load_task = nullptr;
    }
    vTaskDeleteWithCaps(nullptr);
}

void ScheduleLoadPreview(int index) {
    if (index < 0 || index >= static_cast<int>(s_files.size())) {
        ESP_LOGW(TAG, "preview skip: bad index=%d files=%u", index,
                 static_cast<unsigned>(s_files.size()));
        return;
    }
    if (s_load_busy.exchange(true)) {
        ESP_LOGW(TAG, "preview skip: load busy index=%d", index);
        return;
    }
    auto* work = new LoadWork{};
    work->index = index;
    work->epoch = s_epoch;
    std::snprintf(work->path, sizeof(work->path), "%s",
                  s_files[static_cast<size_t>(index)].path);
    // 与 thumb 相同：解码栈放 SPIRAM，避免内部 12KB 连续块不足 →「预览启动失败」
    constexpr uint32_t kPreviewStack = 12 * 1024;
    if (xTaskCreatePinnedToCoreWithCaps(LoadPreviewTask, "wp_preview", kPreviewStack, work, 5,
                                        &s_load_task, 0,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        const size_t free_int = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t largest_int =
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const size_t largest_psram =
            heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        ESP_LOGW(TAG,
                 "preview task create failed: need stack=%u in SPIRAM; "
                 "int free=%u largest=%u; psram free=%u largest=%u; path=%s",
                 static_cast<unsigned>(kPreviewStack),
                 static_cast<unsigned>(free_int),
                 static_cast<unsigned>(largest_int),
                 static_cast<unsigned>(free_psram),
                 static_cast<unsigned>(largest_psram), work->path);
        delete work;
        s_load_busy.store(false);
        s_load_task = nullptr;
        if (s_ui.preview_status != nullptr) {
            lv_label_set_text(s_ui.preview_status,
                              largest_psram < kPreviewStack ? Lang::Strings::WALLPAPER_PREVIEW_OOM
                                                            : Lang::Strings::WALLPAPER_PREVIEW_START_FAIL);
            lv_obj_clear_flag(s_ui.preview_status, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        ESP_LOGI(TAG, "preview task started index=%d path=%s", index, work->path);
    }
}

struct ThumbWorkItem {
    int index = -1;
    char path[192] = {};
};

struct ThumbBatchMsg {
    uint32_t epoch = 0;
    std::vector<std::pair<int, reader::RasterImage*>> items;
};

void ApplyThumbsAsync(void* p) {
    auto* msg = static_cast<ThumbBatchMsg*>(p);
    s_thumb_busy.store(false);
    s_thumb_task = nullptr;
    if (msg == nullptr) {
        return;
    }
    if (!s_screen_alive || msg->epoch != s_epoch) {
        for (auto& it : msg->items) {
            delete it.second;
        }
        delete msg;
        return;
    }

    // 先拆掉仍引用旧/即将被替换缓冲的控件，再 reset unique_ptr，避免 UAF
    if (s_ui.list_host != nullptr) {
        lv_obj_clean(s_ui.list_host);
        s_ui.list_empty = nullptr;
    }

    for (auto& it : msg->items) {
        if (it.first < 0 || it.first >= static_cast<int>(s_thumbs.size())) {
            delete it.second;
            continue;
        }
        if (it.second != nullptr) {
            if (!it.second->empty()) {
                it.second->BindDsc();
            }
            s_thumbs[static_cast<size_t>(it.first)].reset(it.second);
        }
    }
    delete msg;
    if (s_mode == UiMode::kList) {
        RebuildListPageInternal(false);  // 本批已解码，勿再 ScheduleThumbFill
    }
}

struct ThumbFillWork {
    uint32_t epoch = 0;
    int decode_w = 0;
    int decode_h = 0;
    std::vector<ThumbWorkItem> items;
};

void ThumbFillTask(void* arg) {
    auto* work = static_cast<ThumbFillWork*>(arg);
    auto* msg = new ThumbBatchMsg{};
    if (work != nullptr) {
        msg->epoch = work->epoch;
        const int dw = work->decode_w > 0 ? work->decode_w : 120;
        const int dh = work->decode_h > 0 ? work->decode_h : 160;
        for (const auto& w : work->items) {
            auto* img = new reader::RasterImage();
            if (reader::DecodeImageFileToL8(w.path, dw, dh, *img) && !img->empty()) {
                msg->items.emplace_back(w.index, img);
            } else {
                img->Reset();
                msg->items.emplace_back(w.index, img);
            }
        }
        delete work;
    }
    if (lv_async_call(ApplyThumbsAsync, msg) != LV_RESULT_OK) {
        for (auto& it : msg->items) {
            delete it.second;
        }
        delete msg;
        s_thumb_busy.store(false);
        s_thumb_task = nullptr;
    }
    vTaskDeleteWithCaps(nullptr);
}

void ScheduleThumbFill() {
    if (!s_screen_alive || s_mode != UiMode::kList || s_files.empty()) {
        return;
    }
    if (s_thumb_busy.load()) {
        return;
    }
    ClampListPage();
    const int start = s_list_page * s_list_per_page;
    const int end = std::min(start + s_list_per_page, static_cast<int>(s_files.size()));
    auto* work = new ThumbFillWork{};
    work->epoch = s_epoch;
    work->decode_w = FrameInner(s_grid_cell_w, kGridFrameBorder);
    work->decode_h = FrameInner(s_grid_cover_h, kGridFrameBorder);
    work->items.reserve(static_cast<size_t>(end - start));
    for (int i = start; i < end; ++i) {
        if (i >= static_cast<int>(s_thumbs.size())) {
            break;
        }
        if (s_thumbs[static_cast<size_t>(i)] != nullptr) {
            continue;
        }
        ThumbWorkItem item;
        item.index = i;
        std::snprintf(item.path, sizeof(item.path), "%s", s_files[static_cast<size_t>(i)].path);
        work->items.push_back(item);
    }
    if (work->items.empty()) {
        delete work;
        return;
    }
    if (s_thumb_busy.exchange(true)) {
        delete work;
        return;
    }
    // 栈放 SPIRAM：内部 DRAM 常仅数十 KB，12KB 连续块不足时会创建失败
    constexpr uint32_t kThumbStack = 12 * 1024;
    if (xTaskCreatePinnedToCoreWithCaps(ThumbFillTask, "wp_thumb", kThumbStack, work, 4,
                                        &s_thumb_task, 0,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        const size_t free_int = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t largest_int =
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const size_t largest_psram =
            heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        ESP_LOGW(TAG,
                 "thumb task create failed: need stack=%u in SPIRAM; "
                 "int free=%u largest=%u; psram free=%u largest=%u; items=%u",
                 static_cast<unsigned>(kThumbStack),
                 static_cast<unsigned>(free_int),
                 static_cast<unsigned>(largest_int),
                 static_cast<unsigned>(free_psram),
                 static_cast<unsigned>(largest_psram),
                 static_cast<unsigned>(work->items.size()));
        delete work;
        s_thumb_busy.store(false);
        s_thumb_task = nullptr;
    }
}

struct EnableWork {
    uint32_t epoch = 0;
    bool for_standby = false;
    bool clear = false;
    char path[192] = {};
    char name[96] = {};
};

struct EnableResultMsg {
    uint32_t epoch = 0;
    bool ok = false;
    bool for_standby = false;
    char err[80] = {};
    char name[96] = {};
};

void ApplyEnableAsync(void* p) {
    auto* msg = static_cast<EnableResultMsg*>(p);
    s_enable_busy.store(false);
    s_enable_task = nullptr;
    if (msg == nullptr) {
        return;
    }
    if (!s_screen_alive || msg->epoch != s_epoch) {
        delete msg;
        return;
    }
    if (msg->ok) {
        if (msg->for_standby) {
            s_standby_name = msg->name[0] != '\0' ? msg->name : "";
        } else {
            s_shutdown_name = msg->name[0] != '\0' ? msg->name : "";
        }
        char preview_name[sizeof(FileEntry::name)] = {};
        if (s_preview_idx >= 0 && s_preview_idx < static_cast<int>(s_files.size())) {
            std::snprintf(preview_name, sizeof(preview_name), "%s", s_files[s_preview_idx].name);
        }
        SortFilesActiveFirst();
        if (preview_name[0] != '\0') {
            s_preview_idx = -1;
            for (size_t i = 0; i < s_files.size(); ++i) {
                if (std::strcmp(s_files[i].name, preview_name) == 0) {
                    s_preview_idx = static_cast<int>(i);
                    break;
                }
            }
        }
        if (s_mode == UiMode::kList) {
            RebuildListPage();
        }
    } else {
        ESP_LOGW(TAG, "enable failed: %s", msg->err[0] != '\0' ? msg->err : Lang::Strings::WALLPAPER_ENABLE_FAIL);
    }
    UpdateEnableButtonUi();
    UpdateDeleteButtonUi();
    delete msg;
}

void EnableTask(void* arg) {
    auto* work = static_cast<EnableWork*>(arg);
    auto* msg = new EnableResultMsg{};
    if (work == nullptr) {
        std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::WALLPAPER_FILE_INVALID);
    } else {
        msg->epoch = work->epoch;
        msg->for_standby = work->for_standby;
        std::snprintf(msg->name, sizeof(msg->name), "%s", work->name);
        if (work->clear) {
            if (work->for_standby) {
                wallpaper::ClearStandbyWallpaper();
            } else {
                wallpaper::ClearShutdownWallpaper();
            }
            msg->ok = true;
            msg->name[0] = '\0';
        } else {
            std::string err;
            msg->ok = work->for_standby ? wallpaper::SetStandbyFromFile(work->path, err)
                                        : wallpaper::SetActiveFromFile(work->path, err);
            if (!msg->ok) {
                std::snprintf(msg->err, sizeof(msg->err), "%s",
                              err.empty() ? Lang::Strings::WALLPAPER_ENABLE_FAIL : err.c_str());
            }
        }
        delete work;
    }
    if (lv_async_call(ApplyEnableAsync, msg) != LV_RESULT_OK) {
        delete msg;
        s_enable_busy.store(false);
        s_enable_task = nullptr;
    }
    vTaskDelete(nullptr);
}

void ScheduleEnable(int index, bool for_standby, bool clear) {
    if (index < 0 || index >= static_cast<int>(s_files.size())) {
        return;
    }
    if (s_enable_busy.exchange(true)) {
        return;
    }
    UpdateEnableButtonUi();
    auto* work = new EnableWork{};
    work->epoch = s_epoch;
    work->for_standby = for_standby;
    work->clear = clear;
    std::snprintf(work->path, sizeof(work->path), "%s",
                  s_files[static_cast<size_t>(index)].path);
    std::snprintf(work->name, sizeof(work->name), "%s",
                  s_files[static_cast<size_t>(index)].name);
    if (xTaskCreatePinnedToCore(EnableTask, "wp_enable", 8 * 1024, work, 5, &s_enable_task, 0) !=
        pdPASS) {
        delete work;
        s_enable_busy.store(false);
        s_enable_task = nullptr;
        ESP_LOGW(TAG, "enable task create failed");
        UpdateEnableButtonUi();
        UpdateDeleteButtonUi();
    }
}

void OnEnableShutdownClicked(lv_event_t* /*e*/) {
    if (s_mode != UiMode::kPreview || s_preview_idx < 0) {
        return;
    }
    if (s_enable_busy.load() || s_load_busy.load() || s_delete_busy.load()) {
        return;
    }
    const char* name = s_files[static_cast<size_t>(s_preview_idx)].name;
    const bool is_on = !s_shutdown_name.empty() && s_shutdown_name == name;
    ScheduleEnable(s_preview_idx, false, is_on);
}

void OnEnableStandbyClicked(lv_event_t* /*e*/) {
    if (s_mode != UiMode::kPreview || s_preview_idx < 0) {
        return;
    }
    if (s_enable_busy.load() || s_load_busy.load() || s_delete_busy.load()) {
        return;
    }
    const char* name = s_files[static_cast<size_t>(s_preview_idx)].name;
    const bool is_on = !s_standby_name.empty() && s_standby_name == name;
    ScheduleEnable(s_preview_idx, true, is_on);
}

struct DeleteWork {
    uint32_t epoch = 0;
    char path[192] = {};
    char name[96] = {};
};

struct DeleteResultMsg {
    uint32_t epoch = 0;
    bool ok = false;
    char err[80] = {};
};

bool DeleteWallpaperFileOnSd(const char* path, const char* name, std::string& err_out) {
    err_out.clear();
    if (path == nullptr || path[0] == '\0' || name == nullptr || name[0] == '\0') {
        err_out = Lang::Strings::WALLPAPER_FILE_INVALID;
        return false;
    }
    const std::string prefix = std::string(SD_PATH_WALLPAPER) + "/";
    if (std::strncmp(path, prefix.c_str(), prefix.size()) != 0) {
        err_out = Lang::Strings::WALLPAPER_PATH_INVALID;
        return false;
    }
    if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0 || std::strchr(name, '/') != nullptr) {
        err_out = Lang::Strings::WALLPAPER_NAME_INVALID;
        return false;
    }

    if (unlink(path) != 0 && errno != ENOENT) {
        err_out = Lang::Strings::WALLPAPER_DELETE_FAIL;
        return false;
    }
    reader::DeleteWallpaperMeta(path);
    unlink((std::string(path) + ".tmp").c_str());

    // ClearActiveIfMatches / NVS：须在本 worker（内部 RAM 栈），勿在 LVGL 任务调用
    wallpaper::ClearActiveIfMatches(name);
    ESP_LOGI(TAG, "deleted wallpaper %s", path);
    return true;
}

void ApplyDeleteAsync(void* p) {
    auto* msg = static_cast<DeleteResultMsg*>(p);
    s_delete_busy.store(false);
    s_delete_task = nullptr;
    if (msg == nullptr) {
        return;
    }
    if (!s_screen_alive || msg->epoch != s_epoch) {
        delete msg;
        return;
    }
    if (!msg->ok) {
        std::snprintf(s_delete_meta_hint, sizeof(s_delete_meta_hint), "%s",
                      msg->err[0] != '\0' ? msg->err : Lang::Strings::WALLPAPER_DELETE_FAIL);
        UpdateEnableButtonUi();
        UpdateDeleteButtonUi();
        delete msg;
        return;
    }
    s_delete_meta_hint[0] = '\0';
    ClearPreviewImage();
    RefreshActiveName();
    CollectWallpapers();
    s_preview_idx = -1;
    ShowMode(UiMode::kList);
    RebuildListPage();
    delete msg;
}

void DeleteTask(void* arg) {
    auto* work = static_cast<DeleteWork*>(arg);
    auto* msg = new DeleteResultMsg{};
    if (work == nullptr) {
        std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::WALLPAPER_FILE_INVALID);
    } else {
        msg->epoch = work->epoch;
        std::string err;
        msg->ok = DeleteWallpaperFileOnSd(work->path, work->name, err);
        if (!msg->ok) {
            std::snprintf(msg->err, sizeof(msg->err), "%s",
                          err.empty() ? Lang::Strings::WALLPAPER_DELETE_FAIL : err.c_str());
        }
        delete work;
    }
    if (lv_async_call(ApplyDeleteAsync, msg) != LV_RESULT_OK) {
        delete msg;
        s_delete_busy.store(false);
        s_delete_task = nullptr;
    }
    vTaskDelete(nullptr);
}

void ScheduleDelete(int index) {
    if (index < 0 || index >= static_cast<int>(s_files.size())) {
        return;
    }
    if (s_delete_busy.exchange(true)) {
        return;
    }
    s_delete_meta_hint[0] = '\0';
    UpdateEnableButtonUi();
    UpdateDeleteButtonUi();
    auto* work = new DeleteWork{};
    work->epoch = s_epoch;
    std::snprintf(work->path, sizeof(work->path), "%s",
                  s_files[static_cast<size_t>(index)].path);
    std::snprintf(work->name, sizeof(work->name), "%s",
                  s_files[static_cast<size_t>(index)].name);
    if (xTaskCreatePinnedToCore(DeleteTask, "wp_delete", 8 * 1024, work, 5, &s_delete_task, 0) !=
        pdPASS) {
        delete work;
        s_delete_busy.store(false);
        s_delete_task = nullptr;
        std::snprintf(s_delete_meta_hint, sizeof(s_delete_meta_hint), "%s", Lang::Strings::WALLPAPER_DELETE_START_FAIL);
        UpdateEnableButtonUi();
        UpdateDeleteButtonUi();
    }
}

void OnDeleteClicked(lv_event_t* /*e*/) {
    if (s_mode != UiMode::kPreview || s_preview_idx < 0) {
        return;
    }
    if (s_delete_busy.load() || s_enable_busy.load() || s_load_busy.load()) {
        return;
    }
    ScheduleDelete(s_preview_idx);
}

void SyncWpSelectedSize() {
    if (s_selected.size() != s_files.size()) {
        s_selected.assign(s_files.size(), 0);
    }
}

int WpSelectedCount() {
    SyncWpSelectedSize();
    int n = 0;
    for (uint8_t v : s_selected) {
        if (v != 0) {
            ++n;
        }
    }
    return n;
}

bool WpItemSelected(int idx) {
    SyncWpSelectedSize();
    return idx >= 0 && idx < static_cast<int>(s_selected.size()) &&
           s_selected[static_cast<size_t>(idx)] != 0;
}

void ToggleWpItemSelected(int idx) {
    SyncWpSelectedSize();
    if (idx < 0 || idx >= static_cast<int>(s_selected.size())) {
        return;
    }
    s_selected[static_cast<size_t>(idx)] = s_selected[static_cast<size_t>(idx)] ? 0 : 1;
}

void RefreshWpFooterMode() {
    if (s_mode != UiMode::kList) {
        if (s_ui.multi_bar != nullptr) {
            lv_obj_add_flag(s_ui.multi_bar, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_ui.page_lbl != nullptr) {
            lv_obj_add_flag(s_ui.page_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if (s_multi) {
        if (s_ui.multi_bar != nullptr) {
            lv_obj_set_width(s_ui.multi_bar, lv_pct(100));
            lv_obj_align(s_ui.multi_bar, LV_ALIGN_CENTER, 0, -3);
            lv_obj_clear_flag(s_ui.multi_bar, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_ui.page_lbl != nullptr) {
            lv_obj_set_width(s_ui.page_lbl, LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(s_ui.page_lbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(s_ui.page_lbl, LV_ALIGN_RIGHT_MID, -10, 0);
            lv_obj_clear_flag(s_ui.page_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_ui.page_lbl);
        }
        if (s_ui.status_label != nullptr && lv_obj_is_valid(s_ui.status_label)) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), Lang::Strings::WALLPAPER_SELECTED_FMT, WpSelectedCount());
            lv_label_set_text(s_ui.status_label, buf);
        }
    } else {
        if (s_ui.multi_bar != nullptr) {
            lv_obj_add_flag(s_ui.multi_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_width(s_ui.multi_bar, lv_pct(100));
            lv_obj_align(s_ui.multi_bar, LV_ALIGN_CENTER, 0, 0);
        }
        if (s_ui.page_lbl != nullptr) {
            lv_obj_set_width(s_ui.page_lbl, LV_HOR_RES - 16);
            lv_obj_set_style_text_align(s_ui.page_lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(s_ui.page_lbl, LV_ALIGN_CENTER, 0, 0);
            lv_obj_clear_flag(s_ui.page_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_ui.status_label != nullptr && lv_obj_is_valid(s_ui.status_label)) {
            lv_label_set_text(s_ui.status_label, Lang::Strings::HOME_APP_WALLPAPER);
        }
    }
}

void ExitWpMultiMode(bool rebuild) {
    s_multi = false;
    s_suppress_click_until_us = 0;
    s_suppress_click_idx = -1;
    s_selected.assign(s_files.size(), 0);
    RefreshWpFooterMode();
    if (rebuild) {
        RequestWallpaperListRebuild();
    }
}

void EnterWpMultiModeSelect(int idx) {
    s_multi = true;
    s_selected.assign(s_files.size(), 0);
    if (idx >= 0 && idx < static_cast<int>(s_selected.size())) {
        s_selected[static_cast<size_t>(idx)] = 1;
    }
    RefreshWpFooterMode();
    RequestWallpaperListRebuild();
}

lv_obj_t* FindWpCell(int index) {
    if (s_ui.list_host == nullptr || !lv_obj_is_valid(s_ui.list_host)) {
        return nullptr;
    }
    const uint32_t n = lv_obj_get_child_count(s_ui.list_host);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* cell = lv_obj_get_child(s_ui.list_host, i);
        if (cell != nullptr &&
            static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(cell))) == index) {
            return cell;
        }
    }
    return nullptr;
}

lv_obj_t* FindWpCheckBox(lv_obj_t* cell) {
    if (cell == nullptr) {
        return nullptr;
    }
    const uint32_t n = lv_obj_get_child_count(cell);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* c = lv_obj_get_child(cell, i);
        if (c != nullptr && lv_obj_get_width(c) == kWpCheckSize &&
            lv_obj_get_height(c) == kWpCheckSize) {
            return c;
        }
    }
    return nullptr;
}

void PatchWpRowCheckMark(int index) {
    if (!s_multi) {
        return;
    }
    lv_obj_t* check = FindWpCheckBox(FindWpCell(index));
    if (check == nullptr) {
        RequestWallpaperListRebuild();
        return;
    }
    lv_obj_clean(check);
    if (!WpItemSelected(index)) {
        return;
    }
    lv_obj_t* mark = lv_label_create(check);
    lv_label_set_text(mark, "√");
    lv_obj_set_style_text_font(mark, UiFont(), 0);
    lv_obj_set_style_text_color(mark, lv_color_black(), 0);
    lv_obj_center(mark);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
}

void PatchWpVisibleCheckMarks() {
    if (!s_multi || s_ui.list_host == nullptr || !lv_obj_is_valid(s_ui.list_host)) {
        return;
    }
    const uint32_t n = lv_obj_get_child_count(s_ui.list_host);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* cell = lv_obj_get_child(s_ui.list_host, i);
        if (cell == nullptr) {
            continue;
        }
        PatchWpRowCheckMark(
            static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(cell))));
    }
    if (s_ui.status_label != nullptr && lv_obj_is_valid(s_ui.status_label)) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), Lang::Strings::WALLPAPER_SELECTED_FMT, WpSelectedCount());
        lv_label_set_text(s_ui.status_label, buf);
    }
}

ScreenPaintCoalesce s_wp_check_paint{};

void RequestWpCheckMarksPaint() {
    if (s_wp_check_paint.paint == nullptr) {
        s_wp_check_paint.paint = PatchWpVisibleCheckMarks;
    }
    ScreenPaintCoalesceRequestDebounced(&s_wp_check_paint, kWpCheckPaintDebounceUs);
}

void OnWpMultiCancel(lv_event_t* /*e*/) {
    if (s_delete_busy.load()) {
        return;
    }
    ExitWpMultiMode(true);
}

void OnWpMultiSelectAll(lv_event_t* /*e*/) {
    if (!s_multi || s_files.empty() || s_delete_busy.load()) {
        return;
    }
    SyncWpSelectedSize();
    const int n = static_cast<int>(s_files.size());
    const bool clear = (WpSelectedCount() >= n);
    s_selected.assign(static_cast<size_t>(n), clear ? 0 : 1);
    RequestWpCheckMarksPaint();
}

struct BatchDeleteItem {
    char path[192] = {};
    char name[96] = {};
};

struct BatchDeleteWork {
    uint32_t epoch = 0;
    int n = 0;
    BatchDeleteItem items[kMaxItems]{};
};

struct BatchDeleteResultMsg {
    uint32_t epoch = 0;
    int deleted = 0;
};

void ApplyBatchDeleteAsync(void* p) {
    auto* msg = static_cast<BatchDeleteResultMsg*>(p);
    s_delete_busy.store(false);
    s_delete_task = nullptr;
    if (msg == nullptr) {
        return;
    }
    if (!s_screen_alive || msg->epoch != s_epoch) {
        delete msg;
        return;
    }
    ESP_LOGI(TAG, "batch delete done n=%d", msg->deleted);
    RefreshActiveName();
    CollectWallpapers();
    ClampListPage();
    ExitWpMultiMode(false);
    RebuildListPage();
    delete msg;
}

void BatchDeleteTask(void* arg) {
    auto* work = static_cast<BatchDeleteWork*>(arg);
    auto* msg = new BatchDeleteResultMsg{};
    if (work != nullptr) {
        msg->epoch = work->epoch;
        for (int i = 0; i < work->n; ++i) {
            std::string err;
            if (DeleteWallpaperFileOnSd(work->items[i].path, work->items[i].name, err)) {
                ++msg->deleted;
            } else {
                ESP_LOGW(TAG, "batch delete fail %s: %s", work->items[i].name, err.c_str());
            }
        }
        delete work;
    }
    if (lv_async_call(ApplyBatchDeleteAsync, msg) != LV_RESULT_OK) {
        delete msg;
        s_delete_busy.store(false);
        s_delete_task = nullptr;
    }
    vTaskDelete(nullptr);
}

void OnWpMultiRemove(lv_event_t* /*e*/) {
    if (!s_multi || s_delete_busy.load() || s_enable_busy.load() || s_load_busy.load()) {
        return;
    }
    SyncWpSelectedSize();
    if (WpSelectedCount() <= 0) {
        return;
    }
    if (s_delete_busy.exchange(true)) {
        return;
    }
    auto* work = new BatchDeleteWork{};
    work->epoch = s_epoch;
    for (int i = 0; i < static_cast<int>(s_files.size()) && work->n < kMaxItems; ++i) {
        if (i >= static_cast<int>(s_selected.size()) || s_selected[static_cast<size_t>(i)] == 0) {
            continue;
        }
        auto& it = work->items[work->n++];
        std::snprintf(it.path, sizeof(it.path), "%s", s_files[static_cast<size_t>(i)].path);
        std::snprintf(it.name, sizeof(it.name), "%s", s_files[static_cast<size_t>(i)].name);
    }
    if (work->n <= 0) {
        delete work;
        s_delete_busy.store(false);
        return;
    }
    if (xTaskCreatePinnedToCore(BatchDeleteTask, "wp_batch_del", 8 * 1024, work, 5, &s_delete_task,
                                0) != pdPASS) {
        delete work;
        s_delete_busy.store(false);
        ESP_LOGW(TAG, "batch delete task create failed");
    }
}

void OpenPreview(int index) {
    if (index < 0 || index >= static_cast<int>(s_files.size())) {
        return;
    }
    s_preview_idx = index;
    s_delete_meta_hint[0] = '\0';
    ClearPreviewImage();
    ShowMode(UiMode::kPreview);
    const FileEntry& fe = s_files[static_cast<size_t>(index)];
    if (s_ui.preview_title != nullptr) {
        const char* raw = fe.title[0] != '\0' ? fe.title : fe.name;
        const std::string shown = LayoutTitleTwoLines(raw, UiFont(), ContentWidth());
        lv_label_set_text(s_ui.preview_title, shown.c_str());
    }
    if (s_ui.preview_status != nullptr) {
        lv_label_set_text(s_ui.preview_status, Lang::Strings::WALLPAPER_LOADING);
        lv_obj_clear_flag(s_ui.preview_status, LV_OBJ_FLAG_HIDDEN);
    }
    UpdateEnableButtonUi();
    UpdateDeleteButtonUi();
    ScheduleLoadPreview(index);
}

void ClosePreview() {
    s_preview_idx = -1;
    ClearPreviewImage();
    ShowMode(UiMode::kList);
    RebuildListPage();
}

void OnRowLongPressed(lv_event_t* e) {
    if (s_mode != UiMode::kList || s_delete_busy.load()) {
        return;
    }
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (index < 0 || index >= static_cast<int>(s_files.size())) {
        return;
    }
    if (!s_multi) {
        s_suppress_click_idx = index;
        s_suppress_click_until_us = esp_timer_get_time() + kWpSuppressRowClickUs;
        EnterWpMultiModeSelect(index);
        return;
    }
    ToggleWpItemSelected(index);
    RequestWpCheckMarksPaint();
}

void OnRowClicked(lv_event_t* e) {
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (index == s_suppress_click_idx && esp_timer_get_time() < s_suppress_click_until_us) {
        return;
    }
    if (s_multi) {
        ToggleWpItemSelected(index);
        RequestWpCheckMarksPaint();
        return;
    }
    OpenPreview(index);
}

// 创建角标底座；外框与内框留白后用于绘制图标。
lv_obj_t* MakeTipBadge(lv_obj_t* parent) {
    lv_obj_t* outer = lv_obj_create(parent);
    lv_obj_remove_style_all(outer);
    lv_obj_set_size(outer, 30, 30);
    lv_obj_set_style_bg_color(outer, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(outer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(outer, 2, 0);
    lv_obj_set_style_border_color(outer, lv_color_black(), 0);
    lv_obj_set_style_radius(outer, 8, 0);
    lv_obj_set_style_pad_all(outer, 2, 0);
    lv_obj_clear_flag(outer, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(outer);

    lv_obj_t* inner = lv_obj_create(outer);
    lv_obj_remove_style_all(inner);
    lv_obj_set_size(inner, 22, 22);
    lv_obj_set_style_bg_color(inner, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(inner, 2, 0);
    lv_obj_set_style_border_color(inner, lv_color_black(), 0);
    lv_obj_set_style_radius(inner, 5, 0);
    lv_obj_clear_flag(inner, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(inner);
    return inner;
}

// 关机壁纸的角标：电源键。
void DrawPowerTipIcon(lv_obj_t* parent) {
    lv_obj_t* tip = MakeTipBadge(parent);
    lv_obj_t* ring = lv_obj_create(tip);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 12, 12);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_align(ring, LV_ALIGN_CENTER, 0, 1);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(ring);
    lv_obj_t* stem = lv_obj_create(tip);
    lv_obj_remove_style_all(stem);
    lv_obj_set_size(stem, 2, 6);
    lv_obj_set_style_bg_color(stem, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(stem, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(stem, 1, 0);
    lv_obj_align(stem, LV_ALIGN_TOP_MID, 0, 3);
    lv_obj_clear_flag(stem, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(stem);
}

// 待机壁纸的角标：月牙。
void DrawMoonTipIcon(lv_obj_t* parent) {
    lv_obj_t* tip = MakeTipBadge(parent);
    lv_obj_t* moon = lv_obj_create(tip);
    lv_obj_remove_style_all(moon);
    lv_obj_set_size(moon, 10, 10);
    lv_obj_set_style_radius(moon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(moon, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(moon, LV_OPA_COVER, 0);
    lv_obj_align(moon, LV_ALIGN_CENTER, -1, 0);
    lv_obj_clear_flag(moon, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(moon);
    // 白圆切出月牙（与角标白底同色）
    lv_obj_t* cut = lv_obj_create(tip);
    lv_obj_remove_style_all(cut);
    lv_obj_set_size(cut, 8, 8);
    lv_obj_set_style_radius(cut, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cut, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cut, LV_OPA_COVER, 0);
    lv_obj_align(cut, LV_ALIGN_CENTER, 3, -1);
    lv_obj_clear_flag(cut, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(cut);
}

// 网格单元格：封面、启用状态角标和多选勾选框。
lv_obj_t* CreateGridCell(lv_obj_t* parent, int index, lv_coord_t cell_w, lv_coord_t cover_h) {
    lv_obj_t* cell = lv_obj_create(parent);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, cell_w, cover_h);
    lv_obj_set_style_radius(cell, kCoverRadius, 0);
    lv_obj_set_style_clip_corner(cell, true, 0);
    lv_obj_set_style_border_width(cell, kGridFrameBorder, 0);
    lv_obj_set_style_border_color(cell, lv_color_black(), 0);
    lv_obj_set_style_pad_all(cell, kGridFrameBorder, 0);
    lv_obj_set_style_bg_color(cell, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(cell);
    lv_obj_set_user_data(cell, reinterpret_cast<void*>(static_cast<intptr_t>(index)));
    HapticAttachClick(cell);
    lv_obj_add_event_cb(cell, OnRowClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));
    lv_obj_add_event_cb(cell, OnRowLongPressed, LV_EVENT_LONG_PRESSED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));

    if (index >= 0 && index < static_cast<int>(s_thumbs.size()) &&
        s_thumbs[static_cast<size_t>(index)] != nullptr &&
        !s_thumbs[static_cast<size_t>(index)]->empty()) {
        lv_obj_t* thumb = lv_image_create(cell);
        lv_image_set_src(thumb, &s_thumbs[static_cast<size_t>(index)]->dsc);
        lv_obj_set_size(thumb, s_thumbs[static_cast<size_t>(index)]->width,
                        s_thumbs[static_cast<size_t>(index)]->height);
        lv_obj_center(thumb);
        lv_obj_clear_flag(thumb, LV_OBJ_FLAG_CLICKABLE);
    }

    if (index >= 0 && index < static_cast<int>(s_files.size())) {
        const char* name = s_files[static_cast<size_t>(index)].name;
        const bool shutdown_on = !s_shutdown_name.empty() && s_shutdown_name == name;
        const bool standby_on = !s_standby_name.empty() && s_standby_name == name;
        if (shutdown_on || standby_on) {
            lv_obj_t* tips = lv_obj_create(cell);
            lv_obj_remove_style_all(tips);
            lv_obj_set_size(tips, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(tips, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(tips, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(tips, 4, 0);
            // 多选勾在右上，角标改左上避免叠盖
            lv_obj_align(tips, s_multi ? LV_ALIGN_TOP_LEFT : LV_ALIGN_TOP_RIGHT, s_multi ? 4 : -4,
                         4);
            lv_obj_clear_flag(tips, LV_OBJ_FLAG_CLICKABLE);
            DisableScroll(tips);
            if (shutdown_on) {
                DrawPowerTipIcon(tips);
            }
            if (standby_on) {
                DrawMoonTipIcon(tips);
            }
        }
    }

    if (s_multi) {
        lv_obj_t* check = lv_obj_create(cell);
        lv_obj_remove_style_all(check);
        lv_obj_set_size(check, kWpCheckSize, kWpCheckSize);
        lv_obj_set_style_bg_color(check, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(check, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(check, lv_color_black(), 0);
        lv_obj_set_style_border_width(check, kGridFrameBorder, 0);
        lv_obj_set_style_radius(check, 4, 0);
        lv_obj_align(check, LV_ALIGN_TOP_RIGHT, -4, 4);
        DisableScroll(check);
        lv_obj_clear_flag(check, LV_OBJ_FLAG_CLICKABLE);
        if (WpItemSelected(index)) {
            lv_obj_t* mark = lv_label_create(check);
            lv_label_set_text(mark, "√");
            lv_obj_set_style_text_font(mark, UiFont(), 0);
            lv_obj_set_style_text_color(mark, lv_color_black(), 0);
            lv_obj_center(mark);
            lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    return cell;
}

void ShowMessage(lv_obj_t* host, const char* text) {
    if (host == nullptr) {
        return;
    }
    lv_obj_t* lbl = lv_label_create(host);
    lv_obj_set_style_text_font(lbl, UiFont(), 0);
    lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl, text != nullptr ? text : "");
    lv_obj_set_width(lbl, ContentWidth());
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    s_ui.list_empty = lbl;
}

// 只保留当前页缩略图，翻页时释放其他缓存。
void ClearOffPageThumbs() {
    if (s_list_per_page <= 0 || s_thumbs.empty()) {
        return;
    }
    const int start = s_list_page * s_list_per_page;
    const int end = start + s_list_per_page;
    for (int i = 0; i < static_cast<int>(s_thumbs.size()); ++i) {
        if (i < start || i >= end) {
            s_thumbs[static_cast<size_t>(i)].reset();
        }
    }
}

void RebuildListPageInternal(bool schedule_thumbs) {
    if (!s_screen_alive || s_ui.list_host == nullptr) {
        return;
    }
    lv_obj_clean(s_ui.list_host);
    s_ui.list_empty = nullptr;
    DisableScroll(s_ui.list_host);

    lv_obj_set_style_layout(s_ui.list_host, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(s_ui.list_host, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_ui.list_host, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_ui.list_host, kGridRowGap, 0);
    lv_obj_set_style_pad_column(s_ui.list_host, kGridColGap, 0);

    if (!s_sd_ready) {
        lv_obj_set_flex_flow(s_ui.list_host, LV_FLEX_FLOW_COLUMN);
        ShowMessage(s_ui.list_host, Lang::Strings::WALLPAPER_NO_SD);
        if (s_ui.page_lbl != nullptr) {
            lv_label_set_text(s_ui.page_lbl, "1/1");
        }
        RefreshWpFooterMode();
        return;
    }
    if (s_files.empty()) {
        lv_obj_set_flex_flow(s_ui.list_host, LV_FLEX_FLOW_COLUMN);
        char tip[96];
        std::snprintf(tip, sizeof(tip), Lang::Strings::WALLPAPER_EMPTY_FMT, SdUserPath(SD_PATH_WALLPAPER));
        ShowMessage(s_ui.list_host, tip);
        if (s_ui.page_lbl != nullptr) {
            lv_label_set_text(s_ui.page_lbl, "1/1");
        }
        RefreshWpFooterMode();
        return;
    }

    ClampListPage();
    SyncWpSelectedSize();
    if (schedule_thumbs) {
        ClearOffPageThumbs();
        // 本页同步解码：墨水屏一次刷出封面，避免白格后再二次全刷
        FillPageThumbsSync(s_list_page);
    }
    const lv_coord_t cell_w = s_grid_cell_w;
    const lv_coord_t cover_h = s_grid_cover_h;
    const int start = s_list_page * s_list_per_page;
    const int end = std::min(start + s_list_per_page, static_cast<int>(s_files.size()));
    for (int i = start; i < end; ++i) {
        CreateGridCell(s_ui.list_host, i, cell_w, cover_h);
    }
    if (s_ui.page_lbl != nullptr) {
        char foot[48];
        std::snprintf(foot, sizeof(foot), "%d/%d", s_list_page + 1, ListPageCount());
        lv_label_set_text(s_ui.page_lbl, foot);
    }
    RefreshWpFooterMode();
    if (schedule_thumbs) {
        ScheduleThumbFill();  // 已同步填满时为空操作；失败槽位仍可后台补
    }
}

void RebuildListPage() {
    RebuildListPageInternal(true);
}

ScreenPaintCoalesce s_wallpaper_paint{};

void PaintWallpaperList() {
    ESP_LOGI(TAG, "async RebuildListPage page=%d/%d begin", s_list_page + 1, ListPageCount());
    RebuildListPage();
    ESP_LOGI(TAG, "async RebuildListPage done");
}

void RequestWallpaperListRebuild() {
    if (s_wallpaper_paint.paint == nullptr) {
        s_wallpaper_paint.paint = PaintWallpaperList;
    }
    ScreenPaintCoalesceRequest(&s_wallpaper_paint);
}

void AsyncClosePreview(void* /*user_data*/) {
    ESP_LOGI(TAG, "async ClosePreview begin");
    ClosePreview();
    ESP_LOGI(TAG, "async ClosePreview done");
}

void ShowMode(UiMode mode) {
    s_mode = mode;
    if (s_ui.list_body != nullptr) {
        if (mode == UiMode::kList) {
            lv_obj_clear_flag(s_ui.list_body, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ui.list_body, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_ui.preview_body != nullptr) {
        if (mode == UiMode::kPreview) {
            lv_obj_clear_flag(s_ui.preview_body, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ui.preview_body, LV_OBJ_FLAG_HIDDEN);
        }
    }
    RefreshWpFooterMode();
}

bool WallpaperPageRepeatStep(int page_delta) {
    if (page_delta == 0 || s_mode != UiMode::kList) {
        return false;
    }
    const int last = std::max(0, ListPageCount() - 1);
    int next = s_list_page + page_delta;
    if (next < 0) {
        next = 0;
    } else if (next > last) {
        next = last;
    }
    if (next == s_list_page) {
        return false;
    }
    s_list_page = next;
    RequestWallpaperListRebuild();
    return page_delta < 0 ? s_list_page > 0 : s_list_page < last;
}

bool OnVkKeyLongPress(const char* key) {
    return VkPageRepeatTryStart(key, WallpaperPageRepeatStep);
}

bool OnVkKeyPressUp(const char* key) {
    return VkPageRepeatOnPressUp(key);
}

bool OnVkKey(const char* key) {
    if (key == nullptr) {
        return false;
    }
    if (s_mode == UiMode::kPreview) {
        // 详情页：vk_home / vk_prev 均回网格（不直接回系统首页）
        if (std::strcmp(key, "vk_prev") == 0 || std::strcmp(key, "vk_home") == 0) {
            ESP_LOGI(TAG, "%s preview -> queue ClosePreview", key);
            ScreenLvAsync(AsyncClosePreview);
            return true;
        }
        if (std::strcmp(key, "vk_next") == 0) {
            return true;
        }
        return false;
    }
    if (std::strcmp(key, "vk_prev") == 0) {
        if (s_list_page > 0) {
            --s_list_page;
            ESP_LOGI(TAG, "vk_prev list -> page %d queue rebuild", s_list_page + 1);
            RequestWallpaperListRebuild();
            return true;
        }
        // 第一页：多选中则退出批量，否则交给默认返回上一屏
        if (s_multi) {
            ExitWpMultiMode(true);
            return true;
        }
        return false;
    }
    if (std::strcmp(key, "vk_next") == 0) {
        if (s_list_page + 1 < ListPageCount()) {
            ++s_list_page;
            ESP_LOGI(TAG, "vk_next list -> page %d queue rebuild", s_list_page + 1);
            RequestWallpaperListRebuild();
        } else {
            ESP_LOGI(TAG, "vk_next list already last page=%d", s_list_page + 1);
        }
        return true;
    }
    return false;
}

void OnScreenDeleted(lv_event_t* e) {
    if (lv_event_get_target(e) != s_ui.screen) {
        return;
    }
    s_screen_alive = false;
    ++s_epoch;  // 作废在飞 preview/thumb/enable/delete 回调
    // 先拆掉引用 RasterImage 的控件；缩略图缓存在进程内复用，下次进 app 免白屏
    DetachRasterUsers();
    ClearPreviewImage();
    s_preview_idx = -1;
    s_mode = UiMode::kList;
    s_multi = false;
    s_selected.clear();
    s_suppress_click_until_us = 0;
    s_suppress_click_idx = -1;
    s_delete_busy.store(false);
    ScreenPaintCoalesceReset(&s_wp_check_paint);
    ScreenPaintCoalesceReset(&s_wallpaper_paint);
    s_ui = {};
}

}  // namespace

lv_obj_t* WallpaperScreen::Create() {
    // 旧屏可能仍在 async delete：先拆引用，递增 epoch 丢弃在飞任务；缩略图按文件复用
    s_screen_alive = false;
    ++s_epoch;
    DetachRasterUsers();
    ClearPreviewImage();
    s_ui = {};
    s_mode = UiMode::kList;
    s_list_page = 0;
    s_preview_idx = -1;
    s_multi = false;
    s_selected.clear();
    s_suppress_click_until_us = 0;
    s_suppress_click_idx = -1;
    ScreenPaintCoalesceReset(&s_wp_check_paint);
    ScreenPaintCoalesceReset(&s_wallpaper_paint);

    s_sd_ready = SdCardManager::GetInstance().IsMounted() && SdEnsureAppLayout();
    // 禁止在 LVGL 任务读 NVS：只用内存缓存 / SD 回退；NVS 由后台 hydrate。
    RefreshActiveName();
    if (s_sd_ready) {
        CollectWallpapers();
    } else {
        s_files.clear();
        s_thumbs.clear();
    }
    s_selected.assign(s_files.size(), 0);

    ScreenSetIsHome(false);

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, UiFont(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    DisableScroll(scr);
    s_ui.screen = scr;
    lv_obj_add_event_cb(scr, OnScreenDeleted, LV_EVENT_DELETE, nullptr);

    EpdStatusBar status = ScreenCreateStatusBar(scr);
    s_ui.status_label = status.status_label;
    if (status.status_label) {
        lv_label_set_text(status.status_label, Lang::Strings::HOME_APP_WALLPAPER);
    }
    if (status.notification_label) {
        lv_obj_add_flag(status.notification_label, LV_OBJ_FLAG_HIDDEN);
    }

    s_body_h = LV_VER_RES - status.height - kFooterH;
    s_list_per_page = kGridCols * kGridRows;
    // 九格均分可用宽高（加大间距后仍铺满，不再用 4:3 压矮封面）
    s_grid_cell_w = (ContentWidth() - kGridColGap * (kGridCols - 1)) / kGridCols;
    {
        const lv_coord_t usable = s_body_h - kPad * 2;
        lv_coord_t cover_h = (usable - kGridRowGap * (kGridRows - 1)) / kGridRows;
        if (cover_h < 72) {
            cover_h = 72;
        }
        s_grid_cover_h = cover_h;
    }

    // ---- 网格（同书架 3×3，均分铺满）----
    lv_obj_t* list_body = lv_obj_create(scr);
    lv_obj_remove_style_all(list_body);
    lv_obj_set_size(list_body, LV_HOR_RES, s_body_h);
    lv_obj_align(list_body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(list_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(list_body, kPad, 0);
    DisableScroll(list_body);
    lv_obj_clear_flag(list_body, LV_OBJ_FLAG_CLICKABLE);
    s_ui.list_body = list_body;

    lv_obj_t* list_host = lv_obj_create(list_body);
    lv_obj_remove_style_all(list_host);
    lv_obj_set_size(list_host, ContentWidth(), s_body_h - kPad * 2);
    lv_obj_align(list_host, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(list_host, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(list_host, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list_host, kGridRowGap, 0);
    lv_obj_set_style_pad_column(list_host, kGridColGap, 0);
    DisableScroll(list_host);
    s_ui.list_host = list_host;

    // ---- 设置页：大预览 + 标题/标签 + 双列开关 + 删除 ----
    lv_obj_t* preview_body = lv_obj_create(scr);
    lv_obj_remove_style_all(preview_body);
    lv_obj_set_size(preview_body, LV_HOR_RES, s_body_h);
    lv_obj_align(preview_body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(preview_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_hor(preview_body, kPad, 0);
    lv_obj_set_style_pad_top(preview_body, 8, 0);
    lv_obj_set_style_pad_bottom(preview_body, 12, 0);
    lv_obj_set_flex_flow(preview_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(preview_body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(preview_body, kPreviewGap, 0);
    DisableScroll(preview_body);
    lv_obj_add_flag(preview_body, LV_OBJ_FLAG_HIDDEN);
    s_ui.preview_body = preview_body;

    lv_obj_t* img_host = lv_obj_create(preview_body);
    lv_obj_remove_style_all(img_host);
    lv_obj_set_width(img_host, ContentWidth());
    lv_obj_set_flex_grow(img_host, 1);
    lv_obj_set_style_bg_color(img_host, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(img_host, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(img_host, kBorderW, 0);
    lv_obj_set_style_border_color(img_host, lv_color_black(), 0);
    lv_obj_set_style_pad_all(img_host, kBorderW, 0);
    lv_obj_set_style_radius(img_host, 16, 0);
    lv_obj_set_style_clip_corner(img_host, true, 0);
    DisableScroll(img_host);
    s_ui.preview_img_host = img_host;

    lv_obj_t* img = lv_image_create(img_host);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    s_ui.preview_img = img;

    lv_obj_t* preview_status = lv_label_create(img_host);
    lv_obj_set_style_text_font(preview_status, UiFont(), 0);
    lv_obj_set_style_text_color(preview_status, lv_color_black(), 0);
    lv_obj_set_style_text_align(preview_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(preview_status, "");
    lv_obj_align(preview_status, LV_ALIGN_CENTER, 0, 0);
    s_ui.preview_status = preview_status;

    lv_obj_t* preview_title = lv_label_create(preview_body);
    lv_obj_set_width(preview_title, ContentWidth());
    {
        const lv_font_t* f = UiFont();
        const lv_coord_t lh = (f != nullptr && f->line_height > 0) ? f->line_height : 29;
        lv_obj_set_height(preview_title, lh * 2);
    }
    lv_obj_set_style_text_font(preview_title, UiFont(), 0);
    lv_obj_set_style_text_color(preview_title, lv_color_black(), 0);
    lv_obj_set_style_text_align(preview_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_all(preview_title, 0, 0);
    // 文案已按像素拆成最多两行；CLIP 避免再走 LVGL 整词换行
    lv_label_set_long_mode(preview_title, LV_LABEL_LONG_CLIP);
    lv_label_set_text(preview_title, "");
    s_ui.preview_title = preview_title;

    lv_obj_t* preview_meta = lv_obj_create(preview_body);
    lv_obj_remove_style_all(preview_meta);
    lv_obj_set_width(preview_meta, ContentWidth());
    lv_obj_set_height(preview_meta, 36);
    lv_obj_set_flex_flow(preview_meta, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(preview_meta, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(preview_meta, 8, 0);
    lv_obj_clear_flag(preview_meta, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(preview_meta);
    s_ui.preview_meta = preview_meta;

    lv_obj_t* actions = lv_obj_create(preview_body);
    lv_obj_remove_style_all(actions);
    lv_obj_set_width(actions, ContentWidth());
    lv_obj_set_height(actions, kActionH);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(actions, 12, 0);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(actions);

    const lv_coord_t half_w = (ContentWidth() - 12) / 2;
    auto make_toggle = [&](lv_event_cb_t cb, const char* text, lv_obj_t** out_btn,
                           lv_obj_t** out_lbl) {
        lv_obj_t* btn = lv_obj_create(actions);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, half_w, kActionH);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        DisableScroll(btn);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, UiFont(), 0);
        lv_label_set_text(lbl, text);
        StyleActionBtn(btn, lbl, false, true);
        *out_btn = btn;
        *out_lbl = lbl;
    };
    make_toggle(OnEnableShutdownClicked, Lang::Strings::WALLPAPER_SET_SHUTDOWN, &s_ui.enable_shutdown_btn,
                &s_ui.enable_shutdown_lbl);
    make_toggle(OnEnableStandbyClicked, Lang::Strings::WALLPAPER_SET_STANDBY, &s_ui.enable_standby_btn,
                &s_ui.enable_standby_lbl);

    lv_obj_t* delete_btn = lv_obj_create(preview_body);
    lv_obj_remove_style_all(delete_btn);
    lv_obj_set_size(delete_btn, ContentWidth(), kDeleteH);
    lv_obj_set_style_pad_all(delete_btn, 0, 0);
    lv_obj_set_flex_flow(delete_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(delete_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    DisableScroll(delete_btn);
    HapticAttachClick(delete_btn);
    lv_obj_add_event_cb(delete_btn, OnDeleteClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* delete_lbl = lv_label_create(delete_btn);
    lv_obj_set_style_text_font(delete_lbl, UiFont(), 0);
    lv_label_set_text(delete_lbl, Lang::Strings::WALLPAPER_DELETE_BTN);
    StyleActionBtn(delete_btn, delete_lbl, false, true);
    s_ui.delete_btn = delete_btn;
    s_ui.delete_lbl = delete_lbl;

    // 底栏槽：页码与多选共用，不改网格区高度
    lv_obj_t* foot = lv_obj_create(scr);
    lv_obj_remove_style_all(foot);
    lv_obj_set_size(foot, LV_HOR_RES, kFooterH);
    lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(foot, LV_OPA_TRANSP, 0);
    DisableScroll(foot);
    lv_obj_clear_flag(foot, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* page_lbl = lv_label_create(foot);
    lv_obj_set_width(page_lbl, LV_HOR_RES - 16);
    lv_obj_set_style_text_align(page_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(page_lbl, UiFont(), 0);
    lv_obj_set_style_text_color(page_lbl, lv_color_black(), 0);
    lv_label_set_text(page_lbl, "1/1");
    lv_obj_align(page_lbl, LV_ALIGN_CENTER, 0, 0);
    DisableScroll(page_lbl);
    s_ui.page_lbl = page_lbl;

    s_ui.multi_bar = lv_obj_create(foot);
    lv_obj_remove_style_all(s_ui.multi_bar);
    lv_obj_set_width(s_ui.multi_bar, lv_pct(100));
    lv_obj_set_height(s_ui.multi_bar, kWpMultiBtnH);
    lv_obj_set_style_bg_opa(s_ui.multi_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(s_ui.multi_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_ui.multi_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_ui.multi_bar, 10, 0);
    lv_obj_align(s_ui.multi_bar, LV_ALIGN_CENTER, 0, 0);
    DisableScroll(s_ui.multi_bar);
    lv_obj_clear_flag(s_ui.multi_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.multi_bar, LV_OBJ_FLAG_HIDDEN);

    auto make_multi_action = [](lv_obj_t* parent, const char* text, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_obj_create(parent);
        lv_obj_remove_style_all(btn);
        lv_obj_set_height(btn, kWpMultiBtnH);
        lv_obj_set_width(btn, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_hor(btn, 4, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        DisableScroll(btn);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* text_wrap = lv_obj_create(btn);
        lv_obj_remove_style_all(text_wrap);
        lv_obj_set_size(text_wrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_border_side(text_wrap, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(text_wrap, kWpUnderlineH, 0);
        lv_obj_set_style_border_color(text_wrap, lv_color_black(), 0);
        lv_obj_set_style_pad_bottom(text_wrap, 2, 0);
        lv_obj_set_style_pad_hor(text_wrap, kWpUnderlinePadHor, 0);
        lv_obj_set_style_bg_opa(text_wrap, LV_OPA_TRANSP, 0);
        DisableScroll(text_wrap);
        lv_obj_clear_flag(text_wrap, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(text_wrap);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, UiFont(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        return btn;
    };
    auto make_multi_dot = [](lv_obj_t* parent) {
        lv_obj_t* dot = lv_obj_create(parent);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, kWpMultiDotSize, kWpMultiDotSize);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        DisableScroll(dot);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        return dot;
    };
    make_multi_action(s_ui.multi_bar, Lang::Strings::COMMON_CANCEL, OnWpMultiCancel);
    make_multi_dot(s_ui.multi_bar);
    make_multi_action(s_ui.multi_bar, Lang::Strings::COMMON_SELECT_ALL, OnWpMultiSelectAll);
    make_multi_dot(s_ui.multi_bar);
    make_multi_action(s_ui.multi_bar, Lang::Strings::COMMON_REMOVE, OnWpMultiRemove);

    s_screen_alive = true;
    RebuildListPage();
    ShowMode(UiMode::kList);
    // 后台灌 NVS → 缓存，再 lv_async 刷新关机/待机标记
    wallpaper::RequestHydrateFromNvs(OnActiveHydrated, nullptr);

    VkKey_AttachScreen(scr, kScreenId,
                       VkKeyScreenDesc{WallpaperScreen::Create, OnVkKey, nullptr, nullptr, nullptr,
                                       nullptr, OnVkKeyLongPress, OnVkKeyPressUp});
    return scr;
}
