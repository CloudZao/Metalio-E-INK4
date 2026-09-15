#include "cloud_screen.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "api_http.h"
#include "application.h"
#include "board.h"
#include "cloud_screen/push_resources_library.h"
#include "display.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "power_policy.h"
#include "reader/book_library.h"
#include "reader/download_gate.h"
#include "reader/image_util.h"
#include "reader/text_encoding.h"
#include "screen_common.h"
#include "vk_key_handler.h"
#include "vk_page_repeat.h"
#include "assets/lang_config.h"

namespace {

constexpr const char* TAG = "CloudScreen";
constexpr const char* kScreenId = "cloud";

constexpr lv_coord_t kPad = 12;
constexpr lv_coord_t kFooterH = 36;
constexpr lv_coord_t kCloudRowH = 80;
constexpr lv_coord_t kCloudRowLineGap = 6;
constexpr lv_coord_t kRowGap = 8;
constexpr lv_coord_t kThumbW = 48;
constexpr lv_coord_t kThumbH = 64;
constexpr lv_coord_t kThumbGap = 10;
constexpr int kThumbDecodeW = 48;
constexpr int kThumbDecodeH = 64;
constexpr size_t kThumbMaxDownloadBytes = 512 * 1024;
constexpr lv_coord_t kRowPad = 10;
constexpr lv_coord_t kRowBorderW = 2;
constexpr lv_coord_t kRowRadius = 8;
constexpr lv_coord_t kActionH = 56;
constexpr int kPreviewMaxW = 240;
constexpr int kPreviewMaxH = 400;
constexpr size_t kPreviewMaxDownloadBytes = 2 * 1024 * 1024;
constexpr uint32_t kCloudFetchStack = 8 * 1024;
constexpr uint32_t kWallpaperPreviewStack = 10 * 1024;
constexpr uint32_t kCoverThumbStack = 8 * 1024;
constexpr uint32_t kCloudDownloadStack = 8 * 1024;  // 内 DRAM：写 SD 关 cache 时禁 PSRAM 栈
constexpr uint32_t kCloudDeleteStack = 8 * 1024;
constexpr int64_t kSyncThrottleUs = 3000000;  // 手动刷新最短间隔 3s
constexpr lv_coord_t kSyncBtnH = 44;
constexpr lv_coord_t kSyncDividerW = 1;    // 底栏顶部分割线
constexpr lv_coord_t kSyncUnderlineH = 1;  // 「刷新」/多选动作下划线
constexpr lv_coord_t kSyncUnderlinePadHor = 5; // 下划线两端各外延；共比字宽加长 10px
constexpr lv_coord_t kMultiDotSize = 6;        // 分隔圆点直径（约 · 的 2 倍）
constexpr lv_coord_t kFooterBlockH = kSyncBtnH;  // 框内刷新/多选底栏
constexpr lv_coord_t kListFrameBorderW = 2;  // 列表+刷新细外框（不含页码）
constexpr lv_coord_t kListFrameRadius = 10;
constexpr lv_coord_t kListFramePad = 10;
constexpr lv_coord_t kTabH = 48;
constexpr lv_coord_t kTabBorderW = 3;
constexpr lv_coord_t kTabInset = 3;
constexpr lv_coord_t kHeaderH = 36;
constexpr lv_coord_t kHeaderIcon = 32;
constexpr lv_coord_t kHeaderGap = 8;
constexpr lv_coord_t kStatusGap = 12;  // 标题与拉取提示间距（同每日清单）
constexpr int kTabCount = 4;
constexpr lv_coord_t kChipRadius = 999;  // 预览元信息胶囊
constexpr lv_coord_t kChipH = 36;        // 略高于字高，避免贴边
constexpr lv_coord_t kChipPadH = 16;     // 左右内边距，边框大于文字
constexpr lv_coord_t kPreviewGap = 12;
constexpr lv_coord_t kPreviewImgMaxH = 420;  // 预览图框上限，避免占满整屏
constexpr lv_coord_t kFooterOutsideH = kFooterH;
constexpr uint32_t kStatusClearMs = 1200;
constexpr lv_coord_t kCheckSize = 28;  // 多选行尾勾选（同清单）
constexpr lv_coord_t kCheckGap = 10;
constexpr int64_t kSuppressRowClickUs = 400000;  // 只挡长按行松手假点击，不挡其它行连点
constexpr uint32_t kBatchStack = 16 * 1024;

const lv_font_t* UiFont() {
    const lv_font_t* f = fontpack_lv_font_get(30, 2);
    return f != nullptr ? f : fontpack_lv_font_ui();
}

const lv_font_t* ItemFont() {
    return fontpack_lv_font_ui();
}

lv_coord_t ContentWidth() {
    return LV_HOR_RES - kPad * 2;
}

void DisableScroll(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(obj, LV_DIR_NONE);
}

lv_coord_t MeasureTextWidth(const lv_font_t* font, const char* text) {
    if (font == nullptr || text == nullptr || text[0] == '\0') {
        return 0;
    }
    lv_point_t sz = {};
    lv_text_get_size(&sz, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return static_cast<lv_coord_t>(sz.x);
}

std::string EllipsizeText(const std::string& text, const lv_font_t* font, lv_coord_t max_w) {
    if (font == nullptr || max_w <= 0 || MeasureTextWidth(font, text.c_str()) <= max_w) {
        return text;
    }
    constexpr const char* kEllipsis = "…";
    const lv_coord_t suffix_w = MeasureTextWidth(font, kEllipsis);
    if (suffix_w >= max_w) {
        return kEllipsis;
    }
    const lv_coord_t budget = max_w - suffix_w;
    std::string out;
    out.reserve(text.size());
    const uint8_t* d = reinterpret_cast<const uint8_t*>(text.data());
    size_t i = 0;
    lv_coord_t used = 0;
    while (i < text.size()) {
        uint32_t cp = 0;
        const size_t n = reader::Utf8Next(d + i, text.size() - i, &cp);
        if (n == 0) {
            break;
        }
        lv_coord_t gw = static_cast<lv_coord_t>(lv_font_get_glyph_width(font, cp, 0));
        if (gw <= 0) {
            gw = 1;
        }
        if (used + gw > budget) {
            break;
        }
        out.append(text, i, n);
        used += gw;
        i += n;
    }
    out.append(kEllipsis);
    return out;
}

/**
 * 按像素逐字排满最多两行（末行可省略）。
 * 绕开 LVGL 在 LONG_WRAP 时「非行首英文整词不拆」导致的首行留白。
 */
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

void ShowMessage(lv_obj_t* parent, const char* msg) {
    if (parent == nullptr) {
        return;
    }
    lv_obj_clean(parent);
    lv_obj_set_style_layout(parent, LV_LAYOUT_NONE, 0);
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, msg != nullptr ? msg : "");
    lv_obj_set_style_text_font(label, UiFont(), 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, ContentWidth());
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_center(label);
    DisableScroll(label);
}

struct UiState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* main_body = nullptr;  // 列表区；预览时隐藏（页码贴底，不在此列）
    lv_obj_t* list_body = nullptr;
    lv_obj_t* footer = nullptr;  // 框内刷新/多选底栏
    lv_obj_t* sync_btn = nullptr;
    lv_obj_t* sync_lbl = nullptr;
    lv_obj_t* multi_bar = nullptr;  // 取消 / 全选 / 删除 / 保存
    lv_obj_t* page_lbl = nullptr;  // 贴底，对齐壁纸页码
    lv_obj_t* tab_btns[kTabCount] = {};
    lv_obj_t* tab_lbls[kTabCount] = {};
    lv_obj_t* section_title = nullptr;
    lv_obj_t* status_lbl = nullptr;
    lv_obj_t* wallpaper_preview_body = nullptr;
    lv_obj_t* wallpaper_preview_title = nullptr;  // 文件名，双行省略，在胶囊上方
    lv_obj_t* wallpaper_preview_meta = nullptr;   // 类型/大小胶囊行
    lv_obj_t* wallpaper_preview_status = nullptr;
    lv_obj_t* wallpaper_preview_img = nullptr;
    lv_obj_t* wallpaper_preview_img_host = nullptr;
    lv_obj_t* wallpaper_download_btn = nullptr;
    lv_obj_t* wallpaper_download_lbl = nullptr;
    uint32_t wallpaper_preview_epoch = 0;
    int wallpaper_preview_idx = -1;
    bool wallpaper_preview_open = false;
    reader::RasterImage* wallpaper_preview_raster = nullptr;

    std::vector<reader::CloudPushResource> items;
    std::vector<int> filtered;  // items 下标，按 Tab 筛选
    std::vector<uint8_t> selected;  // 与 items 等长；1=多选选中
    std::vector<std::string> batch_save_ids;  // 批量保存队列（taskId）
    std::vector<std::unique_ptr<reader::RasterImage>> thumbs;
    /** 与 items 等长：coverImageUrl 原始字节；列表缩略与详情/落旁路共用，避免二次 HTTP */
    std::vector<std::vector<uint8_t>> cover_bytes;
    uint32_t thumb_epoch = 0;
    int filter_tab = 0;  // 0全部 1壁纸 2书籍 3字体
    int page = 0;
    int page_size = 6;
    bool loading = false;
    bool waiting_net = false;
    bool fetch_done = false;
    bool covers_loading = false;  // 预览图全入 PSRAM 前不渲染列表行
    bool multi = false;
    bool batch_busy = false;
    bool delete_busy = false;  // 多选 DELETE push-resources 进行中
    int64_t suppress_row_click_until_us = 0;
    int suppress_row_click_idx = -1; // 与 until 配对：只吞该行松手 CLICKED
    std::string error;
    char status_text[80] = {};
    bool download_busy = false;
    bool dl_cancel_pending = false;  // 点行取消当前下载（断 HTTP）
    std::string dl_cancel_key;
    reader::PushResourceType download_type = reader::PushResourceType::kUnknown;
    std::string download_key;       // 当前下载 taskId
    uint32_t op_generation = 1;
    uint32_t download_gen = 0;
    int pending_index = -1;
    int action_index = -1;
    lv_obj_t* dialog_mask = nullptr;
    lv_obj_t* action_mask = nullptr;

    // 下载独占进度页（网点底）：单文件/批量共用
    lv_obj_t* dl_page = nullptr;
    lv_obj_t* dl_bar = nullptr;
    lv_obj_t* dl_pct_lbl = nullptr;
    lv_obj_t* dl_count_lbl = nullptr;
    lv_obj_t* dl_title_lbl = nullptr;
    bool dl_page_open = false;
    bool dl_user_abort = false;  // 进度页取消 / 返回：整单放弃，不再续下
    int dl_job_total = 0;
    int dl_job_done = 0;
    int dl_shown_percent = -1;
};

UiState& State() {
    static UiState s;
    return s;
}

lv_timer_t* s_status_clear_timer = nullptr;

void CancelStatusClearTimer() {
    if (s_status_clear_timer == nullptr) {
        return;
    }
    lv_timer_delete(s_status_clear_timer);
    s_status_clear_timer = nullptr;
}

void StatusClearTimerCb(lv_timer_t* /*t*/) {
    s_status_clear_timer = nullptr;
    auto& st = State();
    st.status_text[0] = '\0';
    if (st.status_lbl != nullptr) {
        lv_label_set_text(st.status_lbl, "");
        lv_obj_add_flag(st.status_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

void ApplyStatusTipUi() {
    auto& st = State();
    if (st.status_lbl == nullptr) {
        return;
    }
    if (st.status_text[0] == '\0') {
        lv_label_set_text(st.status_lbl, "");
        lv_obj_add_flag(st.status_lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(st.status_lbl, st.status_text);
    lv_obj_clear_flag(st.status_lbl, LV_OBJ_FLAG_HIDDEN);
}

void SetStatusTip(const char* text, bool auto_clear) {
    CancelStatusClearTimer();
    auto& st = State();
    if (text == nullptr || text[0] == '\0') {
        st.status_text[0] = '\0';
    } else {
        std::snprintf(st.status_text, sizeof(st.status_text), "%s", text);
    }
    ApplyStatusTipUi();
    if (auto_clear && st.status_text[0] != '\0') {
        s_status_clear_timer = lv_timer_create(StatusClearTimerCb, kStatusClearMs, nullptr);
        if (s_status_clear_timer != nullptr) {
            lv_timer_set_repeat_count(s_status_clear_timer, 1);
        }
    }
}

bool ItemMatchesTab(const reader::CloudPushResource& item, int tab) {
    if (tab <= 0) {
        return true;
    }
    if (tab == 1) {
        return item.type == reader::PushResourceType::kBadge;
    }
    if (tab == 2) {
        return item.type == reader::PushResourceType::kBook;
    }
    if (tab == 3) {
        return item.type == reader::PushResourceType::kFont;
    }
    return true;
}

void RebuildFiltered() {
    auto& st = State();
    st.filtered.clear();
    st.filtered.reserve(st.items.size());
    for (int i = 0; i < static_cast<int>(st.items.size()); ++i) {
        if (ItemMatchesTab(st.items[static_cast<size_t>(i)], st.filter_tab)) {
            st.filtered.push_back(i);
        }
    }
}

void RefreshSectionTitle() {
    auto& st = State();
    if (st.section_title == nullptr) {
        return;
    }
    const char* title = Lang::Strings::CLOUD_PENDING;
    if (st.filter_tab == 1) {
        title = Lang::Strings::CLOUD_PUSH_WALLPAPER;
    } else if (st.filter_tab == 2) {
        title = Lang::Strings::CLOUD_PUSH_BOOK;
    } else if (st.filter_tab == 3) {
        title = Lang::Strings::CLOUD_PUSH_FONT;
    }
    lv_label_set_text(st.section_title, title);
}

void StyleTabBtn(lv_obj_t* btn, lv_obj_t* lbl, bool on) {
    if (btn == nullptr) {
        return;
    }
    lv_obj_set_style_bg_color(btn, on ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    if (lbl != nullptr) {
        lv_obj_set_style_text_color(lbl, on ? lv_color_white() : lv_color_black(), 0);
    }
}

void RenderListPage();
void RequestCloudRender();
void CloseDialog();
void CloseActionDialog();

void RefreshTabUi() {
    auto& st = State();
    for (int i = 0; i < kTabCount; ++i) {
        StyleTabBtn(st.tab_btns[i], st.tab_lbls[i], st.filter_tab == i);
    }
    RefreshSectionTitle();
}

void SyncSelectedSize() {
    auto& st = State();
    if (st.selected.size() != st.items.size()) {
        st.selected.assign(st.items.size(), 0);
    }
}

int SelectedCount() {
    SyncSelectedSize();
    auto& st = State();
    int n = 0;
    for (uint8_t v : st.selected) {
        if (v != 0) {
            ++n;
        }
    }
    return n;
}

bool ItemSelected(int idx) {
    SyncSelectedSize();
    auto& st = State();
    return idx >= 0 && idx < static_cast<int>(st.selected.size()) &&
           st.selected[static_cast<size_t>(idx)] != 0;
}

void ToggleItemSelected(int idx) {
    SyncSelectedSize();
    auto& st = State();
    if (idx < 0 || idx >= static_cast<int>(st.selected.size())) {
        return;
    }
    st.selected[static_cast<size_t>(idx)] = st.selected[static_cast<size_t>(idx)] ? 0 : 1;
}

void RefreshFooterMode() {
    auto& st = State();
    if (st.multi) {
        if (st.sync_btn != nullptr) {
            lv_obj_add_flag(st.sync_btn, LV_OBJ_FLAG_HIDDEN);
        }
        if (st.multi_bar != nullptr) {
            lv_obj_clear_flag(st.multi_bar, LV_OBJ_FLAG_HIDDEN);
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), Lang::Strings::CLOUD_SELECTED_FMT, SelectedCount());
        SetStatusTip(buf, false);
    } else {
        if (st.multi_bar != nullptr) {
            lv_obj_add_flag(st.multi_bar, LV_OBJ_FLAG_HIDDEN);
        }
        if (st.sync_btn != nullptr) {
            lv_obj_clear_flag(st.sync_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ExitMultiModeEx(bool rebuild, bool clear_status) {
    auto& st = State();
    if (!st.multi && st.selected.empty()) {
        if (rebuild) {
            RequestCloudRender();
        }
        return;
    }
    st.multi = false;
    st.suppress_row_click_until_us = 0;
    st.suppress_row_click_idx = -1;
    st.selected.assign(st.items.size(), 0);
    if (clear_status) {
        SetStatusTip("", false);
    }
    RefreshFooterMode();
    if (rebuild) {
        RequestCloudRender();
    }
}

void ExitMultiMode(bool rebuild) {
    ExitMultiModeEx(rebuild, true);
}

void EnterMultiModeSelect(int idx) {
    auto& st = State();
    CloseActionDialog();
    CloseDialog();
    st.multi = true;
    SyncSelectedSize();
    st.selected.assign(st.items.size(), 0);
    if (idx >= 0 && idx < static_cast<int>(st.selected.size())) {
        st.selected[static_cast<size_t>(idx)] = 1;
    }
    RefreshFooterMode();
    RequestCloudRender();
}

void FillPreviewMeta(const reader::CloudPushResource* item) {
    auto& st = State();
    if (st.wallpaper_preview_meta == nullptr) {
        return;
    }
    lv_obj_clean(st.wallpaper_preview_meta);
    if (item == nullptr) {
        return;
    }
    auto make_chip = [&](const char* text) {
        lv_obj_t* chip = lv_obj_create(st.wallpaper_preview_meta);
        lv_obj_remove_style_all(chip);
        lv_obj_set_height(chip, kChipH);
        lv_obj_set_width(chip, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(chip, kChipPadH, 0);
        lv_obj_set_style_pad_ver(chip, 4, 0);
        lv_obj_set_style_border_width(chip, kRowBorderW, 0);
        lv_obj_set_style_border_color(chip, lv_color_black(), 0);
        lv_obj_set_style_radius(chip, kChipRadius, 0);
        lv_obj_set_style_bg_color(chip, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
        lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(chip, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(chip);
        lv_obj_t* lbl = lv_label_create(chip);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, UiFont(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    };
    make_chip(item->TypeLabel());
    make_chip(reader::FormatCloudFileSize(item->file_size).c_str());
}

bool ScreenAlive() {
    // 仅认指针：Delete/离页会置空。工作线程禁止 lv_obj_is_valid（无锁会 LoadProhibited）
    return State().screen != nullptr;
}

bool OpStillValid(uint32_t op_gen) {
    return op_gen != 0 && ScreenAlive() && State().op_generation == op_gen;
}

bool SyncBusy() {
    auto& st = State();
    return st.waiting_net || st.loading;
}

void RenderListPageInternal(bool schedule_thumbs);
void ScheduleCoverThumbFill();
lv_obj_t* FindListRow(int index);
lv_obj_t* FindRowChildBySize(lv_obj_t* row, lv_coord_t w, lv_coord_t h);
void PatchRowCheckMark(int index);
void PatchVisibleRowCheckMarks();
void RequestCheckMarksPaint();
void PatchRowThumb(int index);
int FindItemIndexByTaskId(const char* key);
void EraseItemAt(int idx);
void AbortDownloadJobFromUi();
void StopDlWorker();
void ShowDlProgressPage();
void HideDlProgressPage();
void RefreshDlProgressPage(int file_percent);
void EnsureDlProgressPageBuilt();
void ApplyStopTransferUi();
void RequestStopTransfer(const char* why);
void ScheduleCloudFetch();
void ScheduleNetPrepAndFetch();
void ScheduleUserCloudSync();
void OnSyncClicked(lv_event_t* e);
void OnTabClicked(lv_event_t* e);
void SetPageFooterText(const char* text);
void ContinueBatchSaveIfNeeded();
void ScheduleBatchContinueSoon(const char* why);
void AbortCloudSideHttpForDownload();  // 打断封面/预览 HTTP，详情页可仍打开
void ResumeCloudSideHttpAfterDl();     // 下载/批量全结束后续封面，并刷新仍打开的详情
bool CloudDlTasksActive();
void ShowPreviewWaitDlTip();
void ShowActionDialog(int cloud_index);
void StyleDownloadBtn(bool enabled);
void ClearWallpaperPreviewImage();
void CloseWallpaperPreview();
void OpenWallpaperPreview(int index);
void ShowWallpaperPreviewMode(bool preview);
void ScheduleLoadWallpaperPreview(int index);
void StartWallpaperDownload(int index);
void StartResourceDownload(int index);
void OnMultiCancel(lv_event_t* e);
void OnMultiSelectAll(lv_event_t* e);
void OnMultiSave(lv_event_t* e);
void OnMultiDelete(lv_event_t* e);
void ScheduleDeleteItems(std::vector<reader::CloudPushResource> items);

DownloadGate s_dl_gate{};
DownloadGate s_sync_gate{};  // 进页/手动拉列表刷新
int64_t s_last_user_sync_us = 0;  // 上次手动刷新；0=尚未点过
std::atomic<bool> s_ui_net_held{false};
std::atomic<bool> s_thumb_busy{false};
std::atomic<bool> s_thumb_abort{false};  // 下载开始时置位，封面 Read 循环退出
std::atomic<bool> s_dl_owns_http{false};  // 下载/批量独占 HTTP，禁止旁路 CreateHttp
std::atomic<bool> s_cloud_fetch_alive{false};  // 拉取任务仍在（含已 Invalidate 的僵尸）
TaskHandle_t s_net_prep_task = nullptr;
std::atomic<uint32_t> s_net_prep_attach_gen{0};  // 页挂接的 op_gen；复用等网时刷新
TaskHandle_t s_thumb_task = nullptr;
std::atomic<bool> s_wallpaper_preview_busy{false};
TaskHandle_t s_wallpaper_preview_task = nullptr;
TaskHandle_t s_delete_task = nullptr;

void ReleaseUiKeepNet() {
    if (s_ui_net_held.exchange(false)) {
        PowerPolicy::GetInstance().Release(PowerNeed::UiKeepNet);
    }
}

bool DownloadUrlToBuffer(const char* url, std::vector<uint8_t>& buf, size_t max_bytes) {
    buf.clear();
    if (url == nullptr || url[0] == '\0' || max_bytes == 0) {
        return false;
    }
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        return false;
    }
    auto http = network->CreateHttp(0);
    if (http == nullptr) {
        ESP_LOGW(TAG, "cover http CreateHttp failed int=%u psram=%u",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        return false;
    }

    http->SetTimeout(15000);
    api::ApplyCommonHeaders(http);
    if (!http->Open("GET", url)) {
        ESP_LOGW(TAG, "cover http Open fail");
        return false;
    }
    if (s_dl_owns_http.load(std::memory_order_acquire) ||
        s_thumb_abort.load(std::memory_order_acquire)) {
        http->Close();
        return false;
    }

    const int status = http->GetStatusCode();
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "cover http status=%d", status);
        http->Close();
        return false;
    }

    const size_t content_length = http->GetBodyLength();
    if (content_length > max_bytes) {
        ESP_LOGW(TAG, "cover http too large cl=%u max=%u", static_cast<unsigned>(content_length),
                 static_cast<unsigned>(max_bytes));
        http->Close();
        return false;
    }
    size_t cap = content_length > 0 ? content_length : max_bytes;
    if (cap > max_bytes) {
        cap = max_bytes;
    }

    // 读块放堆上，压低任务栈占用
    constexpr size_t kChunk = 1024;
    std::unique_ptr<char[]> chunk(new (std::nothrow) char[kChunk]);
    if (chunk == nullptr) {
        ESP_LOGE(TAG, "cover chunk alloc fail int=%u psram=%u",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        http->Close();
        return false;
    }
    while (buf.size() < cap) {
        if (s_dl_owns_http.load(std::memory_order_acquire) ||
            s_thumb_abort.load(std::memory_order_acquire)) {
            http->Close();
            buf.clear();
            return false;
        }
        const int n = http->Read(chunk.get(), kChunk);
        if (n < 0) {
            http->Close();
            buf.clear();
            return false;
        }
        if (n == 0) {
            break;
        }
        buf.insert(buf.end(), chunk.get(), chunk.get() + static_cast<size_t>(n));
    }
    http->Close();
    ESP_LOGI(TAG, "cover http done bytes=%u cl=%u", static_cast<unsigned>(buf.size()),
             static_cast<unsigned>(content_length));
    return !buf.empty();
}

int PageCount() {
    auto& st = State();
    if (st.filtered.empty() || st.page_size <= 0) {
        return 1;
    }
    return static_cast<int>((st.filtered.size() + static_cast<size_t>(st.page_size) - 1) /
                            static_cast<size_t>(st.page_size));
}

void ClampPage() {
    auto& st = State();
    const int pages = PageCount();
    if (st.page < 0) {
        st.page = 0;
    }
    if (st.page >= pages) {
        st.page = pages - 1;
    }
}

/** 离页：放下 waiting_net；保留 cloud_net 句柄以便再进复用（不叠任务） */
void DetachCloudNetPrep(const char* why) {
    State().waiting_net = false;
    if (s_net_prep_task == nullptr) {
        return;
    }
    ESP_LOGI(TAG,
             "DetachCloudNetPrep (%s): keep task=%p for reuse; RefreshNetworkWait on re-enter",
             why != nullptr ? why : "?", static_cast<void*>(s_net_prep_task));
}

/** Close 绑定 Http：必须丢到主循环，禁止在 LVGL 锁内 Disconnect join */
void AbortCloudGatesOffLvgl() {
    Application::GetInstance().Schedule([]() {
        s_dl_gate.AbortBoundHttp();
        s_sync_gate.AbortBoundHttp();
    });
}

void InvalidateSession() {
    auto& st = State();
    ESP_LOGI(TAG,
             "InvalidateSession: net_prep_task=%p waiting_net=%d loading=%d download_busy=%d "
             "op_gen=%u->%u",
             static_cast<void*>(s_net_prep_task), st.waiting_net ? 1 : 0, st.loading ? 1 : 0,
             st.download_busy ? 1 : 0, static_cast<unsigned>(st.op_generation),
             static_cast<unsigned>(st.op_generation + 1));
    // 停传输业务；等网任务可复用（勿杀句柄）
    DetachCloudNetPrep("InvalidateSession");
    s_dl_gate.RequestCancel();
    s_sync_gate.RequestCancel();
    AbortCloudGatesOffLvgl();
    CancelStatusClearTimer();
    ++st.op_generation;
    ++st.thumb_epoch;
    ++st.wallpaper_preview_epoch;
    st.thumbs.clear();
    st.filtered.clear();
    st.selected.clear();
    st.cover_bytes.clear();
    st.batch_save_ids.clear();
    st.status_text[0] = '\0';
    st.multi = false;
    st.batch_busy = false;
    st.delete_busy = false;
    st.suppress_row_click_until_us = 0;
    st.suppress_row_click_idx = -1;
    st.download_key.clear();
    st.dl_cancel_pending = false;
    st.dl_cancel_key.clear();
    // 封面/预览任务靠 epoch 自停并清 busy；勿在此空句柄（否则可叠任务）
    st.wallpaper_preview_idx = -1;
    st.wallpaper_preview_open = false;
    st.loading = false;
    st.waiting_net = false;
    st.covers_loading = false;
    st.download_busy = false;
    s_dl_owns_http.store(false, std::memory_order_release);
    st.dl_user_abort = false;
    st.dl_job_total = 0;
    st.dl_job_done = 0;
    HideDlProgressPage();
    st.download_type = reader::PushResourceType::kUnknown;
    st.download_gen = 0;
    st.download_key.clear();
    st.pending_index = -1;
    st.action_index = -1;
    StopDlWorker();
}

struct CloudFetchDoneMsg {
    uint32_t op_gen = 0;
    bool ok = false;
    std::vector<reader::CloudPushResource> items;
    char error[64] = {};
};

struct CloudFetchWork {
    uint32_t op_gen = 0;
};

struct CloudDownloadWork {
    uint32_t op_gen = 0;
    reader::CloudPushResource item;
    std::vector<uint8_t> cover_bytes;  // 列表已缓存的 coverImageUrl，供旁路免再 HTTP
};

struct CloudDownloadDoneMsg {
    uint32_t op_gen = 0;
    reader::PushResourceType type = reader::PushResourceType::kUnknown;
    bool ok = false;
    bool acked = false;
    bool cancelled = false;
    char key[160] = {};  // taskId
    char error[64] = {};
};

struct CloudDownloadProgressMsg {
    int percent = 0;
};

void AsyncStopTransferUi(void* /*user*/) {
    ApplyStopTransferUi();
}

void ApplyStopTransferUi() {
    auto& st = State();
    const bool was_busy =
        st.download_busy || st.batch_busy || st.delete_busy || st.dl_page_open;
    st.download_busy = false;
    st.download_gen = 0;
    st.download_key.clear();
    st.dl_cancel_pending = false;
    st.dl_cancel_key.clear();
    st.batch_save_ids.clear();
    st.batch_busy = false;
    st.delete_busy = false;
    st.dl_user_abort = false;
    st.dl_job_total = 0;
    st.dl_job_done = 0;
    s_dl_owns_http.store(false, std::memory_order_release);
    HideDlProgressPage();
    StopDlWorker();
    if (!ScreenAlive() || !was_busy) {
        return;
    }
    if (st.wallpaper_preview_open) {
        StyleDownloadBtn(true);
        return;
    }
    RenderListPage();
}

void RequestStopTransfer(const char* why) {
    ESP_LOGI(TAG, "stop transfer/sync (%s) net_prep_task=%p waiting_net=%d",
             why != nullptr ? why : "?", static_cast<void*>(s_net_prep_task),
             State().waiting_net ? 1 : 0);
    // 页业务停干净；底层 EnsureNetworkReady/扫网可继续
    DetachCloudNetPrep(why != nullptr ? why : "stop");
    // LVGL 锁内只置位；Close/join 丢主循环，避免踩坏 timer 链
    s_dl_gate.RequestCancel();
    s_sync_gate.RequestCancel();
    AbortCloudGatesOffLvgl();
    if (!ScreenLvAsync(AsyncStopTransferUi, nullptr)) {
        ApplyStopTransferUi();
    }
}

std::atomic<int64_t> s_dl_prog_last_ui_us{0};
constexpr int64_t kDlProgUiIntervalUs = 3000 * 1000;  // 进度页约 3s 刷一次

TaskHandle_t s_dl_task = nullptr;
QueueHandle_t s_dl_q = nullptr;  // CloudDownloadWork*；nullptr=毒丸退出

void CloudDownloadWorker(void* arg);

void DrainDlQueue() {
    if (s_dl_q == nullptr) {
        return;
    }
    CloudDownloadWork* w = nullptr;
    while (xQueueReceive(s_dl_q, &w, 0) == pdTRUE) {
        delete w;
    }
}

void StopDlWorker() {
    if (s_dl_task == nullptr) {
        return;
    }
    if (s_dl_q == nullptr) {
        return;
    }
    DrainDlQueue();
    CloudDownloadWork* poison = nullptr;
    xQueueSend(s_dl_q, &poison, 0);
}

bool EnqueueDlWork(CloudDownloadWork* work) {
    if (work == nullptr) {
        return false;
    }
    if (s_dl_q == nullptr) {
        s_dl_q = xQueueCreate(2, sizeof(CloudDownloadWork*));
        if (s_dl_q == nullptr) {
            return false;
        }
    }
    if (s_dl_task == nullptr) {
        if (xTaskCreatePinnedToCoreWithCaps(CloudDownloadWorker, "cloud_res_dl", kCloudDownloadStack,
                                            nullptr, tskIDLE_PRIORITY + 2, &s_dl_task, 0,
                                            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) != pdPASS) {
            s_dl_task = nullptr;
            return false;
        }
        ESP_LOGI(TAG, "cloud_res_dl task created (once)");
    }
    if (xQueueSend(s_dl_q, &work, 0) != pdTRUE) {
        return false;
    }
    return true;
}

int FindItemIndexByTaskId(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        return -1;
    }
    auto& st = State();
    for (int i = 0; i < static_cast<int>(st.items.size()); ++i) {
        if (st.items[static_cast<size_t>(i)].task_id == key) {
            return i;
        }
    }
    return -1;
}

void EraseItemAt(int idx) {
    auto& st = State();
    if (idx < 0 || idx >= static_cast<int>(st.items.size())) {
        return;
    }
    // 释放 RasterImage 前先清掉仍挂着 dsc 的缩略图，避免刷屏 LoadProhibited
    if (lv_obj_t* row = FindListRow(idx)) {
        if (lv_obj_t* thumb_box = FindRowChildBySize(row, kThumbW, kThumbH)) {
            lv_obj_clean(thumb_box);
        }
    }
    st.items.erase(st.items.begin() + idx);
    if (idx < static_cast<int>(st.thumbs.size())) {
        st.thumbs.erase(st.thumbs.begin() + idx);
    }
    if (idx < static_cast<int>(st.cover_bytes.size())) {
        st.cover_bytes.erase(st.cover_bytes.begin() + idx);
    }
    if (idx < static_cast<int>(st.selected.size())) {
        st.selected.erase(st.selected.begin() + idx);
    }
    if (st.wallpaper_preview_idx == idx) {
        st.wallpaper_preview_idx = -1;
    } else if (st.wallpaper_preview_idx > idx) {
        --st.wallpaper_preview_idx;
    }
    RebuildFiltered();
    ClampPage();
}

void OnDlProgressCancelClicked(lv_event_t* /*e*/) {
    AbortDownloadJobFromUi();
}

void EnsureDlProgressPageBuilt() {
    auto& st = State();
    if (st.dl_page != nullptr || st.screen == nullptr) {
        return;
    }

    lv_obj_t* page = lv_obj_create(st.screen);
    st.dl_page = page;
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(page, 0, 0);
    ScreenApplyDotBackdrop(page);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(page);

    lv_obj_t* card = lv_obj_create(page);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, LV_HOR_RES - 48, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, kRowBorderW, 0);
    lv_obj_set_style_radius(card, kRowRadius, 0);
    lv_obj_set_style_pad_all(card, 18, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 14, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, [](lv_event_t* e) { lv_event_stop_bubbling(e); }, LV_EVENT_CLICKED,
                        nullptr);

    st.dl_title_lbl = lv_label_create(card);
    lv_label_set_text(st.dl_title_lbl, Lang::Strings::CLOUD_SAVING);
    lv_obj_set_width(st.dl_title_lbl, LV_HOR_RES - 80);
    lv_obj_set_style_text_align(st.dl_title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(st.dl_title_lbl, UiFont(), 0);
    lv_obj_set_style_text_color(st.dl_title_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(st.dl_title_lbl, LV_OBJ_FLAG_CLICKABLE);

    st.dl_count_lbl = lv_label_create(card);
    lv_label_set_text(st.dl_count_lbl, Lang::Strings::CLOUD_SAVED_0_1);
    lv_obj_set_width(st.dl_count_lbl, LV_HOR_RES - 80);
    lv_obj_set_style_text_align(st.dl_count_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(st.dl_count_lbl, ItemFont(), 0);
    lv_obj_set_style_text_color(st.dl_count_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(st.dl_count_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_coord_t bar_w = 320;
    if (bar_w > LV_HOR_RES - 80) {
        bar_w = LV_HOR_RES - 80;
    }
    st.dl_bar = lv_bar_create(card);
    lv_obj_set_size(st.dl_bar, bar_w, 28);
    lv_bar_set_range(st.dl_bar, 0, 100);
    lv_bar_set_value(st.dl_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(st.dl_bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(st.dl_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(st.dl_bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(st.dl_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(st.dl_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(st.dl_bar, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(st.dl_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(st.dl_bar, 4, LV_PART_INDICATOR);
    lv_obj_clear_flag(st.dl_bar, LV_OBJ_FLAG_CLICKABLE);

    st.dl_pct_lbl = lv_label_create(card);
    lv_label_set_text(st.dl_pct_lbl, "0%");
    lv_obj_set_style_text_font(st.dl_pct_lbl, UiFont(), 0);
    lv_obj_set_style_text_color(st.dl_pct_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(st.dl_pct_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* cancel_btn = lv_obj_create(card);
    lv_obj_remove_style_all(cancel_btn);
    lv_obj_set_size(cancel_btn, 160, 44);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cancel_btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(cancel_btn, kRowBorderW, 0);
    lv_obj_set_style_radius(cancel_btn, kRowRadius, 0);
    lv_obj_add_flag(cancel_btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(cancel_btn);
    lv_obj_add_event_cb(cancel_btn, OnDlProgressCancelClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, Lang::Strings::COMMON_CANCEL);
    lv_obj_set_style_text_font(cancel_lbl, UiFont(), 0);
    lv_obj_center(cancel_lbl);
    lv_obj_clear_flag(cancel_lbl, LV_OBJ_FLAG_CLICKABLE);
}

void RefreshDlProgressPage(int file_percent) {
    auto& st = State();
    if (!st.dl_page_open || st.dl_page == nullptr) {
        return;
    }
    if (file_percent < 0) {
        file_percent = 0;
    }
    if (file_percent > 100) {
        file_percent = 100;
    }
    if (st.dl_count_lbl != nullptr) {
        char buf[48];
        const int total = st.dl_job_total > 0 ? st.dl_job_total : 1;
        const int done = st.dl_job_done < 0 ? 0 : st.dl_job_done;
        std::snprintf(buf, sizeof(buf), Lang::Strings::CLOUD_SAVED_FMT, done, total);
        lv_label_set_text(st.dl_count_lbl, buf);
    }
    if (file_percent == st.dl_shown_percent) {
        return;
    }
    st.dl_shown_percent = file_percent;
    if (st.dl_bar != nullptr) {
        lv_bar_set_value(st.dl_bar, file_percent, LV_ANIM_OFF);
    }
    if (st.dl_pct_lbl != nullptr) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d%%", file_percent);
        lv_label_set_text(st.dl_pct_lbl, buf);
    }
}

void ShowDlProgressPage() {
    auto& st = State();
    EnsureDlProgressPageBuilt();
    if (st.dl_page == nullptr) {
        return;
    }
    CloseDialog();
    CloseActionDialog();
    if (st.wallpaper_preview_open) {
        ++st.wallpaper_preview_epoch;
        st.wallpaper_preview_idx = -1;
        ClearWallpaperPreviewImage();
        ShowWallpaperPreviewMode(false);
    }
    st.dl_page_open = true;
    st.dl_shown_percent = -1;
    if (st.main_body != nullptr) {
        lv_obj_add_flag(st.main_body, LV_OBJ_FLAG_HIDDEN);
    }
    if (st.page_lbl != nullptr) {
        lv_obj_add_flag(st.page_lbl, LV_OBJ_FLAG_HIDDEN);
    }
    if (st.wallpaper_preview_body != nullptr) {
        lv_obj_add_flag(st.wallpaper_preview_body, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(st.dl_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(st.dl_page);
    // 遮罩盖住列表后下层热区仍按坐标早震；关掉热区早震，取消靠 PRESSED
    HapticSetZoneEarlyPulseEnabled(false);
    RefreshDlProgressPage(0);
}

void HideDlProgressPage() {
    auto& st = State();
    st.dl_page_open = false;
    st.dl_shown_percent = -1;
    HapticSetZoneEarlyPulseEnabled(true);
    if (st.dl_page != nullptr) {
        lv_obj_add_flag(st.dl_page, LV_OBJ_FLAG_HIDDEN);
    }
    if (st.main_body != nullptr && !st.wallpaper_preview_open) {
        lv_obj_clear_flag(st.main_body, LV_OBJ_FLAG_HIDDEN);
    }
    if (st.page_lbl != nullptr && !st.wallpaper_preview_open) {
        lv_obj_clear_flag(st.page_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

void AbortDownloadJobFromUi() {
    auto& st = State();
    if (!st.dl_page_open && !st.download_busy && !st.batch_busy) {
        return;
    }
    st.dl_user_abort = true;
    st.batch_save_ids.clear();
    if (st.download_busy && !st.dl_cancel_pending && !st.download_key.empty()) {
        st.dl_cancel_pending = true;
        st.dl_cancel_key = st.download_key;
        ESP_LOGI(TAG, "abort download job key=%s", st.dl_cancel_key.c_str());
        s_dl_gate.RequestCancel();
        Application::GetInstance().Schedule([]() { s_dl_gate.AbortBoundHttp(); });
        if (st.dl_title_lbl != nullptr) {
            lv_label_set_text(st.dl_title_lbl, Lang::Strings::CLOUD_CANCELLING);
        }
        return;
    }
    st.batch_busy = false;
    st.download_busy = false;
    st.dl_cancel_pending = false;
    st.dl_cancel_key.clear();
    HideDlProgressPage();
    SetStatusTip(Lang::Strings::CLOUD_CANCELLED, true);
    RenderListPage();
    ResumeCloudSideHttpAfterDl();
    StopDlWorker();
}

void ShowRowDownloadProgress(int percent) {
    auto& st = State();
    if (!st.download_busy || st.download_gen != st.op_generation || !ScreenAlive()) {
        return;
    }
    RefreshDlProgressPage(percent);
}

void ScheduleBatchContinueSoon(const char* why) {
    ESP_LOGI(TAG, "batch continue scheduled (%s)", why != nullptr ? why : "");
    Application::GetInstance().Schedule([]() {
        lv_timer_t* t = lv_timer_create(
            [](lv_timer_t* timer) {
                if (timer != nullptr) {
                    lv_timer_set_user_data(timer, nullptr);
                }
                auto& st2 = State();
                if (!ScreenAlive() || !st2.batch_busy) {
                    return;
                }
                ESP_LOGI(TAG, "batch continue after settle");
                ContinueBatchSaveIfNeeded();
            },
            150, nullptr);
        if (t == nullptr) {
            ESP_LOGW(TAG, "batch continue timer create failed");
            ContinueBatchSaveIfNeeded();
            return;
        }
        lv_timer_set_repeat_count(t, 1);
    });
}

bool CloudDlTasksActive() {
    const auto& st = State();
    return st.download_busy || st.batch_busy || st.delete_busy;
}

void ShowPreviewWaitDlTip() {
    auto& st = State();
    if (st.wallpaper_preview_status != nullptr) {
        lv_label_set_text(st.wallpaper_preview_status, Lang::Strings::CLOUD_WAIT_DOWNLOAD);
        lv_obj_clear_flag(st.wallpaper_preview_status, LV_OBJ_FLAG_HIDDEN);
    }
    StyleDownloadBtn(true);
}

void AbortCloudSideHttpForDownload() {
    auto& st = State();
    s_dl_owns_http.store(true, std::memory_order_release);
    s_thumb_abort.store(true, std::memory_order_release);
    ++st.thumb_epoch;
    // 打断预览加载任务，详情页保持打开并提示等待
    ++st.wallpaper_preview_epoch;
    if (st.wallpaper_preview_open) {
        ClearWallpaperPreviewImage();
        ShowPreviewWaitDlTip();
    }
}

void ResumeCloudSideHttpAfterDl() {
    auto& st = State();
    if (!ScreenAlive() || CloudDlTasksActive()) {
        return;
    }
    s_dl_owns_http.store(false, std::memory_order_release);
    ScheduleCoverThumbFill();
    if (st.wallpaper_preview_open && st.wallpaper_preview_idx >= 0) {
        if (st.wallpaper_preview_status != nullptr) {
            lv_label_set_text(st.wallpaper_preview_status, Lang::Strings::CLOUD_LOADING);
            lv_obj_clear_flag(st.wallpaper_preview_status, LV_OBJ_FLAG_HIDDEN);
        }
        ScheduleLoadWallpaperPreview(st.wallpaper_preview_idx);
    }
}

void AsyncDownloadProgress(void* user_data) {
    auto* msg = static_cast<CloudDownloadProgressMsg*>(user_data);
    ShowRowDownloadProgress(msg->percent);
    delete msg;
}

void OnDownloadProgress(int percent, void* /*user*/) {
    // 仅在 cloud_res_dl 任务调用：禁止碰 State()（与 LVGL 并发会踩堆）
    if (s_dl_gate.IsCancelled()) {
        return;
    }
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }

    const int64_t now_us = esp_timer_get_time();
    const int64_t last_us = s_dl_prog_last_ui_us.load(std::memory_order_relaxed);
    const bool force = (percent <= 0) || (percent >= 99);
    if (!force && last_us != 0 && (now_us - last_us) < kDlProgUiIntervalUs) {
        return;
    }
    s_dl_prog_last_ui_us.store(now_us, std::memory_order_relaxed);

    auto* msg = new CloudDownloadProgressMsg{};
    msg->percent = percent;
    // cloud_res_dl 非 LVGL：裸 lv_async_call 会与 timer_handler 竞态踩堆
    if (!ScreenLvAsync(AsyncDownloadProgress, msg)) {
        delete msg;
    }
}

void CloseActionDialog() {
    auto& st = State();
    if (st.action_mask != nullptr && ScreenAlive()) {
        lv_obj_delete(st.action_mask);
    }
    st.action_mask = nullptr;
    st.action_index = -1;
}

void OnActionCancelClicked(lv_event_t* /*e*/) {
    CloseActionDialog();
}

struct CloudDeleteWork {
    uint32_t op_gen = 0;
    std::vector<reader::CloudPushResource> items;
};

struct CloudDeleteDoneMsg {
    uint32_t op_gen = 0;
    int ok_n = 0;
    int fail_n = 0;
    std::vector<std::string> ok_ids;
    char error[96] = {};
};

void AsyncApplyDelete(void* p) {
    auto* msg = static_cast<CloudDeleteDoneMsg*>(p);
    auto& st = State();
    st.delete_busy = false;
    if (!OpStillValid(msg->op_gen)) {
        delete msg;
        ResumeCloudSideHttpAfterDl();
        return;
    }
    // 批量摘行前先拆掉列表行（含缩略图控件），再释放 RasterImage。
    // 若只 Erase 后走 coalesce 延迟刷，中间 refr 会画悬空 lv_image src → LoadProhibited。
    if (st.list_body != nullptr) {
        lv_obj_clean(st.list_body);
    }
    for (const auto& id : msg->ok_ids) {
        const int idx = FindItemIndexByTaskId(id.c_str());
        if (idx >= 0) {
            EraseItemAt(idx);
        }
    }
    char buf[64];
    if (msg->ok_n > 0 && msg->fail_n == 0) {
        std::snprintf(buf, sizeof(buf), Lang::Strings::TASK_REMOVED_N_FMT, msg->ok_n);
        SetStatusTip(buf, true);
    } else if (msg->ok_n > 0) {
        std::snprintf(buf, sizeof(buf), Lang::Strings::TASK_BATCH_RESULT_FMT, msg->ok_n,
                      msg->fail_n);
        SetStatusTip(buf, true);
    } else {
        SetStatusTip(msg->error[0] != '\0' ? msg->error : Lang::Strings::CLOUD_DELETE_FAIL, true);
    }
    ExitMultiModeEx(false, false);
    RenderListPage();  // 同步重建，勿 RequestCloudRender 合并延迟
    ResumeCloudSideHttpAfterDl();
    delete msg;
}

void CloudDeleteWorker(void* arg) {
    auto* work = static_cast<CloudDeleteWork*>(arg);
    auto* msg = new CloudDeleteDoneMsg{};
    msg->op_gen = work->op_gen;
    for (const auto& item : work->items) {
        std::string err;
        bool ok = reader::DeletePushResourceRemote(item, err);
        if (!ok) {
            vTaskDelay(pdMS_TO_TICKS(400));
            err.clear();
            ok = reader::DeletePushResourceRemote(item, err);
        }
        if (ok) {
            msg->ok_ids.push_back(item.task_id);
            ++msg->ok_n;
        } else {
            ++msg->fail_n;
            if (msg->error[0] == '\0') {
                std::snprintf(msg->error, sizeof(msg->error), "%s",
                              err.empty() ? Lang::Strings::CLOUD_DELETE_FAIL : err.c_str());
            }
        }
    }
    delete work;
    s_delete_task = nullptr;
    if (!ScreenLvAsync(AsyncApplyDelete, msg)) {
        ESP_LOGW(TAG, "ScreenLvAsync delete failed");
        const uint32_t gen = msg->op_gen;
        delete msg;
        if (ScreenAlive() && State().op_generation == gen) {
            State().delete_busy = false;
        }
    }
    vTaskDelete(nullptr);
}

void ScheduleDeleteItems(std::vector<reader::CloudPushResource> items) {
    auto& st = State();
    if (items.empty()) {
        SetStatusTip(Lang::Strings::CLOUD_SELECT_FIRST, true);
        return;
    }
    if (st.delete_busy || CloudDlTasksActive() || SyncBusy() || s_delete_task != nullptr) {
        SetStatusTip(Lang::Strings::CLOUD_PLEASE_WAIT, true);
        return;
    }
    auto* work = new CloudDeleteWork{};
    work->op_gen = st.op_generation;
    work->items = std::move(items);
    st.delete_busy = true;
    st.multi = false;
    RefreshFooterMode();
    SetStatusTip(Lang::Strings::CLOUD_PLEASE_WAIT, false);
    // 与下载相同：打断封面 HTTP，避免删行时仍回写悬空缩略图
    AbortCloudSideHttpForDownload();
    if (xTaskCreatePinnedToCore(CloudDeleteWorker, "cloud_res_del", kCloudDeleteStack, work,
                                tskIDLE_PRIORITY + 2, &s_delete_task, 0) != pdPASS) {
        ESP_LOGW(TAG, "delete task create failed");
        delete work;
        s_delete_task = nullptr;
        st.delete_busy = false;
        SetStatusTip(Lang::Strings::CLOUD_DELETE_FAIL, true);
        ResumeCloudSideHttpAfterDl();
        return;
    }
}

void OnDeleteClicked(lv_event_t* /*e*/) {
    auto& st = State();
    const int index = st.action_index;
    CloseActionDialog();
    if (index < 0 || index >= static_cast<int>(st.items.size())) {
        return;
    }
    std::vector<reader::CloudPushResource> items;
    items.push_back(st.items[static_cast<size_t>(index)]);
    ScheduleDeleteItems(std::move(items));
}

void ShowActionDialog(int cloud_index) {
    auto& st = State();
    if (!ScreenAlive() || st.action_mask != nullptr || st.dialog_mask != nullptr ||
        st.download_busy || st.delete_busy || SyncBusy()) {
        return;
    }
    if (cloud_index < 0 || cloud_index >= static_cast<int>(st.items.size())) {
        return;
    }

    const reader::CloudPushResource& item = st.items[static_cast<size_t>(cloud_index)];
    char title_buf[160];
    std::snprintf(title_buf, sizeof(title_buf), "「%s」\n%s", item.name.c_str(), item.TypeLabel());
    st.action_index = cloud_index;

    lv_obj_t* mask = lv_obj_create(st.screen);
    st.action_mask = mask;
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(mask, 0, 0);
    ScreenApplyDotBackdrop(mask);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(mask);
    lv_obj_add_event_cb(mask, OnActionCancelClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* card = lv_obj_create(mask);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, LV_HOR_RES - 48, 220);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, kRowBorderW, 0);
    lv_obj_set_style_radius(card, kRowRadius, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, [](lv_event_t* e) { lv_event_stop_bubbling(e); }, LV_EVENT_CLICKED,
                        nullptr);

    lv_obj_t* lbl = lv_label_create(card);
    lv_label_set_text(lbl, title_buf);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl, UiFont(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* btn_row = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, 48);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_CLICKABLE);

    auto make_btn = [&](const char* text, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_obj_create(btn_row);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 120, 44);
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, lv_color_black(), 0);
        lv_obj_set_style_border_width(btn, kRowBorderW, 0);
        lv_obj_set_style_radius(btn, kRowRadius, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* bl = lv_label_create(btn);
        lv_label_set_text(bl, text);
        lv_obj_set_style_text_font(bl, UiFont(), 0);
        lv_obj_center(bl);
        lv_obj_clear_flag(bl, LV_OBJ_FLAG_CLICKABLE);
    };
    make_btn(Lang::Strings::COMMON_DELETE, OnDeleteClicked);
    make_btn(Lang::Strings::COMMON_CANCEL, OnActionCancelClicked);
}

void CloseDialog() {
    auto& st = State();
    if (st.dialog_mask != nullptr && ScreenAlive()) {
        lv_obj_delete(st.dialog_mask);
    }
    st.dialog_mask = nullptr;
    st.pending_index = -1;
}

void OnDownloadNoClicked(lv_event_t* /*e*/) {
    CloseDialog();
}

void AsyncDownloadDone(void* user_data) {
    auto* done = static_cast<CloudDownloadDoneMsg*>(user_data);
    auto& st = State();
    if (!OpStillValid(done->op_gen)) {
        st.batch_save_ids.clear();
        st.batch_busy = false;
        st.download_busy = false;
        st.download_gen = 0;
        st.download_key.clear();
        st.dl_cancel_pending = false;
        st.dl_cancel_key.clear();
        st.dl_user_abort = false;
        HideDlProgressPage();
        delete done;
        ResumeCloudSideHttpAfterDl();
        StopDlWorker();
        return;
    }

    // 取消：断当前 HTTP；用户整单取消则不再续下
    if (done->cancelled && st.dl_cancel_pending && st.dl_cancel_key == done->key) {
        const bool user_abort = st.dl_user_abort;
        const bool batch = st.batch_busy && !user_abort;
        char done_key[160];
        std::snprintf(done_key, sizeof(done_key), "%s", done->key);
        st.dl_cancel_pending = false;
        st.dl_cancel_key.clear();
        st.download_busy = false;
        st.download_gen = 0;
        st.download_key.clear();
        st.download_type = reader::PushResourceType::kUnknown;

        if (user_abort) {
            st.batch_save_ids.clear();
            st.batch_busy = false;
            st.dl_user_abort = false;
            HideDlProgressPage();
            SetStatusTip(Lang::Strings::CLOUD_CANCELLED, true);
            RenderListPage();
            ResumeCloudSideHttpAfterDl();
            StopDlWorker();
        } else if (batch) {
            if (!st.batch_save_ids.empty() && st.batch_save_ids.front() == done_key) {
                st.batch_save_ids.erase(st.batch_save_ids.begin());
            }
            st.batch_save_ids.erase(
                std::remove(st.batch_save_ids.begin(), st.batch_save_ids.end(),
                            std::string(done_key)),
                st.batch_save_ids.end());
            ScheduleBatchContinueSoon("cancel");
        } else {
            HideDlProgressPage();
            RenderListPage();
            ResumeCloudSideHttpAfterDl();
            StopDlWorker();
        }
        delete done;
        return;
    }

    if (done->cancelled || done->op_gen != st.download_gen) {
        st.batch_save_ids.clear();
        st.batch_busy = false;
        st.download_busy = false;
        st.download_gen = 0;
        st.download_key.clear();
        st.dl_cancel_pending = false;
        st.dl_cancel_key.clear();
        st.dl_user_abort = false;
        HideDlProgressPage();
        delete done;
        ResumeCloudSideHttpAfterDl();
        if (ScreenAlive()) {
            RenderListPage();
        }
        StopDlWorker();
        return;
    }
    const bool batch = st.batch_busy && !st.dl_user_abort;
    char done_key[160];
    std::snprintf(done_key, sizeof(done_key), "%s", done->key);
    st.download_busy = false;
    st.download_gen = 0;
    st.download_key.clear();
    st.download_type = reader::PushResourceType::kUnknown;
    st.dl_cancel_pending = false;
    st.dl_cancel_key.clear();

    if (!done->ok) {
        st.batch_save_ids.clear();
        st.batch_busy = false;
        st.dl_user_abort = false;
        ExitMultiModeEx(false, false);
        HideDlProgressPage();
        SetStatusTip(done->error[0] != '\0' ? done->error : Lang::Strings::CLOUD_SAVE_FAIL, true);
        RenderListPage();
        ResumeCloudSideHttpAfterDl();
        delete done;
        StopDlWorker();
        return;
    }

    st.error.clear();
    ++st.dl_job_done;
    RefreshDlProgressPage(100);
    const int idx = FindItemIndexByTaskId(done_key);
    if (done->acked && idx >= 0) {
        EraseItemAt(idx);
    } else if (idx >= 0) {
        SetStatusTip(Lang::Strings::CLOUD_SAVED_SYNC_FAIL, true);
    }
    if (!batch) {
        st.dl_user_abort = false;
        HideDlProgressPage();
        RenderListPage();
        ResumeCloudSideHttpAfterDl();
        StopDlWorker();
    }
    delete done;

    if (batch) {
        if (!st.batch_save_ids.empty() && st.batch_save_ids.front() == done_key) {
            st.batch_save_ids.erase(st.batch_save_ids.begin());
        }
        ScheduleBatchContinueSoon("ok");
        return;
    }
}

void ContinueBatchSaveIfNeeded() {
    auto& st = State();
    if (st.dl_user_abort) {
        st.batch_save_ids.clear();
        st.batch_busy = false;
        st.dl_user_abort = false;
        HideDlProgressPage();
        SetStatusTip(Lang::Strings::CLOUD_CANCELLED, true);
        RenderListPage();
        ResumeCloudSideHttpAfterDl();
        StopDlWorker();
        return;
    }
    while (!st.batch_save_ids.empty()) {
        const std::string id = st.batch_save_ids.front();
        const int idx = FindItemIndexByTaskId(id.c_str());
        if (idx < 0) {
            st.batch_save_ids.erase(st.batch_save_ids.begin());
            continue;
        }
        StartResourceDownload(idx);
        return;
    }
    st.batch_busy = false;
    ExitMultiModeEx(false, false);
    HideDlProgressPage();
    SetStatusTip("", false);
    RenderListPage();
    ResumeCloudSideHttpAfterDl();
    StopDlWorker();
}

void CloudDownloadWorker(void* /*arg*/) {
    // PowerNeedHold 须在 vTaskDelete 前析构（任务自杀不跑栈析构）
    {
        PowerNeedHold hold_net(PowerNeed::OtaDownload);
        for (;;) {
            CloudDownloadWork* work = nullptr;
            if (xQueueReceive(s_dl_q, &work, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            if (work == nullptr) {
                break;
            }

            auto* msg = new CloudDownloadDoneMsg{};
            msg->op_gen = work->op_gen;
            msg->type = work->item.type;
            std::string err;
            std::snprintf(msg->key, sizeof(msg->key), "%s", work->item.task_id.c_str());
            msg->ok = reader::DownloadPushResource(work->item, err, OnDownloadProgress, nullptr,
                                                   &s_dl_gate,
                                                   work->cover_bytes.empty()
                                                       ? nullptr
                                                       : work->cover_bytes.data(),
                                                   work->cover_bytes.size());
            msg->cancelled = s_dl_gate.IsCancelled() || err == Lang::Strings::CLOUD_CANCELLED;
            if (msg->cancelled) {
                ESP_LOGI(TAG, "download stopped key=%s", msg->key);
            }
            if (msg->ok) {
                std::string ack_err;
                msg->acked = reader::AckPushResourceDownloaded(work->item, ack_err);
                if (!msg->acked) {
                    ESP_LOGW(TAG, "ack push resource failed taskId=%s err=%s",
                             work->item.task_id.c_str(), ack_err.c_str());
                }
            } else if (!msg->cancelled) {
                std::snprintf(msg->error, sizeof(msg->error), "%s",
                              err.empty() ? Lang::Strings::CLOUD_DOWNLOAD_FAIL : err.c_str());
            }
            delete work;

            if (!ScreenLvAsync(AsyncDownloadDone, msg)) {
                delete msg;
            }
        }
    }
    s_dl_task = nullptr;
    vTaskDelete(nullptr);
}

void StartResourceDownload(int index) {
    auto& st = State();
    if (st.download_busy || SyncBusy()) {
        return;
    }
    if (index < 0 || index >= static_cast<int>(st.items.size())) {
        return;
    }

    auto* work = new CloudDownloadWork{};
    work->op_gen = st.op_generation;
    work->item = st.items[static_cast<size_t>(index)];
    if (index < static_cast<int>(st.cover_bytes.size()) &&
        !st.cover_bytes[static_cast<size_t>(index)].empty()) {
        work->cover_bytes = st.cover_bytes[static_cast<size_t>(index)];
    }

    s_dl_gate.Reset();
    if (st.wallpaper_preview_open) {
        ++st.wallpaper_preview_epoch;
        st.wallpaper_preview_idx = -1;
        ClearWallpaperPreviewImage();
        ShowWallpaperPreviewMode(false);
    }
    if (!st.dl_page_open) {
        st.dl_job_total = st.batch_busy ? static_cast<int>(st.batch_save_ids.size()) : 1;
        if (st.dl_job_total < 1) {
            st.dl_job_total = 1;
        }
        st.dl_job_done = 0;
        st.dl_user_abort = false;
        ShowDlProgressPage();
    } else {
        RefreshDlProgressPage(0);
    }
    if (st.dl_title_lbl != nullptr) {
        const std::string shown =
            EllipsizeText(work->item.name, UiFont(), LV_HOR_RES - 80);
        char title[192];
        std::snprintf(title, sizeof(title), Lang::Strings::CLOUD_SAVING_NAME_FMT,
                      shown.empty() ? "…" : shown.c_str());
        lv_label_set_text(st.dl_title_lbl, title);
    }
    AbortCloudSideHttpForDownload();
    st.download_busy = true;
    st.download_type = work->item.type;
    st.download_gen = work->op_gen;
    st.download_key = work->item.task_id;
    st.dl_cancel_pending = false;
    st.dl_cancel_key.clear();
    s_dl_prog_last_ui_us.store(0, std::memory_order_relaxed);

    if (!EnqueueDlWork(work)) {
        delete work;
        st.download_busy = false;
        st.download_gen = 0;
        st.download_key.clear();
        st.download_type = reader::PushResourceType::kUnknown;
        st.batch_save_ids.clear();
        st.batch_busy = false;
        st.dl_user_abort = false;
        HideDlProgressPage();
        SetStatusTip(Lang::Strings::CLOUD_TASK_FAIL, true);
        RenderListPage();
        ResumeCloudSideHttpAfterDl();
        StopDlWorker();
    }
}

void OnDownloadYesClicked(lv_event_t* /*e*/) {
    auto& st = State();
    const int index = st.pending_index;
    CloseDialog();
    StartResourceDownload(index);
}

void ShowDownloadDialog(int cloud_index) {
    auto& st = State();
    if (!ScreenAlive() || st.dialog_mask != nullptr || st.action_mask != nullptr ||
        st.download_busy || SyncBusy()) {
        return;
    }
    if (cloud_index < 0 || cloud_index >= static_cast<int>(st.items.size())) {
        return;
    }

    const reader::CloudPushResource& item = st.items[static_cast<size_t>(cloud_index)];
    char msg[192];
    std::snprintf(msg, sizeof(msg), Lang::Strings::CLOUD_CONFIRM_DL_FMT, item.name.c_str(),
                  item.TypeLabel(), reader::FormatCloudFileSize(item.file_size).c_str());
    st.pending_index = cloud_index;

    lv_obj_t* mask = lv_obj_create(st.screen);
    st.dialog_mask = mask;
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(mask, 0, 0);
    ScreenApplyDotBackdrop(mask);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(mask, OnDownloadNoClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* card = lv_obj_create(mask);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, LV_HOR_RES - 48, 240);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, kRowBorderW, 0);
    lv_obj_set_style_radius(card, kRowRadius, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, [](lv_event_t* e) { lv_event_stop_bubbling(e); }, LV_EVENT_CLICKED,
                        nullptr);

    lv_obj_t* lbl = lv_label_create(card);
    lv_label_set_text(lbl, msg);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl, UiFont(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* btn_row = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, 48);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_CLICKABLE);

    auto make_btn = [&](const char* text, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_obj_create(btn_row);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 120, 44);
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, lv_color_black(), 0);
        lv_obj_set_style_border_width(btn, kRowBorderW, 0);
        lv_obj_set_style_radius(btn, kRowRadius, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* bl = lv_label_create(btn);
        lv_label_set_text(bl, text);
        lv_obj_set_style_text_font(bl, UiFont(), 0);
        lv_obj_center(bl);
        lv_obj_clear_flag(bl, LV_OBJ_FLAG_CLICKABLE);
    };
    make_btn(Lang::Strings::CLOUD_DOWNLOAD, OnDownloadYesClicked);
    make_btn(Lang::Strings::COMMON_CANCEL, OnDownloadNoClicked);
}

void OnRowLongPressed(lv_event_t* e) {
    auto& st = State();
    if (st.dl_page_open || st.download_busy || st.batch_busy || st.delete_busy || SyncBusy() ||
        st.dialog_mask != nullptr || st.action_mask != nullptr || st.wallpaper_preview_open) {
        return;
    }
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (index < 0 || index >= static_cast<int>(st.items.size())) {
        return;
    }
    if (!st.multi) {
        st.suppress_row_click_idx = index;
        st.suppress_row_click_until_us = esp_timer_get_time() + kSuppressRowClickUs;
        EnterMultiModeSelect(index);
        return;
    }
    ToggleItemSelected(index);
    RequestCheckMarksPaint();
}

void OnRowClicked(lv_event_t* e) {
    auto& st = State();
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (index == st.suppress_row_click_idx &&
        esp_timer_get_time() < st.suppress_row_click_until_us) {
        return;
    }
    if (st.dl_page_open || SyncBusy() || st.dialog_mask != nullptr || st.action_mask != nullptr) {
        return;
    }
    if (index < 0 || index >= static_cast<int>(st.items.size())) {
        return;
    }
    if (st.multi) {
        ToggleItemSelected(index);
        RequestCheckMarksPaint();
        return;
    }
    OpenWallpaperPreview(index);
}

void AsyncFetchDone(void* p) {
    auto* msg = static_cast<CloudFetchDoneMsg*>(p);
    auto& st = State();
    if (!OpStillValid(msg->op_gen)) {
        delete msg;
        // 僵尸拉取结束：若当前页仍要列表则重开（勿在 Reset 清 cancel 后与下载叠跑）
        if (ScreenAlive() && !st.fetch_done && !st.download_busy && !SyncBusy() &&
            !s_cloud_fetch_alive.load(std::memory_order_acquire)) {
            ScheduleNetPrepAndFetch();
        }
        return;
    }
    st.loading = false;
    if (msg->ok) {
        st.items = std::move(msg->items);
        st.thumbs.clear();
        st.thumbs.resize(st.items.size());
        st.cover_bytes.clear();
        st.cover_bytes.resize(st.items.size());
        ++st.thumb_epoch;
        st.error.clear();
        st.page = 0;
        RebuildFiltered();
        ESP_LOGI(TAG, "sync done ok n=%u", static_cast<unsigned>(st.items.size()));
        // 先全量拉预览图再出列表，翻页不再走封面 HTTP
        st.covers_loading = !st.items.empty();
        SetStatusTip(st.items.empty() ? "" : Lang::Strings::CLOUD_LOAD_COVER, false);
    } else {
        st.items.clear();
        st.filtered.clear();
        st.thumbs.clear();
        st.cover_bytes.clear();
        ++st.thumb_epoch;
        st.covers_loading = false;
        st.error = msg->error[0] != '\0' ? msg->error : Lang::Strings::CLOUD_SYNC_FAIL;
        ESP_LOGW(TAG, "sync done fail err=%s", st.error.c_str());
        SetStatusTip("", false);
    }
    st.fetch_done = true;
    ExitMultiModeEx(false, false);
    SyncSelectedSize();
    RenderListPage();
    if (st.covers_loading) {
        ScheduleCoverThumbFill();
    }
    delete msg;
}

struct SyncStoppedMsg {
    uint32_t op_gen = 0;
};

void AsyncSyncStopped(void* p) {
    auto* msg = static_cast<SyncStoppedMsg*>(p);
    auto& st = State();
    if (msg == nullptr) {
        return;
    }
    if (OpStillValid(msg->op_gen)) {
        st.loading = false;
        SetStatusTip("", false);
        RenderListPage();
        if (!st.fetch_done && !SyncBusy() && !st.download_busy) {
            ScheduleNetPrepAndFetch();
        }
    } else if (ScreenAlive() && !st.fetch_done && !st.download_busy && !SyncBusy() &&
               !s_cloud_fetch_alive.load(std::memory_order_acquire)) {
        ScheduleNetPrepAndFetch();
    }
    delete msg;
}

void CloudFetchWorker(void* arg) {
    auto* work = static_cast<CloudFetchWork*>(arg);
    auto* msg = new CloudFetchDoneMsg{};
    msg->op_gen = work->op_gen;
    ESP_LOGI(TAG, "sync fetch begin gen=%u", static_cast<unsigned>(work->op_gen));
    const int64_t t0 = esp_timer_get_time();
    std::string err;
    {
        PowerNeedHold hold_net(PowerNeed::OtaDownload);
        msg->ok = reader::FetchPushResources(msg->items, err, &s_sync_gate);
    }
    const int64_t ms = (esp_timer_get_time() - t0) / 1000;
    if (s_sync_gate.IsCancelled() || err == Lang::Strings::CLOUD_CANCELLED) {
        auto* stopped = new SyncStoppedMsg{};
        stopped->op_gen = work->op_gen;
        delete work;
        delete msg;
        s_cloud_fetch_alive.store(false, std::memory_order_release);
        if (!ScreenLvAsync(AsyncSyncStopped, stopped)) {
            delete stopped;
        }
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(TAG, "sync fetch end gen=%u ok=%d n=%u %dms err=%s",
             static_cast<unsigned>(work->op_gen), msg->ok ? 1 : 0,
             static_cast<unsigned>(msg->items.size()), static_cast<int>(ms), err.c_str());
    if (!msg->ok) {
        std::snprintf(msg->error, sizeof(msg->error), "%s", err.empty() ? Lang::Strings::CLOUD_SYNC_FAIL : err.c_str());
    }
    delete work;
    s_cloud_fetch_alive.store(false, std::memory_order_release);
    if (!ScreenLvAsync(AsyncFetchDone, msg)) {
        delete msg;
    }
    vTaskDelete(nullptr);
}

void ScheduleCloudFetch() {
    auto& st = State();
    if (st.loading || st.download_busy || st.waiting_net) {
        return;
    }
    // 勿 Reset 清 cancel 叠跑僵尸：旧 worker 未退出则只 Abort，等其 Async 结束再重开
    if (s_cloud_fetch_alive.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "ScheduleCloudFetch: stale fetch alive, cancel+abort defer");
        s_sync_gate.RequestCancel();
        AbortCloudGatesOffLvgl();
        SetStatusTip(Lang::Strings::CLOUD_REFRESHING, false);
        return;
    }
    st.loading = true;
    st.error.clear();
    st.fetch_done = false;
    SetStatusTip(Lang::Strings::CLOUD_REFRESHING, false);
    // 已有列表时保留内容，仅标题旁提示；空列表仍走 Render 占位
    if (st.items.empty()) {
        RenderListPage();
    } else {
        ApplyStatusTipUi();
    }

    auto* work = new CloudFetchWork{};
    work->op_gen = st.op_generation;
    s_sync_gate.Reset();
    s_cloud_fetch_alive.store(true, std::memory_order_release);
    if (xTaskCreatePinnedToCore(CloudFetchWorker, "cloud_res_fetch", kCloudFetchStack, work,
                                tskIDLE_PRIORITY + 2, nullptr, 0) != pdPASS) {
        s_cloud_fetch_alive.store(false, std::memory_order_release);
        delete work;
        st.loading = false;
        st.error = Lang::Strings::CLOUD_TASK_FAIL;
        SetStatusTip("", false);
        RenderListPage();
    }
}

struct NetPrepDoneMsg {
    bool ok = false;
    uint32_t op_gen = 0;
};

void AsyncNetPrepDone(void* p) {
    auto* msg = static_cast<NetPrepDoneMsg*>(p);
    auto& st = State();
    const bool alive = ScreenAlive();
    const bool valid = OpStillValid(msg->op_gen);
    const bool cancelled = s_sync_gate.IsCancelled();
    ESP_LOGI(TAG,
             "net_prep async done ok=%d op_gen=%u cur_gen=%u alive=%d valid=%d cancelled=%d "
             "waiting_net=%d",
             msg->ok ? 1 : 0, static_cast<unsigned>(msg->op_gen),
             static_cast<unsigned>(st.op_generation), alive ? 1 : 0, valid ? 1 : 0,
             cancelled ? 1 : 0, st.waiting_net ? 1 : 0);
    if (!valid || cancelled) {
        if (valid) {
            st.waiting_net = false;
        }
        delete msg;
        return;
    }
    st.waiting_net = false;
    if (!msg->ok) {
        st.error = Lang::Strings::CLOUD_NET_NOT_READY;
        SetStatusTip("", false);
        RenderListPage();
        delete msg;
        return;
    }
    st.error.clear();
    delete msg;
    ScheduleCloudFetch();
}

void NetPrepTask(void* /*arg*/) {
    const TaskHandle_t self = xTaskGetCurrentTaskHandle();
    const UBaseType_t stack0 = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG, "net_prep begin op_gen=%u core=%d stack_hwm=%u",
             static_cast<unsigned>(s_net_prep_attach_gen.load()), xPortGetCoreID(),
             static_cast<unsigned>(stack0));
    auto* msg = new NetPrepDoneMsg{};
    bool ok = false;
    {
        // 硬占网：离页后仍可把 Ensure 跑完；结果是否上屏看 attach_gen
        PowerNeedHold hold_net(PowerNeed::OtaDownload);
        for (;;) {
            const uint32_t gen_before = s_net_prep_attach_gen.load();
            ok = Board::GetInstance().EnsureNetworkReady();
            if (ok) {
                break;
            }
            // 超时后若 Schedule 已换挂接（再进页刷新了 attach_gen），再等一轮
            if (s_net_prep_attach_gen.load() == gen_before) {
                break;
            }
            ESP_LOGI(TAG, "net_prep: attach_gen refreshed %u->%u, Ensure again",
                     static_cast<unsigned>(gen_before),
                     static_cast<unsigned>(s_net_prep_attach_gen.load()));
        }
    }
    msg->ok = ok;
    msg->op_gen = s_net_prep_attach_gen.load();
    const bool valid = OpStillValid(msg->op_gen);
    const UBaseType_t stack1 = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG,
             "net_prep after EnsureNetworkReady ok=%d valid=%d cancelled=%d "
             "stack_hwm=%u->%u owned=%d attach_gen=%u",
             msg->ok ? 1 : 0, valid ? 1 : 0, s_sync_gate.IsCancelled() ? 1 : 0,
             static_cast<unsigned>(stack0), static_cast<unsigned>(stack1),
             (s_net_prep_task == self) ? 1 : 0, static_cast<unsigned>(msg->op_gen));
    if (!valid) {
        delete msg;
    } else if (!ScreenLvAsync(AsyncNetPrepDone, msg)) {
        ESP_LOGW(TAG, "net_prep ScreenLvAsync failed op_gen=%u",
                 static_cast<unsigned>(msg->op_gen));
        delete msg;
    }
    if (s_net_prep_task == self) {
        s_net_prep_task = nullptr;
    }
    vTaskDelete(nullptr);
}

void ScheduleNetPrepAndFetch() {
    auto& st = State();
    if (st.fetch_done || st.download_busy || SyncBusy()) {
        ESP_LOGI(TAG, "ScheduleNetPrep skip fetch_done=%d download=%d sync_busy=%d",
                 st.fetch_done ? 1 : 0, st.download_busy ? 1 : 0, SyncBusy() ? 1 : 0);
        return;
    }
    if (s_cloud_fetch_alive.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "ScheduleNetPrep: stale fetch alive, cancel+abort defer");
        s_sync_gate.RequestCancel();
        AbortCloudGatesOffLvgl();
        SetStatusTip(Lang::Strings::CLOUD_CONNECTING_NET, false);
        return;
    }
    s_sync_gate.Reset();
    st.waiting_net = true;
    st.error.clear();
    SetStatusTip(Lang::Strings::CLOUD_CONNECTING_NET, false);
    if (st.items.empty()) {
        RenderListPage();
    } else {
        ApplyStatusTipUi();
    }

    // 已有 cloud_net：挂接当前 op_gen + 刷新 Board 等网 deadline，勿再造任务
    if (s_net_prep_task != nullptr) {
        s_net_prep_attach_gen.store(st.op_generation);
        Board::GetInstance().RefreshNetworkWaitDeadline(30000);
        ESP_LOGI(TAG, "ScheduleNetPrep: reuse task=%p op_gen=%u",
                 static_cast<void*>(s_net_prep_task), static_cast<unsigned>(st.op_generation));
        return;
    }

    s_net_prep_attach_gen.store(st.op_generation);
    if (xTaskCreatePinnedToCore(NetPrepTask, "cloud_net", 4096, nullptr, tskIDLE_PRIORITY + 2,
                                &s_net_prep_task, 0) != pdPASS) {
        s_net_prep_task = nullptr;
        s_net_prep_attach_gen.store(0);
        st.waiting_net = false;
        st.error = Lang::Strings::CLOUD_NET_NOT_READY;
        SetStatusTip("", false);
        RenderListPage();
        ESP_LOGW(TAG, "ScheduleNetPrep: task create failed");
        return;
    }
    ESP_LOGI(TAG, "ScheduleNetPrep: started task=%p op_gen=%u", static_cast<void*>(s_net_prep_task),
             static_cast<unsigned>(st.op_generation));
}


void StyleDownloadBtn(bool enabled) {
    auto& st = State();
    if (st.wallpaper_download_btn == nullptr || st.wallpaper_download_lbl == nullptr) {
        return;
    }
    lv_obj_set_style_bg_color(st.wallpaper_download_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(st.wallpaper_download_btn, enabled ? LV_OPA_COVER : LV_OPA_50, 0);
    lv_obj_set_style_border_color(st.wallpaper_download_btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(st.wallpaper_download_btn, kRowBorderW, 0);
    lv_obj_set_style_radius(st.wallpaper_download_btn, kRowRadius, 0);
    lv_obj_set_style_text_color(st.wallpaper_download_lbl, lv_color_black(), 0);
    if (enabled) {
        lv_obj_add_flag(st.wallpaper_download_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_state(st.wallpaper_download_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_flag(st.wallpaper_download_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(st.wallpaper_download_btn, LV_STATE_DISABLED);
    }
}

void ClearWallpaperPreviewImage() {
    auto& st = State();
    if (st.wallpaper_preview_img != nullptr) {
        lv_image_set_src(st.wallpaper_preview_img, nullptr);
        lv_obj_add_flag(st.wallpaper_preview_img, LV_OBJ_FLAG_HIDDEN);
    }
    if (st.wallpaper_preview_raster != nullptr) {
        delete st.wallpaper_preview_raster;
        st.wallpaper_preview_raster = nullptr;
    }
}

void ShowWallpaperPreviewMode(bool preview) {
    auto& st = State();
    st.wallpaper_preview_open = preview;
    if (st.main_body != nullptr) {
        if (preview) {
            lv_obj_add_flag(st.main_body, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(st.main_body, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (st.page_lbl != nullptr) {
        if (preview) {
            lv_obj_add_flag(st.page_lbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(st.page_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (st.wallpaper_preview_body != nullptr) {
        if (preview) {
            lv_obj_clear_flag(st.wallpaper_preview_body, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(st.wallpaper_preview_body, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void CloseWallpaperPreview() {
    auto& st = State();
    ++st.wallpaper_preview_epoch;
    st.wallpaper_preview_idx = -1;
    ClearWallpaperPreviewImage();
    ShowWallpaperPreviewMode(false);
    RenderListPage();
}

struct WallpaperPreviewWork {
    int index = -1;
    uint32_t epoch = 0;
    char local_path[256] = {};
    char download_url[768] = {};
    bool use_local = false;
    std::vector<uint8_t> cached_bytes;  // 非空则本地按详情尺寸解码，不再 HTTP
};

struct WallpaperPreviewResultMsg {
    int index = -1;
    uint32_t epoch = 0;
    bool ok = false;
    char err[80] = {};
    reader::RasterImage* img = nullptr;
};

void ApplyWallpaperPreviewAsync(void* p) {
    auto* msg = static_cast<WallpaperPreviewResultMsg*>(p);
    s_wallpaper_preview_busy.store(false);
    s_wallpaper_preview_task = nullptr;
    if (msg == nullptr) {
        return;
    }
    auto& st = State();
    if (!ScreenAlive() || msg->epoch != st.wallpaper_preview_epoch ||
        !st.wallpaper_preview_open || msg->index != st.wallpaper_preview_idx) {
        delete msg->img;
        delete msg;
        if (ScreenAlive() && st.wallpaper_preview_open && st.wallpaper_preview_idx >= 0) {
            if (CloudDlTasksActive() || s_dl_owns_http.load(std::memory_order_acquire)) {
                ShowPreviewWaitDlTip();
            } else {
                ScheduleLoadWallpaperPreview(st.wallpaper_preview_idx);
            }
        }
        return;
    }

    ClearWallpaperPreviewImage();
    if (!msg->ok || msg->img == nullptr || msg->img->empty()) {
        if (st.wallpaper_preview_status != nullptr) {
            lv_label_set_text(st.wallpaper_preview_status,
                              msg->err[0] != '\0' ? msg->err : Lang::Strings::CLOUD_PREVIEW_FAIL);
            lv_obj_clear_flag(st.wallpaper_preview_status, LV_OBJ_FLAG_HIDDEN);
        }
        StyleDownloadBtn(true);
        delete msg->img;
        delete msg;
        return;
    }

    st.wallpaper_preview_raster = msg->img;
    msg->img = nullptr;
    st.wallpaper_preview_raster->BindDsc();

    if (st.wallpaper_preview_img != nullptr) {
        lv_image_set_scale(st.wallpaper_preview_img, LV_SCALE_NONE);
        lv_image_set_src(st.wallpaper_preview_img, &st.wallpaper_preview_raster->dsc);
        lv_obj_set_size(st.wallpaper_preview_img, st.wallpaper_preview_raster->width,
                        st.wallpaper_preview_raster->height);
        lv_obj_clear_flag(st.wallpaper_preview_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(st.wallpaper_preview_img);
        lv_obj_invalidate(st.wallpaper_preview_img);
        if (st.wallpaper_preview_img_host != nullptr) {
            lv_obj_invalidate(st.wallpaper_preview_img_host);
        }
    }
    if (st.wallpaper_preview_status != nullptr) {
        lv_obj_add_flag(st.wallpaper_preview_status, LV_OBJ_FLAG_HIDDEN);
    }
    StyleDownloadBtn(true);
    delete msg;
}

void WallpaperPreviewLoadTask(void* arg) {
    auto* work = static_cast<WallpaperPreviewWork*>(arg);
    auto* msg = new WallpaperPreviewResultMsg{};
    {
        if (work == nullptr) {
            std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::CLOUD_FILE_INVALID);
        } else {
            msg->index = work->index;
            msg->epoch = work->epoch;
            auto* img = new reader::RasterImage();
            bool ok = false;
            if (State().wallpaper_preview_epoch != work->epoch) {
                delete img;
                img = nullptr;
                std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::CLOUD_CANCELLED);
            } else if (s_dl_owns_http.load(std::memory_order_acquire)) {
                delete img;
                img = nullptr;
                std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::CLOUD_WAIT_DOWNLOAD);
            } else if (!work->cached_bytes.empty()) {
                // 列表已缓存 coverImageUrl 字节：按详情尺寸解码，不再 HTTP
                ok = reader::DecodeImageToL8(work->cached_bytes.data(), work->cached_bytes.size(),
                                             kPreviewMaxW, kPreviewMaxH, *img) &&
                     !img->empty();
                if (!ok) {
                    std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::CLOUD_DECODE_FAIL);
                }
            } else {
                PowerNeedHold hold_net(PowerNeed::OtaDownload);
                if (work->use_local && work->local_path[0] != '\0') {
                    ok = reader::DecodeImageFileToL8(work->local_path, kPreviewMaxW, kPreviewMaxH,
                                                     *img) &&
                         !img->empty();
                } else if (work->download_url[0] != '\0') {
                    std::vector<uint8_t> bytes;
                    if (DownloadUrlToBuffer(work->download_url, bytes, kPreviewMaxDownloadBytes) &&
                        State().wallpaper_preview_epoch == work->epoch) {
                        ok = reader::DecodeImageToL8(bytes.data(), bytes.size(), kPreviewMaxW,
                                                     kPreviewMaxH, *img) &&
                             !img->empty();
                    } else {
                        std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::CLOUD_LOAD_FAIL);
                    }
                }
            }
            if (ok) {
                msg->ok = true;
                msg->img = img;
            } else {
                delete img;
                if (msg->err[0] == '\0') {
                    std::snprintf(msg->err, sizeof(msg->err), "%s", Lang::Strings::CLOUD_DECODE_FAIL);
                }
            }
            delete work;
        }
    }

    if (!ScreenLvAsync(ApplyWallpaperPreviewAsync, msg)) {
        delete msg->img;
        delete msg;
        s_wallpaper_preview_busy.store(false);
        s_wallpaper_preview_task = nullptr;
    }
    vTaskDelete(nullptr);
}

void ScheduleLoadWallpaperPreview(int index) {
    auto& st = State();
    if (index < 0 || index >= static_cast<int>(st.items.size())) {
        return;
    }
    // 下载独占 HTTP：详情可开，但不加载封面（含缓存解码），统一提示；全结束后续 Resume
    if (CloudDlTasksActive() || s_dl_owns_http.load(std::memory_order_acquire)) {
        ShowPreviewWaitDlTip();
        return;
    }
    if (s_wallpaper_preview_busy.exchange(true)) {
        return;
    }
    const reader::CloudPushResource& item = st.items[static_cast<size_t>(index)];
    auto* work = new WallpaperPreviewWork{};
    work->index = index;
    work->epoch = st.wallpaper_preview_epoch;
    // 预览只用 coverImageUrl（列表缓存或 HTTP）；已落盘壁纸可走本地文件；绝不拉完整 downloadUrl
    const bool is_badge = item.type == reader::PushResourceType::kBadge;
    work->use_local = is_badge && item.IsDownloaded() &&
                      (index >= static_cast<int>(st.cover_bytes.size()) ||
                       st.cover_bytes[static_cast<size_t>(index)].empty()) &&
                      item.CoverThumbUrl().empty();
    if (work->use_local) {
        std::snprintf(work->local_path, sizeof(work->local_path), "%s", item.LocalPath().c_str());
    }
    if (index < static_cast<int>(st.cover_bytes.size()) &&
        !st.cover_bytes[static_cast<size_t>(index)].empty()) {
        work->cached_bytes = st.cover_bytes[static_cast<size_t>(index)];
        ESP_LOGI(TAG, "preview cover from list cache idx=%d bytes=%u", index,
                 static_cast<unsigned>(work->cached_bytes.size()));
    } else if (!work->use_local) {
        const std::string& url = item.CoverThumbUrl();
        if (url.empty()) {
            delete work;
            s_wallpaper_preview_busy.store(false);
            s_wallpaper_preview_task = nullptr;
            if (st.wallpaper_preview_status != nullptr) {
                lv_label_set_text(st.wallpaper_preview_status, Lang::Strings::CLOUD_NO_COVER);
                lv_obj_clear_flag(st.wallpaper_preview_status, LV_OBJ_FLAG_HIDDEN);
            }
            StyleDownloadBtn(true);
            return;
        }
        if (url.size() >= sizeof(work->download_url)) {
            delete work;
            s_wallpaper_preview_busy.store(false);
            s_wallpaper_preview_task = nullptr;
            if (st.wallpaper_preview_status != nullptr) {
                lv_label_set_text(st.wallpaper_preview_status, Lang::Strings::CLOUD_PREVIEW_URL_LONG);
                lv_obj_clear_flag(st.wallpaper_preview_status, LV_OBJ_FLAG_HIDDEN);
            }
            StyleDownloadBtn(true);
            return;
        }
        std::snprintf(work->download_url, sizeof(work->download_url), "%s", url.c_str());
        ESP_LOGI(TAG, "preview cover HTTP fallback idx=%d (no list cache)", index);
    }
    // 预览加载任务
    if (xTaskCreatePinnedToCore(WallpaperPreviewLoadTask, "cloud_wp_prev",
                                kWallpaperPreviewStack, work, 5, &s_wallpaper_preview_task,
                                0) != pdPASS) {
        delete work;
        s_wallpaper_preview_busy.store(false);
        s_wallpaper_preview_task = nullptr;
        if (st.wallpaper_preview_status != nullptr) {
            lv_label_set_text(st.wallpaper_preview_status, Lang::Strings::CLOUD_PREVIEW_START_FAIL);
            lv_obj_clear_flag(st.wallpaper_preview_status, LV_OBJ_FLAG_HIDDEN);
        }
        StyleDownloadBtn(true);
    }
}

void OpenWallpaperPreview(int index) {
    auto& st = State();
    if (index < 0 || index >= static_cast<int>(st.items.size())) {
        return;
    }
    if (st.dialog_mask != nullptr || st.action_mask != nullptr) {
        return;
    }
    const reader::CloudPushResource& item = st.items[static_cast<size_t>(index)];
    ++st.wallpaper_preview_epoch;
    st.wallpaper_preview_idx = index;
    ClearWallpaperPreviewImage();
    ShowWallpaperPreviewMode(true);
    if (st.wallpaper_preview_title != nullptr) {
        // 优先可读名；展示去扩展名（同壁纸/书库标题）
        const char* raw =
            !item.resource_name.empty() ? item.resource_name.c_str() : item.name.c_str();
        const std::string stem = reader::TitleFromPath(raw);
        const std::string shown =
            LayoutTitleTwoLines(stem.empty() ? raw : stem.c_str(), UiFont(), ContentWidth());
        lv_label_set_text(st.wallpaper_preview_title, shown.c_str());
    }
    FillPreviewMeta(&item);
    if (st.wallpaper_download_lbl != nullptr) {
        lv_label_set_text(st.wallpaper_download_lbl, Lang::Strings::CLOUD_SAVE_LOCAL);
    }
    StyleDownloadBtn(true);
    if (CloudDlTasksActive() || s_dl_owns_http.load(std::memory_order_acquire)) {
        ShowPreviewWaitDlTip();
        return;
    }
    if (st.wallpaper_preview_status != nullptr) {
        lv_label_set_text(st.wallpaper_preview_status, Lang::Strings::CLOUD_LOADING);
        lv_obj_clear_flag(st.wallpaper_preview_status, LV_OBJ_FLAG_HIDDEN);
    }
    ScheduleLoadWallpaperPreview(index);
}

void OnWallpaperDownloadClicked(lv_event_t* /*e*/) {
    auto& st = State();
    if (!st.wallpaper_preview_open || st.wallpaper_preview_idx < 0) {
        return;
    }
    StartWallpaperDownload(st.wallpaper_preview_idx);
}

void StartWallpaperDownload(int index) {
    auto& st = State();
    if (SyncBusy()) {
        SetStatusTip(Lang::Strings::CLOUD_PLEASE_WAIT, true);
        return;
    }
    if (index < 0 || index >= static_cast<int>(st.items.size())) {
        return;
    }
    const std::string& id = st.items[static_cast<size_t>(index)].task_id;
    auto close_preview = [&]() {
        if (st.wallpaper_preview_open) {
            ++st.wallpaper_preview_epoch;
            st.wallpaper_preview_idx = -1;
            ClearWallpaperPreviewImage();
            ShowWallpaperPreviewMode(false);
        }
    };

    // 正在下这一条：仅提示
    if (st.download_busy && st.download_key == id) {
        SetStatusTip(Lang::Strings::CLOUD_DOWNLOADING, true);
        close_preview();
        return;
    }
    // 已有下载/批量：加入串行队列，当前完成后 ContinueBatchSaveIfNeeded 会接着下
    if (st.download_busy || st.batch_busy || st.delete_busy) {
        if (st.delete_busy) {
            SetStatusTip(Lang::Strings::CLOUD_PLEASE_WAIT, true);
            close_preview();
            return;
        }
        if (std::find(st.batch_save_ids.begin(), st.batch_save_ids.end(), id) !=
            st.batch_save_ids.end()) {
            SetStatusTip(Lang::Strings::CLOUD_IN_QUEUE, true);
            close_preview();
            return;
        }
        st.batch_save_ids.push_back(id);
        st.batch_busy = true;
        ++st.dl_job_total;
        close_preview();
        if (st.dl_page_open) {
            RefreshDlProgressPage(st.dl_shown_percent < 0 ? 0 : st.dl_shown_percent);
        }
        return;
    }

    close_preview();
    StartResourceDownload(index);
}

void SetPageFooterText(const char* text) {
    auto& st = State();
    if (st.page_lbl != nullptr) {
        lv_label_set_text(st.page_lbl, text != nullptr ? text : "");
    }
}

/** 手动刷新：清空 fetch_done 后重拉统一推送列表（按钮文案固定「刷新」） */
void ScheduleUserCloudSync() {
    auto& st = State();
    if (st.wallpaper_preview_open) {
        return;
    }
    if (st.download_busy || SyncBusy() || s_net_prep_task != nullptr) {
        return;
    }
    if (Board::GetInstance().IsWifiConfigMode()) {
        st.error = Lang::Strings::CLOUD_NEED_WIFI_CFG;
        RenderListPage();
        return;
    }
    st.fetch_done = false;
    ScheduleNetPrepAndFetch();
}

void OnTabClicked(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    auto& st = State();
    // Tab 仅本地筛选；下载进度页打开时不可切
    if (st.dl_page_open || st.wallpaper_preview_open || st.dialog_mask != nullptr ||
        st.action_mask != nullptr) {
        return;
    }
    const int tab = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (tab < 0 || tab >= kTabCount || tab == st.filter_tab) {
        return;
    }
    // 多选跨 Tab 保留选中；仅换筛选与列表
    st.filter_tab = tab;
    st.page = 0;
    RefreshTabUi();
    RenderListPage();
}

lv_obj_t* MakeTabBtn(lv_obj_t* parent, const char* text, int tab) {
    auto& st = State();
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, lv_pct(100));
    lv_obj_set_style_pad_all(btn, 0, 0);
    DisableScroll(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(btn);
    lv_obj_add_event_cb(btn, OnTabClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(tab)));
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, ItemFont(), 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    st.tab_btns[tab] = btn;
    st.tab_lbls[tab] = lbl;
    return btn;
}

void OnSyncClicked(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    auto& st = State();
    if (st.wallpaper_preview_open || st.multi) {
        return;
    }
    if (st.download_busy || st.batch_busy || st.delete_busy || SyncBusy() ||
        s_net_prep_task != nullptr) {
        SetStatusTip(Lang::Strings::CLOUD_PLEASE_WAIT, true);
        return;
    }
    const int64_t now = esp_timer_get_time();
    if (s_last_user_sync_us != 0 && (now - s_last_user_sync_us) < kSyncThrottleUs) {
        SetStatusTip(Lang::Strings::CLOUD_PLEASE_WAIT, true);
        return;
    }
    s_last_user_sync_us = now;
    ScheduleUserCloudSync();
}

void OnMultiCancel(lv_event_t* /*e*/) {
    auto& st = State();
    if (st.batch_busy || st.download_busy || st.delete_busy) {
        return;
    }
    ExitMultiMode(true);
}

void OnMultiSelectAll(lv_event_t* /*e*/) {
    auto& st = State();
    if (!st.multi || st.batch_busy || st.download_busy || st.delete_busy) {
        return;
    }
    RebuildFiltered();
    SyncSelectedSize();
    if (st.filtered.empty()) {
        return;
    }
    int selected_n = 0;
    for (int idx : st.filtered) {
        if (ItemSelected(idx)) {
            ++selected_n;
        }
    }
    const bool clear = (selected_n >= static_cast<int>(st.filtered.size()));
    for (int idx : st.filtered) {
        if (idx >= 0 && idx < static_cast<int>(st.selected.size())) {
            st.selected[static_cast<size_t>(idx)] = clear ? 0 : 1;
        }
    }
    RequestCheckMarksPaint();
}

void OnMultiDelete(lv_event_t* /*e*/) {
    auto& st = State();
    if (!st.multi || st.batch_busy || st.download_busy || st.delete_busy || SyncBusy()) {
        return;
    }
    SyncSelectedSize();
    std::vector<reader::CloudPushResource> items;
    for (size_t i = 0; i < st.items.size() && i < st.selected.size(); ++i) {
        if (st.selected[i] != 0) {
            items.push_back(st.items[i]);
        }
    }
    ScheduleDeleteItems(std::move(items));
}

void OnMultiSave(lv_event_t* /*e*/) {
    auto& st = State();
    if (!st.multi || st.batch_busy || st.download_busy || st.delete_busy || SyncBusy()) {
        return;
    }
    SyncSelectedSize();
    st.batch_save_ids.clear();
    for (size_t i = 0; i < st.items.size() && i < st.selected.size(); ++i) {
        if (st.selected[i] != 0) {
            st.batch_save_ids.push_back(st.items[i].task_id);
        }
    }
    if (st.batch_save_ids.empty()) {
        SetStatusTip(Lang::Strings::CLOUD_SELECT_FIRST, true);
        return;
    }
    st.batch_busy = true;
    st.dl_job_total = static_cast<int>(st.batch_save_ids.size());
    st.dl_job_done = 0;
    st.dl_user_abort = false;
    SetStatusTip("", false);
    st.multi = false;
    RefreshFooterMode();
    ShowDlProgressPage();
    ContinueBatchSaveIfNeeded();
}

lv_obj_t* FindListRow(int index) {
    auto& st = State();
    if (st.list_body == nullptr || !lv_obj_is_valid(st.list_body)) {
        return nullptr;
    }
    const uint32_t n = lv_obj_get_child_count(st.list_body);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* row = lv_obj_get_child(st.list_body, i);
        if (row != nullptr &&
            static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(row))) == index) {
            return row;
        }
    }
    return nullptr;
}

lv_obj_t* FindRowChildBySize(lv_obj_t* row, lv_coord_t w, lv_coord_t h) {
    if (row == nullptr) {
        return nullptr;
    }
    const uint32_t n = lv_obj_get_child_count(row);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* c = lv_obj_get_child(row, i);
        if (c != nullptr && lv_obj_get_width(c) == w && lv_obj_get_height(c) == h) {
            return c;
        }
    }
    return nullptr;
}

void PatchRowCheckMark(int index) {
    auto& st = State();
    if (!st.multi) {
        return;
    }
    lv_obj_t* row = FindListRow(index);
    lv_obj_t* check = FindRowChildBySize(row, kCheckSize, kCheckSize);
    if (check == nullptr) {
        RequestCloudRender(); // 勾选列尚未建好（进多选 coalesce 未落地）
        return;
    }
    lv_obj_clean(check);
    if (!ItemSelected(index)) {
        return;
    }
    lv_obj_t* mark = lv_label_create(check);
    lv_label_set_text(mark, "√");
    lv_obj_set_style_text_font(mark, ItemFont(), 0);
    lv_obj_set_style_text_color(mark, lv_color_black(), 0);
    lv_obj_center(mark);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
}

void PatchVisibleRowCheckMarks() {
    auto& st = State();
    if (!st.multi || st.list_body == nullptr || !lv_obj_is_valid(st.list_body)) {
        return;
    }
    const uint32_t n = lv_obj_get_child_count(st.list_body);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* row = lv_obj_get_child(st.list_body, i);
        if (row == nullptr) {
            continue;
        }
        PatchRowCheckMark(static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(row))));
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), Lang::Strings::CLOUD_SELECTED_FMT, SelectedCount());
    SetStatusTip(buf, false);
}

void PatchRowThumb(int index) {
    auto& st = State();
    if (index < 0 || index >= static_cast<int>(st.thumbs.size()) ||
        st.thumbs[static_cast<size_t>(index)] == nullptr ||
        st.thumbs[static_cast<size_t>(index)]->empty()) {
        return;
    }
    lv_obj_t* row = FindListRow(index);
    lv_obj_t* thumb_box = FindRowChildBySize(row, kThumbW, kThumbH);
    if (thumb_box == nullptr) {
        return;  // 非本页或无封面行：数据已写入 st.thumbs，翻页再建
    }
    lv_obj_clean(thumb_box);
    auto& img = *st.thumbs[static_cast<size_t>(index)];
    lv_obj_t* thumb = lv_image_create(thumb_box);
    lv_image_set_src(thumb, &img.dsc);
    lv_obj_set_size(thumb, img.width, img.height);
    lv_obj_center(thumb);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t* CreateResourceRow(lv_obj_t* parent, const reader::CloudPushResource& item, int index) {
    auto& st = State();
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    // 以外框内容宽为准：需扣 list_frame 的 pad + border，否则右边框竖线被裁切
    lv_coord_t row_w = parent != nullptr ? lv_obj_get_content_width(parent) : 0;
    if (row_w <= 0) {
        row_w = ContentWidth() - kListFramePad * 2 - kListFrameBorderW * 2;
    }
    lv_obj_set_size(row, row_w, kCloudRowH);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, kRowBorderW, 0);
    lv_obj_set_style_border_color(row, lv_color_black(), 0);
    lv_obj_set_style_radius(row, kRowRadius, 0);
    lv_obj_set_style_pad_hor(row, kRowPad, 0);
    lv_obj_set_style_pad_ver(row, 6, 0);
    lv_obj_set_style_layout(row, LV_LAYOUT_NONE, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(row);
    lv_obj_set_user_data(row, reinterpret_cast<void*>(static_cast<intptr_t>(index)));
    HapticAttachClick(row);
    lv_obj_add_event_cb(row, OnRowClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));
    lv_obj_add_event_cb(row, OnRowLongPressed, LV_EVENT_LONG_PRESSED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));

    const bool show_thumb = item.type == reader::PushResourceType::kBook ||
                            item.type == reader::PushResourceType::kBadge ||
                            item.type == reader::PushResourceType::kFont;
    lv_coord_t text_x = 0;
    if (show_thumb) {
        lv_obj_t* thumb_box = lv_obj_create(row);
        lv_obj_remove_style_all(thumb_box);
        lv_obj_set_size(thumb_box, kThumbW, kThumbH);
        lv_obj_set_style_border_width(thumb_box, 1, 0);
        lv_obj_set_style_border_color(thumb_box, lv_color_black(), 0);
        lv_obj_set_style_bg_color(thumb_box, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(thumb_box, LV_OPA_COVER, 0);
        lv_obj_align(thumb_box, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_clear_flag(thumb_box, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(thumb_box);

        if (index >= 0 && index < static_cast<int>(st.thumbs.size()) &&
            st.thumbs[static_cast<size_t>(index)] != nullptr &&
            !st.thumbs[static_cast<size_t>(index)]->empty()) {
            lv_obj_t* thumb = lv_image_create(thumb_box);
            lv_image_set_src(thumb, &st.thumbs[static_cast<size_t>(index)]->dsc);
            lv_obj_set_size(thumb, st.thumbs[static_cast<size_t>(index)]->width,
                            st.thumbs[static_cast<size_t>(index)]->height);
            lv_obj_center(thumb);
            lv_obj_clear_flag(thumb, LV_OBJ_FLAG_CLICKABLE);
        }
        text_x = kThumbW + kThumbGap;
    }

    const lv_font_t* item_font = ItemFont();
    const lv_coord_t line_h =
        (item_font != nullptr && item_font->line_height > 0) ? item_font->line_height : 30;
    const bool show_check = st.multi;
    const lv_coord_t trail_reserve = show_check ? (kCheckSize + kCheckGap) : 0;
    const lv_coord_t text_w = row_w - kRowPad * 2 - text_x - trail_reserve;
    const std::string line1 = EllipsizeText(item.name, item_font, text_w);

    lv_obj_t* title = lv_label_create(row);
    lv_obj_set_style_text_font(title, item_font, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_size(title, text_w, line_h);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_label_set_text(title, line1.c_str());
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, text_x, show_thumb ? 4 : 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    char line2[80];
    std::snprintf(line2, sizeof(line2), "%s · %s", item.TypeLabel(),
                  reader::FormatCloudFileSize(item.file_size).c_str());
    lv_obj_t* meta = lv_label_create(row);
    lv_obj_set_style_text_font(meta, item_font, 0);
    lv_obj_set_style_text_color(meta, lv_color_black(), 0);
    lv_obj_set_size(meta, text_w, line_h);
    lv_label_set_long_mode(meta, LV_LABEL_LONG_CLIP);
    lv_label_set_text(meta, line2);
    lv_obj_align(meta, LV_ALIGN_TOP_LEFT, text_x,
                 (show_thumb ? 4 : 0) + line_h + kCloudRowLineGap);
    lv_obj_clear_flag(meta, LV_OBJ_FLAG_CLICKABLE);

    if (show_check) {
        // 行尾勾选：白底黑框；选中只画 √，勿实心填充（同每日清单）
        lv_obj_t* check = lv_obj_create(row);
        lv_obj_remove_style_all(check);
        lv_obj_set_size(check, kCheckSize, kCheckSize);
        lv_obj_set_style_bg_color(check, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(check, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(check, lv_color_black(), 0);
        lv_obj_set_style_border_width(check, kRowBorderW, 0);
        lv_obj_set_style_radius(check, 4, 0);
        lv_obj_align(check, LV_ALIGN_RIGHT_MID, 0, 0);
        DisableScroll(check);
        lv_obj_clear_flag(check, LV_OBJ_FLAG_CLICKABLE);
        if (ItemSelected(index)) {
            lv_obj_t* mark = lv_label_create(check);
            lv_label_set_text(mark, "√");
            lv_obj_set_style_text_font(mark, item_font, 0);
            lv_obj_set_style_text_color(mark, lv_color_black(), 0);
            lv_obj_center(mark);
            lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    return row;
}

struct CoverThumbWorkItem {
    int index = -1;
    char url[768] = {};
};

struct CoverThumbOneMsg {
    uint32_t epoch = 0;
    int index = -1;
    reader::RasterImage* img = nullptr;
    std::vector<uint8_t> bytes;  // 原始封面字节，与小图一并入库
    bool done = false;  // 本批最后一项（或失败收尾）后清 busy
};

void ApplyCoverThumbOneAsync(void* p) {
    auto* msg = static_cast<CoverThumbOneMsg*>(p);
    if (msg == nullptr) {
        return;
    }
    auto& st = State();
    if (!ScreenAlive() || msg->epoch != st.thumb_epoch) {
        delete msg->img;
        if (msg->done) {
            s_thumb_busy.store(false);
            s_thumb_task = nullptr;
        }
        delete msg;
        return;
    }
    if (msg->index >= 0 && msg->index < static_cast<int>(st.thumbs.size()) && msg->img != nullptr) {
        if (msg->index >= static_cast<int>(st.cover_bytes.size())) {
            st.cover_bytes.resize(st.items.size());
        }
        if (msg->index < static_cast<int>(st.cover_bytes.size()) && !msg->bytes.empty()) {
            st.cover_bytes[static_cast<size_t>(msg->index)] = std::move(msg->bytes);
        }
        if (!msg->img->empty()) {
            msg->img->BindDsc();
        }
        st.thumbs[static_cast<size_t>(msg->index)].reset(msg->img);
        msg->img = nullptr;
        // 封面全量加载中列表尚未画出；就绪后整页 Render，勿 Patch 空行
        if (!st.covers_loading) {
            PatchRowThumb(msg->index);
        }
    } else {
        delete msg->img;
    }
    if (msg->done) {
        s_thumb_busy.store(false);
        s_thumb_task = nullptr;
        if (st.covers_loading) {
            st.covers_loading = false;
            SetStatusTip(Lang::Strings::CLOUD_REFRESHED, true);
            RenderListPage();
        }
    }
    delete msg;
}

struct CoverThumbFillWork {
    uint32_t epoch = 0;
    std::vector<CoverThumbWorkItem> items;
};

void CoverThumbFillTask(void* arg) {
    auto* work = static_cast<CoverThumbFillWork*>(arg);
    if (work == nullptr) {
        s_thumb_busy.store(false);
        s_thumb_task = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    const uint32_t epoch = work->epoch;
    const size_t n = work->items.size();
    // 诊断：sp 落点区分片内(0x3FC8…) / PSRAM(0x3C…)；配合 malloc 失败日志钉下次崩因
    auto log_cover_diag = [](const char* phase, int idx) {
        void* sp = nullptr;
#if defined(__GNUC__)
        sp = __builtin_frame_address(0);
#endif
        const size_t free_int = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t free_ps = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const size_t large_int = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        const size_t large_ps = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG,
                 "cover diag %s idx=%d sp=%p hwm=%u int=%u/%u psram=%u/%u",
                 phase != nullptr ? phase : "?", idx, sp,
                 static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                 static_cast<unsigned>(free_int), static_cast<unsigned>(large_int),
                 static_cast<unsigned>(free_ps), static_cast<unsigned>(large_ps));
    };
    log_cover_diag("task_start", -1);
    {
        // 封面下载短时硬占网；须在 vTaskDelete 前析构（任务自杀不跑栈析构）
        PowerNeedHold hold_net(PowerNeed::OtaDownload);
        for (size_t i = 0; i < n; ++i) {
            if (State().thumb_epoch != epoch || s_thumb_abort.load(std::memory_order_acquire)) {
                break;
            }
            const CoverThumbWorkItem& w = work->items[i];
            log_cover_diag("before_new_img", w.index);
            // 禁止抛 bad_alloc：上次崩在 operator new→malloc NULL→throw（EXCVADDR=0）
            auto* img = new (std::nothrow) reader::RasterImage();
            if (img == nullptr) {
                const bool intact = heap_caps_check_integrity_all(true);
                ESP_LOGE(TAG,
                         "cover RasterImage new fail idx=%d heap_ok=%d int=%u psram=%u",
                         w.index, intact ? 1 : 0,
                         static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                         static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
                break;
            }
            bool ok = false;
            std::vector<uint8_t> bytes;
            if (w.url[0] != '\0') {
                ESP_LOGI(TAG, "cover dl begin idx=%d", w.index);
                if (DownloadUrlToBuffer(w.url, bytes, kThumbMaxDownloadBytes)) {
                    ok = reader::DecodeImageToL8(bytes.data(), bytes.size(), kThumbDecodeW,
                                                 kThumbDecodeH, *img) &&
                         !img->empty();
                    ESP_LOGI(TAG, "cover decode idx=%d ok=%d bytes=%u", w.index, ok ? 1 : 0,
                             static_cast<unsigned>(bytes.size()));
                } else {
                    ESP_LOGW(TAG, "cover dl fail idx=%d", w.index);
                }
            }
            if (State().thumb_epoch != epoch) {
                delete img;
                break;
            }
            if (!ok) {
                // 占位空图：避免同页反复重试刷网；翻页/ack 删项会 bump epoch 重建
                img->Reset();
                // 下载成功但缩略解码失败时仍保留 bytes，供详情按大尺寸再解
            }
            log_cover_diag("before_new_msg", w.index);
            auto* msg = new (std::nothrow) CoverThumbOneMsg{};
            if (msg == nullptr) {
                const bool intact = heap_caps_check_integrity_all(true);
                ESP_LOGE(TAG, "cover CoverThumbOneMsg new fail idx=%d heap_ok=%d", w.index,
                         intact ? 1 : 0);
                delete img;
                break;
            }
            msg->epoch = epoch;
            msg->index = w.index;
            msg->img = img;
            msg->bytes = std::move(bytes);
            msg->done = (i + 1 == n);
            // cloud_cover 非 LVGL：禁止裸 lv_async_call
            if (!ScreenLvAsync(ApplyCoverThumbOneAsync, msg)) {
                delete msg->img;
                delete msg;
                if (i + 1 == n) {
                    s_thumb_busy.store(false);
                    s_thumb_task = nullptr;
                }
            }
        }
        // 中途 epoch 失效：须清 busy（否则再进页 Schedule 被挡）
        if (State().thumb_epoch != epoch) {
            s_thumb_busy.store(false);
            s_thumb_task = nullptr;
        } else if (n == 0) {
            s_thumb_busy.store(false);
            s_thumb_task = nullptr;
        }
        delete work;
    }
    log_cover_diag("task_end", -1);
    vTaskDelete(nullptr);
}

void ScheduleCoverThumbFill() {
    auto& st = State();
    if (!ScreenAlive() || st.items.empty()) {
        return;
    }
    // 下载/批量/删除进行中勿叠封面 HTTP
    if (st.download_busy || st.batch_busy || st.delete_busy ||
        s_dl_owns_http.load(std::memory_order_acquire)) {
        return;
    }
    if (s_thumb_busy.load()) {
        return;
    }
    if (st.cover_bytes.size() != st.items.size()) {
        st.cover_bytes.resize(st.items.size());
    }
    if (st.thumbs.size() != st.items.size()) {
        st.thumbs.resize(st.items.size());
    }
    // 全量预览图进 PSRAM；翻页只读 thumbs，不再按页 HTTP
    auto* work = new (std::nothrow) CoverThumbFillWork{};
    if (work == nullptr) {
        ESP_LOGE(TAG, "cover work alloc fail int=%u psram=%u",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        if (st.covers_loading) {
            st.covers_loading = false;
            SetStatusTip(Lang::Strings::CLOUD_COVER_FAIL, true);
            RenderListPage();
        }
        return;
    }
    work->epoch = st.thumb_epoch;
    work->items.reserve(st.items.size());
    for (size_t i = 0; i < st.items.size(); ++i) {
        if (st.thumbs[i] != nullptr) {
            continue;
        }
        const reader::CloudPushResource& item = st.items[i];
        if (!item.HasCoverThumb()) {
            continue;
        }
        CoverThumbWorkItem w;
        w.index = static_cast<int>(i);
        const std::string& cover_url = item.CoverThumbUrl();
        if (cover_url.size() >= sizeof(w.url)) {
            ESP_LOGW(TAG, "cover url too long idx=%u len=%u", static_cast<unsigned>(i),
                     static_cast<unsigned>(cover_url.size()));
            continue;
        }
        std::snprintf(w.url, sizeof(w.url), "%s", cover_url.c_str());
        work->items.push_back(w);
    }
    if (work->items.empty()) {
        delete work;
        if (st.covers_loading) {
            st.covers_loading = false;
            SetStatusTip(Lang::Strings::CLOUD_REFRESHED, true);
            RenderListPage();
        }
        return;
    }
    s_thumb_abort.store(false, std::memory_order_release);
    if (s_thumb_busy.exchange(true)) {
        delete work;
        return;
    }
    if (xTaskCreatePinnedToCore(CoverThumbFillTask, "cloud_cover", kCoverThumbStack, work, 4,
                                &s_thumb_task, 0) != pdPASS) {
        s_thumb_busy.store(false);
        s_thumb_task = nullptr;
        delete work;
        ESP_LOGW(TAG, "cover thumb task create failed");
        if (st.covers_loading) {
            st.covers_loading = false;
            SetStatusTip(Lang::Strings::CLOUD_COVER_FAIL, true);
            RenderListPage();
        }
    }
}

void RenderListPageInternal(bool schedule_thumbs) {
    auto& st = State();
    if (st.list_body == nullptr) {
        return;
    }
    lv_obj_set_style_layout(st.list_body, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(st.list_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(st.list_body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(st.list_body, kRowGap, 0);
    lv_obj_clean(st.list_body);
    DisableScroll(st.list_body);

    // 拉取中若已有列表：保留行，提示在标题旁；仅空列表时占位
    if (st.waiting_net && st.items.empty()) {
        ShowMessage(st.list_body, Lang::Strings::CLOUD_CONNECT_NET);
        SetPageFooterText("");
        return;
    }
    if (st.loading && st.items.empty()) {
        ShowMessage(st.list_body, Lang::Strings::CLOUD_FETCHING_LIST);
        SetPageFooterText("");
        return;
    }
    if (st.covers_loading && !st.items.empty()) {
        ShowMessage(st.list_body, Lang::Strings::CLOUD_LOAD_COVER);
        SetPageFooterText("");
        return;
    }
    if (!st.error.empty() && st.items.empty()) {
        ShowMessage(st.list_body, st.error.c_str());
        SetPageFooterText("1 / 1");
        return;
    }

    RebuildFiltered();
    if (st.filtered.empty()) {
        const char* empty_msg = Lang::Strings::CLOUD_EMPTY_ALL;
        if (st.filter_tab == 1) {
            empty_msg = Lang::Strings::CLOUD_EMPTY_WALLPAPER;
        } else if (st.filter_tab == 2) {
            empty_msg = Lang::Strings::CLOUD_EMPTY_BOOK;
        } else if (st.filter_tab == 3) {
            empty_msg = Lang::Strings::CLOUD_EMPTY_FONT;
        }
        ShowMessage(st.list_body, empty_msg);
        SetPageFooterText("1 / 1");
        return;
    }

    ClampPage();
    const int start = st.page * st.page_size;
    const int end = std::min(start + st.page_size, static_cast<int>(st.filtered.size()));
    for (int fi = start; fi < end; ++fi) {
        const int i = st.filtered[static_cast<size_t>(fi)];
        CreateResourceRow(st.list_body, st.items[static_cast<size_t>(i)], i);
    }

    {
        char foot[48];
        std::snprintf(foot, sizeof(foot), "%d / %d", st.page + 1, PageCount());
        SetPageFooterText(foot);
    }

    if (schedule_thumbs && !st.loading && !st.waiting_net && !st.covers_loading &&
        !st.download_busy && !st.batch_busy && !s_dl_owns_http.load(std::memory_order_acquire)) {
        ScheduleCoverThumbFill();
    }
    RefreshFooterMode();
}

void RenderListPage() {
    RenderListPageInternal(true);
}

ScreenPaintCoalesce s_cloud_paint{};
ScreenPaintCoalesce s_cloud_check_paint{};

void RequestCloudRender() {
    if (s_cloud_paint.paint == nullptr) {
        s_cloud_paint.paint = RenderListPage;
    }
    ScreenPaintCoalesceRequest(&s_cloud_paint);
}

void RequestCheckMarksPaint() {
    if (s_cloud_check_paint.paint == nullptr) {
        s_cloud_check_paint.paint = PatchVisibleRowCheckMarks;
    }
    // 点间隙手指会抬起：勿立刻上屏，停手后再一次刷全部 √
    ScreenPaintCoalesceRequestDebounced(&s_cloud_check_paint, 280000);
}

bool OnVkKey(const char* key) {
    if (key == nullptr) {
        return true;
    }
    auto& st = State();
    if (std::strcmp(key, "vk_home") == 0) {
        RequestStopTransfer("vk_home");
        ScreenRequestHome();
        return true;
    }
    if (st.dl_page_open) {
        if (std::strcmp(key, "vk_prev") == 0) {
            ScreenLvAsync([](void*) { AbortDownloadJobFromUi(); });
            return true;
        }
        return true;
    }
    if (st.wallpaper_preview_open) {
        if (std::strcmp(key, "vk_prev") == 0) {
            ScreenLvAsync([](void*) { CloseWallpaperPreview(); });
            return true;
        }
        if (std::strcmp(key, "vk_next") == 0) {
            return true;
        }
        return true;
    }
    // 等网/拉列表中仍可翻页看已有数据；离页再 StopTransfer
    if (st.dialog_mask != nullptr || st.action_mask != nullptr) {
        return true;
    }
    if (std::strcmp(key, "vk_prev") == 0) {
        if (st.page > 0) {
            --st.page;
            RequestCloudRender();
            return true;
        }
        if (st.multi) {
            if (st.batch_busy || st.download_busy || st.delete_busy) {
                return true;
            }
            ScreenLvAsync([](void*) {
                if (ScreenAlive()) {
                    ExitMultiMode(true);
                }
            });
            return true;
        }
        RequestStopTransfer("vk_prev");
        ScreenLvAsync([](void*) {
            if (!ScreenAlive()) {
                return;
            }
            const char* active = VkKey_ActiveScreen();
            if (active == nullptr || std::strcmp(active, kScreenId) != 0) {
                ESP_LOGI(TAG, "vk_prev skip stale back: active=%s screen=%s",
                         active != nullptr ? active : "null", kScreenId);
                return;
            }
            ScreenNavigateBack();
        });
        return true;
    }
    if (std::strcmp(key, "vk_next") == 0) {
        if (st.page + 1 < PageCount()) {
            ++st.page;
            RequestCloudRender();
        }
        return true;
    }
    return true;
}

bool CloudPageRepeatStep(int page_delta) {
    auto& st = State();
    // 翻页看已有列表，不依赖 SyncBusy；等网中空列表 PageCount=1 自然不动
    if (page_delta == 0 || st.wallpaper_preview_open || st.dialog_mask != nullptr ||
        st.action_mask != nullptr) {
        return false;
    }
    const int last = std::max(0, PageCount() - 1);
    int next = st.page + page_delta;
    if (next < 0) {
        next = 0;
    } else if (next > last) {
        next = last;
    }
    if (next == st.page) {
        return false;
    }
    st.page = next;
    RequestCloudRender();
    return page_delta < 0 ? st.page > 0 : st.page < last;
}

bool OnVkKeyLongPress(const char* key) {
    return VkPageRepeatTryStart(key, CloudPageRepeatStep);
}

bool OnVkKeyPressUp(const char* key) {
    return VkPageRepeatOnPressUp(key);
}

void OnScreenDeleted(lv_event_t* e) {
    auto& st = State();
    if (lv_event_get_target(e) != st.screen) {
        return;
    }
    ScreenPaintCoalesceReset(&s_cloud_paint);
    ScreenPaintCoalesceReset(&s_cloud_check_paint);
    InvalidateSession();
    ReleaseUiKeepNet();
    if (auto* disp = Board::GetInstance().GetDisplay()) {
        disp->SetIdleStatusMode(IdleStatusMode::kClock);
    }
    if (st.wallpaper_preview_img != nullptr) {
        lv_image_set_src(st.wallpaper_preview_img, nullptr);
    }
    if (st.wallpaper_preview_raster != nullptr) {
        delete st.wallpaper_preview_raster;
        st.wallpaper_preview_raster = nullptr;
    }
    st.screen = nullptr;
    st.main_body = nullptr;
    st.list_body = nullptr;
    st.footer = nullptr;
    st.sync_btn = nullptr;
    st.sync_lbl = nullptr;
    st.multi_bar = nullptr;
    st.page_lbl = nullptr;
    st.section_title = nullptr;
    st.status_lbl = nullptr;
    for (int i = 0; i < kTabCount; ++i) {
        st.tab_btns[i] = nullptr;
        st.tab_lbls[i] = nullptr;
    }
    st.wallpaper_preview_body = nullptr;
    st.wallpaper_preview_title = nullptr;
    st.wallpaper_preview_meta = nullptr;
    st.wallpaper_preview_status = nullptr;
    st.wallpaper_preview_img = nullptr;
    st.wallpaper_preview_img_host = nullptr;
    st.wallpaper_download_btn = nullptr;
    st.wallpaper_download_lbl = nullptr;
    st.dialog_mask = nullptr;
    st.action_mask = nullptr;
    st.dl_page = nullptr;
    st.dl_bar = nullptr;
    st.dl_pct_lbl = nullptr;
    st.dl_count_lbl = nullptr;
    st.dl_title_lbl = nullptr;
    st.dl_page_open = false;
}

}  // namespace

lv_obj_t* CloudScreen::Create() {
    auto& st = State();
    InvalidateSession();
    CancelStatusClearTimer();
    st.items.clear();
    st.filtered.clear();
    st.selected.clear();
    st.batch_save_ids.clear();
    st.thumbs.clear();
    st.cover_bytes.clear();
    // InvalidateSession 已 bump epoch；勿清 busy/句柄，孤儿任务靠 epoch 自停
    ++st.thumb_epoch;
    st.error.clear();
    st.status_text[0] = '\0';
    st.page = 0;
    st.filter_tab = 0;
    st.fetch_done = false;
    st.covers_loading = false;
    st.multi = false;
    st.batch_busy = false;
    st.delete_busy = false;
    st.suppress_row_click_until_us = 0;
    st.suppress_row_click_idx = -1;
    st.main_body = nullptr;
    st.list_body = nullptr;
    st.footer = nullptr;
    st.sync_btn = nullptr;
    st.sync_lbl = nullptr;
    st.multi_bar = nullptr;
    st.page_lbl = nullptr;
    st.section_title = nullptr;
    st.status_lbl = nullptr;
    for (int i = 0; i < kTabCount; ++i) {
        st.tab_btns[i] = nullptr;
        st.tab_lbls[i] = nullptr;
    }
    st.wallpaper_preview_body = nullptr;
    st.wallpaper_preview_title = nullptr;
    st.wallpaper_preview_meta = nullptr;
    st.wallpaper_preview_status = nullptr;
    st.wallpaper_preview_img = nullptr;
    st.wallpaper_preview_img_host = nullptr;
    st.wallpaper_download_btn = nullptr;
    st.wallpaper_download_lbl = nullptr;
    st.wallpaper_preview_raster = nullptr;
    st.wallpaper_preview_idx = -1;
    st.wallpaper_preview_open = false;
    // 勿清 preview busy：孤儿靠 epoch 自停
    ++st.wallpaper_preview_epoch;
    st.dialog_mask = nullptr;
    st.action_mask = nullptr;
    st.dl_page = nullptr;
    st.dl_bar = nullptr;
    st.dl_pct_lbl = nullptr;
    st.dl_count_lbl = nullptr;
    st.dl_title_lbl = nullptr;
    st.dl_page_open = false;
    st.dl_user_abort = false;
    st.dl_job_total = 0;
    st.dl_job_done = 0;
    st.dl_shown_percent = -1;
    st.download_key.clear();
    st.dl_cancel_pending = false;
    st.dl_cancel_key.clear();
    st.download_type = reader::PushResourceType::kUnknown;
    // 不整页 UiKeepNet：空闲走保网计时后可 AppIdle 掉网（同清单）；拉列表/下载用 OtaDownload

    ScreenSetIsHome(false);

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, UiFont(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    DisableScroll(scr);
    st.screen = scr;
    lv_obj_add_event_cb(scr, OnScreenDeleted, LV_EVENT_DELETE, nullptr);

    EpdStatusBar status = ScreenCreateStatusBar(scr);
    // 顶栏保持时钟（勿改成「传输」）
    if (auto* disp = Board::GetInstance().GetDisplay()) {
        disp->SetIdleStatusMode(IdleStatusMode::kClock);
    }
    if (status.notification_label) {
        lv_obj_add_flag(status.notification_label, LV_OBJ_FLAG_HIDDEN);
    }

    s_last_user_sync_us = 0;
    // 为贴底页码留出 kFooterOutsideH，列表区可多挤一行
    const lv_coord_t body_h = LV_VER_RES - status.height - kFooterOutsideH;

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_HOR_RES, body_h);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(body, kPad, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(body, kRowGap, 0);
    DisableScroll(body);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_CLICKABLE);
    st.main_body = body;

    lv_obj_t* tab_row = lv_obj_create(body);
    lv_obj_remove_style_all(tab_row);
    lv_obj_set_width(tab_row, ContentWidth());
    lv_obj_set_height(tab_row, kTabH);
    lv_obj_set_style_bg_color(tab_row, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(tab_row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tab_row, lv_color_black(), 0);
    lv_obj_set_style_border_width(tab_row, kTabBorderW, 0);
    lv_obj_set_style_radius(tab_row, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(tab_row, true, 0);
    lv_obj_set_flex_flow(tab_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tab_row, kTabInset, 0);
    lv_obj_set_style_pad_column(tab_row, 0, 0);
    DisableScroll(tab_row);
    lv_obj_clear_flag(tab_row, LV_OBJ_FLAG_CLICKABLE);
    MakeTabBtn(tab_row, Lang::Strings::CLOUD_TAB_ALL, 0);
    MakeTabBtn(tab_row, Lang::Strings::CLOUD_TAB_WALLPAPER, 1);
    MakeTabBtn(tab_row, Lang::Strings::CLOUD_TAB_BOOK, 2);
    MakeTabBtn(tab_row, Lang::Strings::CLOUD_TAB_FONT, 3);
    RefreshTabUi();

    lv_obj_t* header_row = lv_obj_create(body);
    lv_obj_remove_style_all(header_row);
    lv_obj_set_width(header_row, ContentWidth());
    lv_obj_set_height(header_row, kHeaderH);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header_row, kHeaderGap, 0);
    DisableScroll(header_row);
    lv_obj_clear_flag(header_row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* star_wrap = lv_obj_create(header_row);
    lv_obj_remove_style_all(star_wrap);
    lv_obj_set_size(star_wrap, kHeaderIcon, kHeaderIcon);
    lv_obj_set_style_bg_color(star_wrap, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(star_wrap, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(star_wrap, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(star_wrap, 0, 0);
    DisableScroll(star_wrap);
    lv_obj_clear_flag(star_wrap, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* star = lv_label_create(star_wrap);
    lv_label_set_text(star, "\xE2\x98\x85");  // ★
    lv_obj_set_style_text_color(star, lv_color_white(), 0);
    lv_obj_set_style_text_font(star, ItemFont(), 0);
    lv_obj_center(star);
    lv_obj_clear_flag(star, LV_OBJ_FLAG_CLICKABLE);

    st.section_title = lv_label_create(header_row);
    lv_label_set_text(st.section_title, Lang::Strings::CLOUD_PENDING);
    lv_obj_set_style_text_font(st.section_title, UiFont(), 0);
    lv_obj_set_style_text_color(st.section_title, lv_color_black(), 0);
    lv_obj_clear_flag(st.section_title, LV_OBJ_FLAG_CLICKABLE);

    st.status_lbl = lv_label_create(header_row);
    lv_label_set_text(st.status_lbl, "");
    lv_obj_set_style_pad_left(st.status_lbl, kStatusGap, 0);
    lv_obj_set_style_text_font(st.status_lbl, ItemFont(), 0);
    lv_obj_set_style_text_color(st.status_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(st.status_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(st.status_lbl, LV_OBJ_FLAG_HIDDEN);

    // 细外框：包住列表 + 刷新；页码贴底在框外
    lv_obj_t* list_frame = lv_obj_create(body);
    lv_obj_remove_style_all(list_frame);
    lv_obj_set_width(list_frame, ContentWidth());
    lv_obj_set_flex_grow(list_frame, 1);
    lv_obj_set_style_bg_color(list_frame, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(list_frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list_frame, kListFrameBorderW, 0);
    lv_obj_set_style_border_color(list_frame, lv_color_black(), 0);
    lv_obj_set_style_radius(list_frame, kListFrameRadius, 0);
    lv_obj_set_style_pad_all(list_frame, kListFramePad, 0);
    lv_obj_set_flex_flow(list_frame, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_frame, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list_frame, 0, 0);
    DisableScroll(list_frame);
    lv_obj_clear_flag(list_frame, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* list_body = lv_obj_create(list_frame);
    lv_obj_remove_style_all(list_body);
    lv_obj_set_width(list_body, lv_pct(100));
    lv_obj_set_flex_grow(list_body, 1);
    DisableScroll(list_body);
    st.list_body = list_body;

    lv_obj_t* footer = lv_obj_create(list_frame);
    lv_obj_remove_style_all(footer);
    lv_obj_set_width(footer, lv_pct(100));
    lv_obj_set_height(footer, kFooterBlockH);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(footer, kSyncDividerW, 0);
    lv_obj_set_style_border_color(footer, lv_color_black(), 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DisableScroll(footer);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_CLICKABLE);
    st.footer = footer;

    // 分割线 + 居中「刷新」下划线（多选时隐藏，换 multi_bar）
    st.sync_btn = lv_obj_create(footer);
    lv_obj_remove_style_all(st.sync_btn);
    lv_obj_set_width(st.sync_btn, lv_pct(100));
    lv_obj_set_height(st.sync_btn, kSyncBtnH);
    lv_obj_set_style_bg_opa(st.sync_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(st.sync_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(st.sync_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    DisableScroll(st.sync_btn);
    lv_obj_add_flag(st.sync_btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(st.sync_btn);
    lv_obj_add_event_cb(st.sync_btn, OnSyncClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* sync_text = lv_obj_create(st.sync_btn);
    lv_obj_remove_style_all(sync_text);
    lv_obj_set_size(sync_text, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_border_side(sync_text, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(sync_text, kSyncUnderlineH, 0);
    lv_obj_set_style_border_color(sync_text, lv_color_black(), 0);
    lv_obj_set_style_pad_bottom(sync_text, 2, 0);
    lv_obj_set_style_pad_hor(sync_text, kSyncUnderlinePadHor, 0);
    lv_obj_set_style_bg_opa(sync_text, LV_OPA_TRANSP, 0);
    DisableScroll(sync_text);
    lv_obj_clear_flag(sync_text, LV_OBJ_FLAG_CLICKABLE);

    st.sync_lbl = lv_label_create(sync_text);
    lv_label_set_text(st.sync_lbl, Lang::Strings::CLOUD_REFRESH);
    lv_obj_set_style_text_font(st.sync_lbl, ItemFont(), 0);
    lv_obj_set_style_text_color(st.sync_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(st.sync_lbl, LV_OBJ_FLAG_CLICKABLE);

    st.multi_bar = lv_obj_create(footer);
    lv_obj_remove_style_all(st.multi_bar);
    lv_obj_set_width(st.multi_bar, lv_pct(100));
    lv_obj_set_height(st.multi_bar, kSyncBtnH);
    lv_obj_set_style_bg_opa(st.multi_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(st.multi_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(st.multi_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(st.multi_bar, 10, 0);
    DisableScroll(st.multi_bar);
    lv_obj_clear_flag(st.multi_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(st.multi_bar, LV_OBJ_FLAG_HIDDEN);

    // 与「刷新」同款：下划线文字；多项间居中「·」
    auto make_multi_action = [](lv_obj_t* parent, const char* text, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_obj_create(parent);
        lv_obj_remove_style_all(btn);
        lv_obj_set_height(btn, kSyncBtnH);
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
        lv_obj_set_style_border_width(text_wrap, kSyncUnderlineH, 0);
        lv_obj_set_style_border_color(text_wrap, lv_color_black(), 0);
        lv_obj_set_style_pad_bottom(text_wrap, 2, 0);
        lv_obj_set_style_pad_hor(text_wrap, kSyncUnderlinePadHor, 0);
        lv_obj_set_style_bg_opa(text_wrap, LV_OPA_TRANSP, 0);
        DisableScroll(text_wrap);
        lv_obj_clear_flag(text_wrap, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(text_wrap);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, ItemFont(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        return btn;
    };
    auto make_multi_dot = [](lv_obj_t* parent) {
        lv_obj_t* dot = lv_obj_create(parent);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, kMultiDotSize, kMultiDotSize);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        DisableScroll(dot);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        return dot;
    };
    make_multi_action(st.multi_bar, Lang::Strings::COMMON_CANCEL, OnMultiCancel);
    make_multi_dot(st.multi_bar);
    make_multi_action(st.multi_bar, Lang::Strings::COMMON_SELECT_ALL, OnMultiSelectAll);
    make_multi_dot(st.multi_bar);
    make_multi_action(st.multi_bar, Lang::Strings::COMMON_DELETE, OnMultiDelete);
    make_multi_dot(st.multi_bar);
    make_multi_action(st.multi_bar, Lang::Strings::CLOUD_SAVE, OnMultiSave);

    // 页码贴底，对齐壁纸页
    st.page_lbl = lv_label_create(scr);
    lv_label_set_text(st.page_lbl, "");
    lv_obj_set_width(st.page_lbl, LV_HOR_RES - 16);
    lv_obj_set_style_text_align(st.page_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(st.page_lbl, ItemFont(), 0);
    lv_obj_set_style_text_color(st.page_lbl, lv_color_black(), 0);
    lv_obj_align(st.page_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_clear_flag(st.page_lbl, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(st.page_lbl);

    // 固定 6 条/页（Tab+标题后列表区刚好可排下）
    st.page_size = 6;

    // 预览：图框 → 双行文件名 → 类型/大小胶囊 → 保存（对齐壁纸设置页）
    lv_obj_t* preview_body = lv_obj_create(scr);
    lv_obj_remove_style_all(preview_body);
    lv_obj_set_size(preview_body, LV_HOR_RES, LV_VER_RES - status.height);
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
    st.wallpaper_preview_body = preview_body;

    lv_obj_t* img_host = lv_obj_create(preview_body);
    lv_obj_remove_style_all(img_host);
    lv_obj_set_width(img_host, ContentWidth());
    lv_obj_set_flex_grow(img_host, 1);
    lv_obj_set_style_max_height(img_host, kPreviewImgMaxH, 0);
    lv_obj_set_style_bg_color(img_host, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(img_host, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(img_host, kRowBorderW, 0);
    lv_obj_set_style_border_color(img_host, lv_color_black(), 0);
    lv_obj_set_style_radius(img_host, 16, 0);
    lv_obj_set_style_clip_corner(img_host, true, 0);
    DisableScroll(img_host);
    st.wallpaper_preview_img_host = img_host;

    lv_obj_t* img = lv_image_create(img_host);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    st.wallpaper_preview_img = img;

    lv_obj_t* preview_status = lv_label_create(img_host);
    lv_obj_set_style_text_font(preview_status, UiFont(), 0);
    lv_obj_set_style_text_color(preview_status, lv_color_black(), 0);
    lv_obj_set_style_text_align(preview_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(preview_status, "");
    lv_obj_align(preview_status, LV_ALIGN_CENTER, 0, 0);
    st.wallpaper_preview_status = preview_status;

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
    st.wallpaper_preview_title = preview_title;

    lv_obj_t* preview_meta = lv_obj_create(preview_body);
    lv_obj_remove_style_all(preview_meta);
    lv_obj_set_width(preview_meta, ContentWidth());
    lv_obj_set_height(preview_meta, 40);
    lv_obj_set_flex_flow(preview_meta, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(preview_meta, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(preview_meta, 8, 0);
    lv_obj_clear_flag(preview_meta, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(preview_meta);
    st.wallpaper_preview_meta = preview_meta;

    lv_obj_t* download_btn = lv_obj_create(preview_body);
    lv_obj_remove_style_all(download_btn);
    lv_obj_set_size(download_btn, ContentWidth(), kActionH);
    lv_obj_set_style_pad_all(download_btn, 0, 0);
    lv_obj_set_flex_flow(download_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(download_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    DisableScroll(download_btn);
    HapticAttachClick(download_btn);
    lv_obj_add_event_cb(download_btn, OnWallpaperDownloadClicked, LV_EVENT_CLICKED, nullptr);
    st.wallpaper_download_btn = download_btn;

    lv_obj_t* download_lbl = lv_label_create(download_btn);
    lv_obj_set_style_text_font(download_lbl, UiFont(), 0);
    lv_label_set_text(download_lbl, Lang::Strings::CLOUD_SAVE_LOCAL);
    st.wallpaper_download_lbl = download_lbl;
    StyleDownloadBtn(true);

    ScheduleNetPrepAndFetch();

    VkKey_AttachScreen(scr, kScreenId,
                       VkKeyScreenDesc{CloudScreen::Create, OnVkKey, nullptr, nullptr, nullptr,
                                       nullptr, OnVkKeyLongPress, OnVkKeyPressUp});
    return scr;
}

void CloudScreen::StopTransfer() {
    RequestStopTransfer("api");
}
