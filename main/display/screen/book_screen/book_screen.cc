#include "book_screen.h"

// xtensa-g++：本文件过大时 -O2 可能 ICE（cfgcleanup try_forward_edges）
#pragma GCC optimize("O1")

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <memory>
#include <string>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "SdCardManager.hpp"
#include "assets/lang_config.h"
#include "assistant_screen/assistant_screen.h"
#include "book_screen/book_cover_loader.h"
#include "book_screen/book_reader_prefs.h"
#include "reader/book_cover_sidecar.h"
#include "reader/book_home_snapshot.h"
#include "reader/book_progress_cache.h"
#include "display/font/epdfont.h"
#include "display/font/ttf_convert.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "lv_adapter_display.h"
#include "reader/reader.h"
#include "reader/reader_page_render.h"
#include "reader/text_encoding.h"
#include "screen_common.h"
#include "sd_paths.h"
#include "vk_key_handler.h"
#include "vk_page_repeat.h"


namespace {

constexpr const char* TAG = "BookScreen";
constexpr const char* kScreenLibrary = "book";         // 阅读首页
constexpr const char* kScreenBookshelf = "book_shelf"; // 我的书架网格
constexpr const char* kScreenDetail = "book_detail";
constexpr const char* kScreenRead = "book_read";
// 轻量 epdfont：索引常驻 + 按页预热位图（替代整库 lv_binfont）
// 命名：misans_{px}_{bpp}.ef ，默认 25px / 2bpp；实际路径由 NVS 偏好决定
constexpr const char* kBookFontPath = SD_PATH_BOOK_FONT;
constexpr const char* kBookFontPathAlt = SD_PATH_BOOK_FONT_ALT;
constexpr lv_coord_t kListPad = 16;   // 首页/书架外边距
constexpr lv_coord_t kFooterH = 36;
// 分页高度相对正文区再收一点：末行字形/下划线(+4)可能略超 line_height，避免钻进底栏
constexpr lv_coord_t kReadViewportBottomSlack = 8;
constexpr lv_coord_t kShelfMultiBtnH = 36;     // 底栏多选行（与页码同槽，不挤书籍）
constexpr lv_coord_t kShelfUnderlineH = 1;
constexpr lv_coord_t kShelfUnderlinePadHor = 5;
constexpr lv_coord_t kShelfMultiDotSize = 6;
constexpr lv_coord_t kShelfCheckSize = 24;     // 封面角勾选
constexpr int64_t kShelfSuppressRowClickUs = 400000; // 只挡长按行松手假 CLICKED
constexpr int64_t kFontSuppressRowClickUs = 1200000;  // 设置卡刷行更慢，松手假点击窗更宽
constexpr uint64_t kShelfCheckPaintDebounceUs = 280000;
constexpr lv_coord_t kHomeShelfBtnH = 76;  // 「我的书架」按钮
constexpr lv_coord_t kHomeShelfBtnRadius = 8; // 方框略圆，勿全胶囊
constexpr lv_coord_t kHomeCardRadius = 14;
constexpr lv_coord_t kHomeGap = 12;
constexpr lv_coord_t kHomeSectionGap = 12; // 继续阅读 ↔ 最近阅读
constexpr lv_coord_t kHomeShelfTopGap = 22; // 封面 ↔ 我的书架
constexpr lv_coord_t kHomeBottomPad = 8;   // 书架距屏底再上抬
constexpr lv_coord_t kHomeStatsPad = 8;    // 统计区上下内边距
constexpr lv_coord_t kHomeStatsGap = 8;    // 统计行距
constexpr lv_coord_t kRecentCoverH = 290;  // 最近阅读封面高度上限（比例适中，勿吃满余高）
constexpr lv_coord_t kRecentTitleGap = 14; // 最近阅读封面与书名间距
constexpr lv_coord_t kShelfTitleGap = 6;
constexpr int kShelfCols = 3;
constexpr int kShelfRows = 3;             // 每页固定 3×3
constexpr lv_coord_t kShelfColGap = 12;
constexpr lv_coord_t kShelfRowGap = 16;    // 上下两排间距
constexpr lv_coord_t kCoverFrameBorder = 1; // 封面外框；图须缩进，避免 L8 盖住描边
constexpr lv_coord_t kCoverSpineInset = 6;  // 书脊竖线相对左边框内缩
constexpr lv_coord_t kTocCoverW = 194;  // 目录页封面（约原 216×0.9）
constexpr lv_coord_t kTocCoverH = 259;
constexpr lv_coord_t kTocHeadGap = 16;  // 封面与书名间距
constexpr lv_coord_t kTocRowH = 44;     // 目录章行（底部分隔线）
constexpr lv_coord_t kTocRowGap = 6;    // 字号列表行距；目录列表用底线不用此值
constexpr lv_coord_t kTocRowPad = 6;
constexpr lv_coord_t kTocHeadListGap = 12; // 书头与章节列表间距
constexpr lv_coord_t kTocBelowStatus = 12; // 封面与顶栏之间留白
constexpr lv_coord_t kTocLineThin = 1;     // 章间分隔线
constexpr lv_coord_t kTocLineCur = 3;      // 当前章上下分隔线
constexpr lv_coord_t kRowBorderW = 2;
constexpr lv_coord_t kRowRadius = 8;
constexpr lv_coord_t kReadPadTop = kListPad;  // 与左右边距一致
constexpr lv_coord_t kSheetSidePad = 12;      // 悬浮设置卡片左右边距
constexpr lv_coord_t kSheetInnerPad = 12;
constexpr lv_coord_t kSheetGap = 10;
constexpr lv_coord_t kSheetIconBtn = 36;
constexpr lv_coord_t kSheetRadius = 16;
constexpr lv_coord_t kSheetBorderW = 2;       // 卡片描边（加粗）
constexpr int kFontListPageSize = 5;         // 设置卡字体列表每页固定行数
constexpr lv_coord_t kFontListRowH = 44;     // 与目录行高一致，列表总高固定
constexpr uint32_t kLayoutDebounceMs = 300;   // 抬起且停顿后再排版（定时器内仍会等手指）
constexpr uint32_t kLayoutHintTickMs = 500;   // 「完成」提示到期检查
constexpr int64_t kLayoutHintDoneUs = 1500 * 1000;  // 「完成」显示时长
constexpr int kBuiltinReadingSizePx = 25; // 无 SD .ef 时正文回退 fontpack
constexpr int kTtfConvertSizeMin = 20;
constexpr int kTtfConvertSizeMax = 40;
constexpr int kTtfConvertSizeDefault = 25;

struct ReadFontEntry {
    std::string file;
    int size_px = 0;
};
constexpr lv_coord_t kDetailCoverMaxW = 152;
constexpr lv_coord_t kDetailCoverMaxH = 214;
constexpr lv_coord_t kDetailCtaBottom = 24; // 继续阅读距屏幕底（+8）
constexpr lv_coord_t kDetailProgCardH = 88; // 进度卡固定高（标题与百分比同行）
constexpr lv_coord_t kDetailTitleLines = 1; // 书名单行省略；其余控件整体上移一行
constexpr lv_coord_t kDetailSectionGap = 8; // 详情纵距收紧
constexpr lv_coord_t kOpenBarW = 320;
constexpr lv_coord_t kOpenBarH = 28;
constexpr uint32_t kOpenWorkerStack = 16 * 1024;
constexpr uint32_t kDetailCoverWorkerStack = 16 * 1024;
constexpr uint32_t kListCoverWorkerStack = 16 * 1024;
constexpr uint32_t kPageImageWorkerStack = 16 * 1024;
// 阅读时长周期落盘：30–60s 中取 45s，掉电最多丢约半分钟
constexpr uint32_t kReadTimeCheckpointMs = 45000;

/** 扣除双边描边后的可绘区，避免封面 L8 盖住外框 */
static inline int CoverFrameInner(lv_coord_t outer) {
    const lv_coord_t inset = 2 * kCoverFrameBorder;
    return static_cast<int>(outer > inset ? outer - inset : 1);
}

/** 书册右侧圆角半径：约宽的 1/6 */
static lv_coord_t BookCoverRightRadius(lv_coord_t w, lv_coord_t h) {
    lv_coord_t r = w / 6;
    if (r < 10) {
        r = 10;
    }
    const lv_coord_t max_r = std::min(w, h) / 2 - 2;
    if (max_r > 0 && r > max_r) {
        r = max_r;
    }
    return r > 0 ? r : 1;
}

/** 实心墨点（无 AA），直线/圆弧同一套，避免墨色深浅不一 */
static void CoverInkFill(lv_layer_t* layer, const lv_draw_rect_dsc_t& ink, lv_coord_t x1,
                         lv_coord_t y1, lv_coord_t x2, lv_coord_t y2) {
    if (x1 > x2 || y1 > y2) {
        return;
    }
    lv_area_t a;
    a.x1 = x1;
    a.y1 = y1;
    a.x2 = x2;
    a.y2 = y2;
    lv_draw_rect(layer, &ink, &a);
}

/** 擦掉右上/右下「方角耳朵」：圆外整行涂白（与描边共用圆心半径） */
void EraseBookCoverRightEars(lv_layer_t* layer, const lv_area_t& box, lv_coord_t r) {
    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = lv_color_white();
    rd.bg_opa = LV_OPA_COVER;
    rd.border_width = 0;
    rd.radius = 0;
    const lv_coord_t cx = box.x2 - r;
    const int rr = static_cast<int>(r) * static_cast<int>(r);
    for (lv_coord_t dy = 0; dy < r; ++dy) {
        const int t = static_cast<int>(r - dy);
        // 圆内最右像素 = cx+floor(sqrt)；其右一列起为耳朵
        const lv_coord_t x0 =
            cx + static_cast<lv_coord_t>(std::floor(std::sqrt(static_cast<float>(rr - t * t)))) + 1;
        if (x0 > box.x2) {
            continue;
        }
        CoverInkFill(layer, rd, x0, box.y1 + dy, box.x2, box.y1 + dy);
        CoverInkFill(layer, rd, x0, box.y2 - dy, box.x2, box.y2 - dy);
    }
}

/** 右上/右下四分之一圆外轮廓：Bresenham 连续实心点，与直线边重叠 1px 衔接 */
static void DrawBookCoverCornerArcs(lv_layer_t* layer, const lv_draw_rect_dsc_t& ink,
                                    const lv_area_t& box, lv_coord_t r) {
    const lv_coord_t bw = kCoverFrameBorder;
    const lv_coord_t cx = box.x2 - r;
    const lv_coord_t cy_top = box.y1 + r;
    const lv_coord_t cy_bot = box.y2 - r;
    auto plot = [&](lv_coord_t x, lv_coord_t y) {
        if (x < box.x1 || x > box.x2 || y < box.y1 || y > box.y2) {
            return;
        }
        CoverInkFill(layer, ink, x, y, x + bw - 1, y + bw - 1);
    };

    int x = 0;
    int y = static_cast<int>(r);
    int err = 1 - y;
    while (x <= y) {
        plot(cx + x, cy_top - y);
        plot(cx + y, cy_top - x);
        plot(cx + x, cy_bot + y);
        plot(cx + y, cy_bot + x);
        if (err < 0) {
            err += 2 * x + 3;
        } else {
            err += 2 * (x - y) + 5;
            --y;
        }
        ++x;
    }
}

void DrawBookCoverBorder(lv_layer_t* layer, const lv_area_t& box, lv_coord_t r) {
    lv_draw_rect_dsc_t ink;
    lv_draw_rect_dsc_init(&ink);
    ink.bg_color = lv_color_black();
    ink.bg_opa = LV_OPA_COVER;
    ink.border_width = 0;
    ink.radius = 0;
    const lv_coord_t bw = kCoverFrameBorder;
    const lv_coord_t cx = box.x2 - r;

    // 左边框（整高）
    CoverInkFill(layer, ink, box.x1, box.y1, box.x1 + bw - 1, box.y2);
    // 顶/底直线接到圆角起点（含 cx，与弧端点重叠）
    CoverInkFill(layer, ink, box.x1, box.y1, cx, box.y1 + bw - 1);
    CoverInkFill(layer, ink, box.x1, box.y2 - bw + 1, cx, box.y2);
    // 右侧直线（含圆角端点行，与弧重叠）
    CoverInkFill(layer, ink, box.x2 - bw + 1, box.y1 + r, box.x2, box.y2 - r);

    // 书脊细线（左边框内侧）
    CoverInkFill(layer, ink, box.x1 + kCoverSpineInset, box.y1 + bw,
                 box.x1 + kCoverSpineInset + bw - 1, box.y2 - bw);

    DrawBookCoverCornerArcs(layer, ink, box, r);
}

void OnBookCoverFrameDraw(lv_event_t* e) {
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    if (obj == nullptr || layer == nullptr) {
        return;
    }
    lv_area_t box;
    lv_obj_get_coords(obj, &box);
    const lv_coord_t w = lv_area_get_width(&box);
    const lv_coord_t h = lv_area_get_height(&box);
    if (w < 8 || h < 8) {
        return;
    }
    const lv_coord_t r = BookCoverRightRadius(w, h);
    EraseBookCoverRightEars(layer, box, r);
    DrawBookCoverBorder(layer, box, r);
}

/** 封面外框：左直脊、右圆角（子控件绘完后再切角描边） */
void StyleBookCoverFrame(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_clip_corner(obj, false, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_add_event_cb(obj, OnBookCoverFrameDraw, LV_EVENT_DRAW_POST_END, nullptr);
}

epdfont_t* s_epdfont = nullptr;

/** 书库/详情等界面标题：固件 fontpack 30@2 */
const lv_font_t* ListFont() {
    const lv_font_t* f = fontpack_lv_font_get(30, 2);
    return f != nullptr ? f : fontpack_lv_font_ui();
}

/** 书库列表 item：固件 fontpack 25@2 */
const lv_font_t* ItemFont() {
    return fontpack_lv_font_ui();
}

/** 仅正文内容：SD epdfont（NVS 所选），失败回退 fontpack UI */
const lv_font_t* BookFont() {
    const lv_font_t* f = epdfont_get_lv_font(s_epdfont);
    return f != nullptr ? f : fontpack_lv_font_ui();
}

void EnsureBookFont() {
    if (s_epdfont != nullptr) {
        return;
    }
    char path[192];
    if (BookReaderPrefsFontFullPath(path, sizeof(path)) != nullptr) {
        s_epdfont = epdfont_open(path);
    }
    if (s_epdfont == nullptr) {
        s_epdfont = epdfont_open(kBookFontPath);
    }
    if (s_epdfont == nullptr) {
        s_epdfont = epdfont_open(kBookFontPathAlt);
    }
    if (s_epdfont == nullptr) {
        ESP_LOGW(TAG, "epdfont load failed (%s), fallback MiSans flash",
                 BookReaderPrefsFontFile());
    } else {
        ESP_LOGI(TAG, "epdfont loaded file=%s glyphs=%u bpp=%u", BookReaderPrefsFontFile(),
                 static_cast<unsigned>(epdfont_glyph_count(s_epdfont)), epdfont_get_bpp(s_epdfont));
    }
}

void ReleaseBookFont() {
    if (s_epdfont == nullptr) {
        return;
    }
    epdfont_close(s_epdfont);
    s_epdfont = nullptr;
}

void PrewarmPageFont(const reader::Page* page) {
    if (s_epdfont == nullptr || page == nullptr) {
        return;
    }
    std::string blob;
    blob.reserve(2048);
    for (const auto& item : page->items) {
        if (item.kind == reader::ContentKind::kText) {
            blob.append(item.text);
            blob.push_back('\n');
        }
    }
    if (!blob.empty()) {
        epdfont_prewarm_utf8(s_epdfont, blob.c_str(), blob.size());
    }
}

struct BookUiState {
    std::vector<reader::BookInfo> books;
    std::unique_ptr<reader::BookSession> session;
    int selected = -1;
    int list_page = 0;
    int list_page_size = 9; // 书架默认 3×3；首页不用
    lv_coord_t shelf_cover_h = 0; // 书架封面高（按三行可用高度算）

    // 详情返回根：首页或书架
    enum class NavRoot : uint8_t { kHome = 0, kShelf };
    NavRoot back_root = NavRoot::kHome;

    lv_obj_t* list_scr = nullptr;
    lv_obj_t* list_body = nullptr;
    lv_obj_t* list_footer = nullptr; // 页码；多选时靠右下
    lv_obj_t* multi_bar = nullptr;   // 取消 / 全选 / 移除
    lv_obj_t* status_label = nullptr;
    bool shelf_multi = false;
    std::vector<uint8_t> shelf_selected; // 与 books 等长
    int64_t shelf_suppress_click_until_us = 0;
    int shelf_suppress_click_idx = -1;

    // 首页最近阅读 / 书架网格：旁路封面像素（控件只引用 dsc）
    std::vector<std::unique_ptr<reader::RasterImage>> recent_covers;
    std::vector<std::unique_ptr<reader::RasterImage>> shelf_covers;
    // 列表/目录：无旁路时串行后台抽内嵌；token 作废翻页/离页迟到回调
    std::atomic<uint32_t> list_cover_token{0};
    struct ListCoverPending {
        reader::BookInfo info;
        int max_w = 0;
        int max_h = 0;
        lv_obj_t* host = nullptr;
        reader::RasterImage* slot = nullptr;  // recent/shelf/detail_cover，非拥有
    };
    std::vector<ListCoverPending> list_cover_pending;
    // 已确认无内嵌封面的路径（进程内）；避免每次回书架重复 Open 抽封面
    std::vector<std::string> cover_embed_miss;

    lv_obj_t* detail_scr = nullptr;
    reader::RasterImage detail_cover;
    // 无旁路时后台补文件内封面；token 作废离开/换书的迟到回调
    std::atomic<uint32_t> detail_cover_token{0};
    lv_obj_t* detail_cover_host = nullptr;
    lv_coord_t detail_cover_fw = kDetailCoverMaxW; // 详情封面外框宽
    lv_coord_t detail_cover_fh = kDetailCoverMaxH; // 详情封面外框高
    lv_obj_t* detail_title_lbl = nullptr;
    lv_obj_t* detail_meta_lbl = nullptr;
    reader::BookFormat detail_format = reader::BookFormat::kTxt;

    lv_obj_t* read_scr = nullptr;
    lv_obj_t* content = nullptr;
    lv_obj_t* page_label = nullptr;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* open_bar = nullptr;
    lv_obj_t* open_pct_lbl = nullptr;

    // 阅读浮层状态机（均不另开 screen）：
    // kSettings → 顶部悬浮设置卡片（盖在正文上）；kToc → 目录整页
    enum class ReadChrome : uint8_t {
        kReading = 0,
        kToc,
        kSettings,
    };
    ReadChrome read_chrome = ReadChrome::kReading;
    bool settings_resume_after_open = false;  // 整本重开失败回退时回到设置卡片
    int toc_list_page = 0;
    int toc_page_size = 8;
    lv_obj_t* status_bar = nullptr;
    lv_obj_t* status_overlay = nullptr;
    lv_coord_t status_h = 0;
    lv_obj_t* settings_sheet = nullptr;  // 悬浮设置卡片（盖正文）
    // 设置卡控件缓存：字体翻页 / 边距行距 ± 原地刷，避免整卡 clean
    struct SheetFontRow {
        lv_obj_t* row = nullptr;
        lv_obj_t* name = nullptr;
        lv_obj_t* px = nullptr;
        lv_obj_t* check = nullptr; // 多选勾选框
    };
    SheetFontRow sheet_font_rows[kFontListPageSize]{};
    lv_obj_t* sheet_font_title = nullptr; // 「字体」/「已选 N」
    lv_obj_t* sheet_font_page_prev = nullptr;
    lv_obj_t* sheet_font_page_next = nullptr;
    lv_obj_t* sheet_font_page_lab = nullptr;
    lv_obj_t* sheet_font_multi_bar = nullptr; // 多选时插入的操作行：取消 / 全选 / 移除
    lv_obj_t* sheet_margin_value = nullptr;
    lv_obj_t* sheet_margin_dec = nullptr;
    lv_obj_t* sheet_margin_inc = nullptr;
    lv_obj_t* sheet_margin_bound = nullptr;
    lv_obj_t* sheet_gap_value = nullptr;
    lv_obj_t* sheet_gap_dec = nullptr;
    lv_obj_t* sheet_gap_inc = nullptr;
    lv_obj_t* sheet_gap_bound = nullptr;
    lv_coord_t viewport_w = 0;
    lv_coord_t viewport_h = 0;
    std::vector<ReadFontEntry> font_entries;
    int font_list_page = 0;  // 设置卡字体列表页码
    bool font_multi = false;
    std::vector<uint8_t> font_selected; // 与 font_entries 等长
    int64_t font_suppress_click_until_us = 0;
    int font_suppress_click_idx = -1;
    bool font_suppress_next_click = false; // 长按进多选后吃掉松手假 CLICKED

    // TTF 导入转换：面板覆盖设置卡（随 settings_sheet 隐藏/重建）
    enum class TtfMode : uint8_t { kNone, kList, kConfirm, kProgress, kResult };
    TtfMode ttf_mode = TtfMode::kNone;
    std::vector<std::string> ttf_files;
    int ttf_page = 0;
    std::string ttf_pick;      // 选中的字体文件名（fonts_ttf 内）
    int ttf_size_px = 25;      // 确认页步进器：20–40，默认 25；只输出一档
    bool ttf_result_ok = false;
    lv_obj_t* sheet_font_import = nullptr; // 字体标题行右侧「导入 TTF」
    lv_obj_t* ttf_sheet = nullptr;
    struct TtfRow {
        lv_obj_t* row = nullptr;
        lv_obj_t* name = nullptr;
    };
    TtfRow ttf_rows[kFontListPageSize]{};
    lv_obj_t* ttf_bar = nullptr;
    lv_obj_t* ttf_pct_lbl = nullptr;
    lv_obj_t* ttf_msg_lbl = nullptr;
    lv_obj_t* ttf_size_value = nullptr;
    lv_obj_t* ttf_size_dec = nullptr;
    lv_obj_t* ttf_size_inc = nullptr;
    lv_timer_t* ttf_poll_timer = nullptr;

    // 正文触摸：长按后抑制紧随其后的 CLICKED
    struct ReadTouch {
        bool suppress_click = false;
    } read_touch;

    // 异步打开：worker 解析，经 ScreenLvAsync 回 LVGL（禁止 worker 裸 lv_async_call）
    std::atomic<bool> opening{false};
    std::atomic<uint32_t> open_token{0};
    bool deferred_cleanup = false;  // 打开中离开：结束后再关 session/字库
    reader::BookFormat opening_format = reader::BookFormat::kTxt;

    // TXT 换参：当前页预览后后台建索引；busy 期间翻页禁用，连点只记 again
    std::atomic<bool> layout_busy{false};
    std::atomic<uint32_t> layout_token{0};
    bool layout_again = false;
    bool layout_reload_font = false;
    reader::BookSession::LayoutAnchor layout_anchor{};
    // ±/字体：卡文案 coalesce；正文排版等抬起+停顿；全书页表关设置卡后再建
    lv_timer_t* layout_debounce_timer = nullptr;
    bool layout_debounce_reload_font = false;
    // 章节全书页表后台重建：不挡翻页（与 layout_busy 分离）
    std::atomic<bool> chapter_pages_busy{false};
    std::atomic<uint32_t> chapter_pages_token{0};
    bool chapter_pages_again = false;
    // 正文插图流式解码：不挡翻页；token 作废翻页/离页迟到回调
    std::atomic<bool> page_image_busy{false};
    std::atomic<uint32_t> page_image_token{0};
    std::atomic<bool> page_image_abort{false};
    bool page_image_again = false;
    struct PageImagePending {
        std::string href;
        int max_w = 0;
        int max_h = 0;
        lv_obj_t* slot = nullptr; // 占位容器；回调时校验仍有效
    };
    std::vector<PageImagePending> page_image_pending;
    // 底栏总页旁：编排中 / 完成
    lv_timer_t* layout_hint_timer = nullptr;
    int64_t layout_hint_done_until_us = 0;

    // 阅读时长 checkpoint：仅 LVGL 任务启停；周期 fold+写 .pos
    lv_timer_t* read_time_ckpt_timer = nullptr;
    bool reading_paused_by_standby = false; // 进待机停钟，退出待机可恢复
};

ScreenPaintCoalesce s_reader_paint{};
ScreenPaintCoalesce s_library_paint{};
ScreenPaintCoalesce s_shelf_check_paint{};
ScreenPaintCoalesce s_toc_paint{};
ScreenPaintCoalesce s_settings_sheet_paint{}; // 设置卡：连点只记状态，抬起后一次刷控件
// 盖板键/长按定时器只累加；真正 Next/Prev 在 LVGL 绘制里做，避免跨章重建 pages_ 时 UAF
std::atomic<int> s_reader_page_delta{0};

struct OpenProgressMsg {
    uint32_t token = 0;
    int percent = 0;
};

struct OpenDoneMsg {
    uint32_t token = 0;
    bool ok = false;
};

struct OpenWorkerArg {
    uint32_t token = 0;
    reader::BookInfo info;  // 拷贝，避免 Create 重建书库时 vector 失效
};

struct DetailCoverDoneMsg {
    uint32_t token = 0;
    bool cover_ok = false;
    reader::RasterImage* cover = nullptr;
    std::string title;
    std::string author;
    std::string path;
};

struct DetailCoverWorkerArg {
    uint32_t token = 0;
    reader::BookInfo info;
    int max_w = 0;
    int max_h = 0;
};

struct PageImageDoneMsg {
    uint32_t token = 0;
    lv_obj_t* slot = nullptr;
    reader::RasterImage* image = nullptr;
    std::string href;
    bool ok = false;
    bool page_images = false;
};

struct PageImageWorkItem {
    std::string href;
    int max_w = 0;
    int max_h = 0;
    lv_obj_t* slot = nullptr;
};

struct PageImageWorkerArg {
    uint32_t token = 0;
    std::string book_path;
    reader::BookFormat format = reader::BookFormat::kTxt;
    bool page_images = false;
    std::vector<PageImageWorkItem> items;
};

struct ListCoverDoneMsg {
    uint32_t token = 0;
    lv_obj_t* host = nullptr;
    reader::RasterImage* slot = nullptr;
    reader::RasterImage* cover = nullptr;
    std::string path;
    bool cover_ok = false;
};

struct ListCoverFillWork {
    uint32_t token = 0;
    std::vector<BookUiState::ListCoverPending> items;
};

struct CoverMissMsg {
    std::string path;
};

BookUiState& State() {
    static BookUiState s;
    return s;
}

bool IsReadOverlayChrome(BookUiState::ReadChrome c) {
    using C = BookUiState::ReadChrome;
    return c == C::kToc || c == C::kSettings;
}

void BindSessionLayoutPrefs(reader::BookSession& session) {
    BookReaderPrefsEnsureLoaded();
    session.SetFont(BookFont());
    session.SetGaps(static_cast<lv_coord_t>(BookReaderPrefsLineGap()),
                    static_cast<lv_coord_t>(BookReaderPrefsParaGap()));
}

/**
 * 正文内容区宽度（阅读页边距）；与沉浸态居中栏宽一致。
 * 书库/详情继续用 ContentWidth()（kListPad），互不影响。
 */
lv_coord_t ReadContentWidth() {
    BookReaderPrefsEnsureLoaded();
    const int ml = BookReaderPrefsMarginLeft();
    const int mr = BookReaderPrefsMarginRight();
    lv_coord_t w = LV_HOR_RES - static_cast<lv_coord_t>(ml + mr);
    return w > 40 ? w : 40;
}

/** 目录/设置等列表：固定用书库边距，避免阅读页边距档位挤乱菜单行宽 */
void ApplyOverlayListContentPads(lv_coord_t top_pad) {
    auto& st = State();
    if (st.content == nullptr) {
        return;
    }
    lv_obj_set_style_pad_top(st.content, top_pad, 0);
    lv_obj_set_style_pad_bottom(st.content, 0, 0);
    lv_obj_set_style_pad_left(st.content, kListPad, 0);
    lv_obj_set_style_pad_right(st.content, kListPad, 0);
}

/**
 * 阅读区几何模式（与 read_chrome 解耦，避免「浮层态算出的 viewport 写进 session」）：
 * - kImmersive：正文分页/渲染唯一真源；hide_prog 时不占底栏高度
 * - kOverlayList：目录/设置等临时 UI，始终留底栏文案区；不得用其结果 SetViewport
 */
enum class ReaderGeomMode : uint8_t { kImmersive = 0, kOverlayList };

/** 是否整页图模式（未 Open 时用 Peek，便于建屏设 viewport） */
bool ReaderPageImagesHint() {
    auto& st = State();
    if (st.session && st.session->IsOpen()) {
        return st.session->IsPageImagesMode();
    }
    if (st.selected < 0 || st.selected >= static_cast<int>(st.books.size())) {
        return false;
    }
    const reader::BookInfo& info = st.books[static_cast<size_t>(st.selected)];
    return info.format == reader::BookFormat::kEbook &&
           reader::EbookDocument::PeekPageImages(info.path.c_str());
}

/**
 * @brief 按偏好刷新正文 body 尺寸/边距/底栏（须在 LVGL 任务）
 * @param mode 仅 kImmersive 更新 st.viewport_*（供 SetViewport/重开）；浮层保留上次沉浸值
 */
void ApplyReaderPageGeometry(ReaderGeomMode mode) {
    auto& st = State();
    if (st.read_scr == nullptr || st.content == nullptr || !lv_obj_is_valid(st.content)) {
        return;
    }
    BookReaderPrefsEnsureLoaded();
    const bool show_footer =
        (mode == ReaderGeomMode::kOverlayList) || (BookReaderPrefsHideProgress() == 0);
    const lv_coord_t footer_h = show_footer ? kFooterH : 0;
    const lv_coord_t body_h = LV_VER_RES - footer_h;

    if (st.page_label != nullptr && lv_obj_is_valid(st.page_label)) {
        if (show_footer) {
            lv_obj_clear_flag(st.page_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(st.page_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_set_size(st.content, LV_HOR_RES, body_h);
    lv_obj_align(st.content, LV_ALIGN_TOP_MID, 0, 0);
    // 子控件画出 body 时勿盖住底栏
    lv_obj_remove_flag(st.content, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // 浮层列表：只调 body/底栏；pad 由 ApplyOverlayListContentPads 设置，且禁止改 viewport_*
    if (mode == ReaderGeomMode::kOverlayList) {
        return;
    }

    const bool page_images = ReaderPageImagesHint();
    const int mt = BookReaderPrefsMarginTop();
    const int mb = BookReaderPrefsMarginBottom();
    if (page_images) {
        lv_obj_set_style_pad_top(st.content, 0, 0);
        lv_obj_set_style_pad_bottom(st.content, 0, 0);
        lv_obj_set_style_pad_left(st.content, 0, 0);
        lv_obj_set_style_pad_right(st.content, 0, 0);
        st.viewport_w = LV_HOR_RES;
        st.viewport_h = body_h;
    } else {
        // 左右 pad=0：水平居中由 RenderReaderPage 显式算 x，避免 align/flex 失效时贴左
        lv_obj_set_style_pad_top(st.content, static_cast<lv_coord_t>(mt), 0);
        lv_obj_set_style_pad_bottom(st.content, static_cast<lv_coord_t>(mb), 0);
        lv_obj_set_style_pad_left(st.content, 0, 0);
        lv_obj_set_style_pad_right(st.content, 0, 0);
        st.viewport_w = ReadContentWidth();
        lv_coord_t vh =
            body_h - static_cast<lv_coord_t>(mt + mb) - kReadViewportBottomSlack;
        st.viewport_h = vh > 40 ? vh : 40;
    }
}

void StopReadTimeCheckpointTimer() {
    auto& st = State();
    if (st.read_time_ckpt_timer == nullptr) {
        return;
    }
    lv_timer_del(st.read_time_ckpt_timer);
    st.read_time_ckpt_timer = nullptr;
}

void OnReadTimeCheckpoint(lv_timer_t* /*t*/) {
    auto& st = State();
    // 打开中 / session 已关：跳过；不删 timer（AsyncOpenDone 会重建）
    if (st.opening.load() || !st.session || !st.session->IsOpen()) {
        return;
    }
    const bool ok = st.session->SaveProgress();
    const uint32_t sec = st.session->ReadingSeconds();
    const uint32_t daily = st.session->ReadingDailySeconds();
    const uint32_t min = sec / 60u;
    const uint32_t rem = sec % 60u;
    ESP_LOGI(TAG, "read-time checkpoint (%us): %s total=%u sec (%u min %u sec) daily=%u sec title=%s",
             static_cast<unsigned>(kReadTimeCheckpointMs / 1000u), ok ? "saved" : "FAIL",
             static_cast<unsigned>(sec), static_cast<unsigned>(min), static_cast<unsigned>(rem),
             static_cast<unsigned>(daily), st.session->Title().c_str());
}

void StartReadTimeCheckpointTimer() {
    auto& st = State();
    StopReadTimeCheckpointTimer();
    st.read_time_ckpt_timer =
        lv_timer_create(OnReadTimeCheckpoint, kReadTimeCheckpointMs, nullptr);
    if (st.read_time_ckpt_timer == nullptr) {
        ESP_LOGW(TAG, "read time checkpoint timer create failed");
    }
}

/** 正文可读后开始累计；须在 LVGL 任务调用 */
void BeginReadingTimeTracking() {
    auto& st = State();
    if (!st.session || !st.session->IsOpen()) {
        return;
    }
    const uint32_t sec = st.session->ReadingSeconds();
    const uint32_t daily = st.session->ReadingDailySeconds();
    const uint32_t min = sec / 60u;
    const uint32_t rem = sec % 60u;
    ESP_LOGI(TAG, "read-time open: total=%u sec (%u min %u sec) daily=%u sec title=%s path=%s",
             static_cast<unsigned>(sec), static_cast<unsigned>(min), static_cast<unsigned>(rem),
             static_cast<unsigned>(daily), st.session->Title().c_str(),
             st.session->Info().path.c_str());
    st.session->StartReadingClock();
    StartReadTimeCheckpointTimer();
}

/**
 * 离开正文前停 checkpoint + 停墙钟（秒数进内存）。
 * session.reset()/Close 会再 SaveProgress 落盘；本函数不写盘，避免双写路径分叉。
 */
void EndReadingTimeTracking() {
    StopReadTimeCheckpointTimer();
    auto& st = State();
    st.reading_paused_by_standby = false;
    if (st.session && st.session->IsOpen()) {
        st.session->StopReadingClock();
    }
}

/** 进待机：停累计；退出待机由 ResumeReadingTimeAfterStandby 恢复 */
void PauseReadingTimeForStandby() {
    auto& st = State();
    if (!st.session || !st.session->IsReadingClockActive()) {
        return;
    }
    EndReadingTimeTracking();
    st.reading_paused_by_standby = true;
    ESP_LOGI(TAG, "read-time paused for standby");
}

void ResumeReadingTimeAfterStandby() {
    auto& st = State();
    if (!st.reading_paused_by_standby) {
        return;
    }
    st.reading_paused_by_standby = false;
    BeginReadingTimeTracking();
    ESP_LOGI(TAG, "read-time resumed after standby");
}

/** 首页统计：总时长短文案（如 50.8h / 32m） */
void FormatHomeDurationShort(char* out, size_t out_sz, uint32_t seconds) {
    if (out == nullptr || out_sz == 0) {
        return;
    }
    if (seconds < 60u) {
        std::snprintf(out, out_sz, "0m");
        return;
    }
    const uint32_t total_min = seconds / 60u;
    if (total_min < 60u) {
        std::snprintf(out, out_sz, "%um", static_cast<unsigned>(total_min));
        return;
    }
    const double hours = static_cast<double>(seconds) / 3600.0;
    std::snprintf(out, out_sz, "%.1fh", hours);
}

struct HomeStats {
    int continue_index = -1;     // 继续阅读书目下标；-1 无
    int continue_progress_x10 = -1;
    uint32_t total_seconds = 0;
    int finished_count = 0;
    int recent_indices[2] = {-1, -1};
    bool has_recent = false;  // NVS MRU 非空且能对上当前书库
};

/**
 * 首页统计：不再全库 Peek .pos。
 * - 最近/继续：book_home_snapshot MRU（写进度时维护）；空则不展示
 * - 总时长/读完：开机 sync Peek 累加快照；未同步过则显示 0
 * - 仅对「继续阅读」那一本 Peek 进度百分比（实时）
 */
HomeStats CollectHomeStats(const std::vector<reader::BookInfo>& books) {
    HomeStats out;
    const auto agg = reader::book_home_snapshot::LoadAggregateStats();
    if (agg.valid) {
        out.total_seconds = agg.total_seconds;
        out.finished_count = agg.finished_count;
    }

    if (books.empty()) {
        return out;
    }

    std::vector<std::string> recent_abs;
    reader::book_home_snapshot::LoadRecentAbsPaths(recent_abs);
    if (recent_abs.empty()) {
        return out;
    }

    int recent_n = 0;
    auto push_recent = [&](int idx) {
        if (idx < 0 || recent_n >= 2) {
            return;
        }
        for (int k = 0; k < recent_n; ++k) {
            if (out.recent_indices[k] == idx) {
                return;
            }
        }
        out.recent_indices[recent_n++] = idx;
    };

    for (const std::string& path : recent_abs) {
        int found = -1;
        for (int i = 0; i < static_cast<int>(books.size()); ++i) {
            if (books[static_cast<size_t>(i)].path == path) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            continue;
        }
        push_recent(found);
        if (recent_n >= 2) {
            break;
        }
    }

    if (recent_n <= 0) {
        return out;
    }

    out.has_recent = true;
    out.continue_index = out.recent_indices[0];
    {
        reader::BookSession::ProgressPeek peek;
        const char* path = books[static_cast<size_t>(out.continue_index)].path.c_str();
        if (reader::book_progress_cache::TryGet(path, peek) && peek.progress_x10 >= 0) {
            out.continue_progress_x10 = peek.progress_x10;
        } else {
            out.continue_progress_x10 = reader::BookSession::PeekReadingProgressX10(path);
        }
    }
    return out;
}

lv_coord_t ContentWidth() {
    return LV_HOR_RES - kListPad * 2;
}

lv_coord_t MeasureTextWidth(const lv_font_t* font, const char* text) {
    if (font == nullptr || text == nullptr || text[0] == '\0') {
        return 0;
    }
    lv_point_t sz = {};
    lv_text_get_size(&sz, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return static_cast<lv_coord_t>(sz.x);
}

/** 单行截断：过长则末尾加「…」，按像素宽度预算。 */
std::string TruncateTextToWidth(const std::string& text, const lv_font_t* font, lv_coord_t max_w) {
    if (font == nullptr || max_w <= 0 || MeasureTextWidth(font, text.c_str()) <= max_w) {
        return text;
    }
    constexpr const char* kEllipsis = "…";
    const lv_coord_t ellipsis_w = MeasureTextWidth(font, kEllipsis);
    const lv_coord_t budget = max_w - ellipsis_w;
    if (budget <= 0) {
        return kEllipsis;
    }
    std::string out;
    out.reserve(text.size() + 3);
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

void DisableScroll(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(obj, LV_DIR_NONE);
}

void ShowMessage(lv_obj_t* parent, const char* msg, const lv_font_t* font = nullptr) {
    auto& st = State();
    if (parent == st.content) {
        st.open_bar = nullptr;
        st.open_pct_lbl = nullptr;
    }
    lv_obj_clean(parent);
    // 提示页需屏幕居中；关掉 flex，避免 START 把文案顶到上方
    lv_obj_set_style_layout(parent, LV_LAYOUT_NONE, 0);
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, msg != nullptr ? msg : "");
    lv_obj_set_style_text_font(label, font != nullptr ? font : ListFont(), 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, ContentWidth());
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_center(label);
    DisableScroll(label);
}

int ListPageCount() {
    auto& st = State();
    if (st.books.empty() || st.list_page_size <= 0) {
        return 1;
    }
    return static_cast<int>((st.books.size() + static_cast<size_t>(st.list_page_size) - 1) /
                            static_cast<size_t>(st.list_page_size));
}

void ClampListPage() {
    auto& st = State();
    const int pages = ListPageCount();
    if (st.list_page < 0) {
        st.list_page = 0;
    }
    if (st.list_page >= pages) {
        st.list_page = pages - 1;
    }
}

int EnsureBookInLibrary(const reader::BookInfo& info) {
    auto& st = State();
    for (int i = 0; i < static_cast<int>(st.books.size()); ++i) {
        if (st.books[static_cast<size_t>(i)].path == info.path) {
            st.books[static_cast<size_t>(i)] = info;
            // 更新后按最新优先挪到队首
            if (i > 0) {
                reader::BookInfo moved = std::move(st.books[static_cast<size_t>(i)]);
                st.books.erase(st.books.begin() + i);
                st.books.insert(st.books.begin(), std::move(moved));
            }
            // 与磁盘列表对齐缓存，避免进首页又复用开机旧快照
            reader::ReplaceBookLibraryCache(st.books);
            return 0;
        }
    }
    // 新书插入队首（与书库按最新 mtime 排序一致）
    st.books.insert(st.books.begin(), info);
    reader::ReplaceBookLibraryCache(st.books);
    return 0;
}

lv_obj_t* CreateReaderScreen(const reader::BookInfo& info);
lv_obj_t* CreateDetailScreen(int book_index);
lv_obj_t* CreateBookshelfScreen();
lv_obj_t* ResumeReaderScreen();   // 返回栈：关 session 后再开，依 .pos 续读
lv_obj_t* ResumeDetailScreen();   // 返回栈：重建当前书详情
void ReleaseReaderSessionForLeave();
void RenderReadingHome();
void RenderBookshelfPage();
void RequestRenderBookshelfPage();
void RequestShelfCheckMarksPaint();
void RefreshShelfFooterMode();
void ExitShelfMultiMode(bool rebuild);
void EnterShelfMultiModeSelect(int idx);
void PatchShelfVisibleCheckMarks();
void OnShelfMultiCancel(lv_event_t* e);
void OnShelfMultiSelectAll(lv_event_t* e);
void OnShelfMultiRemove(lv_event_t* e);
void ExitFontMultiMode(bool paint);
void EnterFontMultiModeSelect(int idx);
void RefreshFontMultiFooter();
void SyncFontSelectedSize();
bool FontItemSelected(int idx);
void OnSheetFontLongPressed(lv_event_t* e);
void OnFontMultiCancel(lv_event_t* e);
void OnFontMultiSelectAll(lv_event_t* e);
void OnFontMultiRemove(lv_event_t* e);
bool DeleteReadFontFile(const std::string& file, std::string& err_out);
void TtfScanFiles();
void TtfEnsurePanel();
void TtfRebuildContent();
void TtfPanelClose();
void TtfStartConvert();
void StopTtfPollTimer();
void OnFontImportClick(lv_event_t* e);
void OnTtfRowClick(lv_event_t* e);
void OnTtfPagePrev(lv_event_t* e);
void OnTtfPageNext(lv_event_t* e);
void OnTtfConfirmYes(lv_event_t* e);
void OnTtfConfirmNo(lv_event_t* e);
void OnTtfClose(lv_event_t* e);
void OnTtfResultOk(lv_event_t* e);
void OnTtfSizeDec(lv_event_t* e);
void OnTtfSizeInc(lv_event_t* e);
void ClampTtfSizePx();
void RefreshTtfConfirmTexts();
void TtfPollTimerCb(lv_timer_t* t);
void RenderReaderPage();
void RenderTocList();
void OnReaderUnderlineDraw(lv_event_t* e);
void AttachReaderUnderline(lv_obj_t* label, int mode);
void ShowOpenProgress(int percent);
void StartOpenWorker(int book_index);
void CancelDetailCoverLoad();
void CancelListCoverFill();
void CancelPageImageLoad();
void StartPageImageWorker();
void EnqueueListCoverFill(const reader::BookInfo& info, int max_w, int max_h, lv_obj_t* host,
                          reader::RasterImage* slot);
void ScheduleListCoverFill();
void FinishDeferredCleanup();
void HideReadChrome();
void ShowReadSettingsSheet();
void OpenReadToc();
void RebuildSettingsSheet();
void ClearSettingsSheetWidgetRefs();
void SetSheetBtnEnabled(lv_obj_t* btn, bool enabled);
void UpdateSheetBoundTip(lv_obj_t* tip, bool can_dec, bool can_inc);
lv_obj_t* MakeSheetIconBtn(lv_obj_t* parent, const char* txt, lv_event_cb_t cb, bool enabled);
void RefreshSettingsSheetFontList();
void RefreshSettingsSheetAdj();
void RefreshSettingsSheet();
void RequestSettingsSheetPaint();
void EnsureSettingsSheetAfterLayout();
bool SettingsSheetWidgetsReady();
void ScanReadFonts();
void EnsureFontListPageShowsSelection();
int CurrentFontIndex();
int FontListPageCount();
void ClampFontListPage();
void FormatFontListName(const std::string& file, char* out, size_t out_len);
void ReopenReaderAfterPrefsChange();
void ApplyReaderLayoutLive(bool reload_font);
void ScheduleLayoutApply(bool reload_font);
void FlushLayoutDebounce();
void CancelLayoutDebounce();
void StartLayoutWorker();
void StartChapterPageWorker();
void BindSessionLayoutPrefs(reader::BookSession& session);
void ApplyReaderPageGeometry(ReaderGeomMode mode);
bool IsReadOverlayChrome(BookUiState::ReadChrome c);
lv_coord_t ReadContentWidth();
void UpdateReadFooter();
void EnsureLayoutHintTimer();
void StopLayoutHintTimer();
void MarkLayoutHintBusy();
void MarkLayoutHintDone();
bool IsLayoutHintBusy();
void OnReadContentClicked(lv_event_t* e);
void OnReadContentLongPressed(lv_event_t* e);
void OnSheetTocClicked(lv_event_t* e);
void OnSheetFontSelect(lv_event_t* e);
void OnSheetFontPagePrev(lv_event_t* e);
void OnSheetFontPageNext(lv_event_t* e);
void OnSheetMarginDec(lv_event_t* e);
void OnSheetMarginInc(lv_event_t* e);
void OnSheetGapDec(lv_event_t* e);
void OnSheetGapInc(lv_event_t* e);
void OnSheetProgShow(lv_event_t* e);
void OnSheetProgHide(lv_event_t* e);
void OnTocRowClicked(lv_event_t* e);
void AttachReadBodyInput(lv_obj_t* obj);
bool BookLvAsync(void (*cb)(void*), void* user_data = nullptr);
void RequestRenderReaderPage();
void RequestRenderBookshelfPage();
void RequestRenderTocList();
void ApplyReaderPageDelta();
bool QueueReaderPageTurn(int dir);
bool ReadEventPoint(lv_event_t* e, lv_point_t* out);

// 书库/详情/正文共用：仅 BOOT 长按进百问；盖板 prev/next 长按连翻（vk_home 长按不进百问）
VkKeyScreenDesc BookAiLongPressDesc(ScreenFactory factory) {
    return VkKeyScreenDesc{factory, BookScreen::OnVkKey, nullptr, BookScreen::OnBootLongPress,
                           nullptr, nullptr, BookScreen::OnVkKeyLongPress,
                           BookScreen::OnVkKeyPressUp};
}

void ResetReadTouch() {
    State().read_touch.suppress_click = false;
}

bool ReadEventPoint(lv_event_t* e, lv_point_t* out) {
    if (out == nullptr) {
        return false;
    }
    lv_indev_t* indev = e != nullptr ? lv_event_get_indev(e) : nullptr;
    if (indev == nullptr) {
        indev = lv_indev_active();
    }
    if (indev == nullptr || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) {
        return false;
    }
    lv_indev_get_point(indev, out);
    return true;
}

void DetachReadBodyInput(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    lv_obj_remove_event_cb(obj, OnReadContentClicked);
    lv_obj_remove_event_cb(obj, OnReadContentLongPressed);
    // 正文整页热区：进目录/设置后须摘掉，否则空白处仍早震
    HapticDetachClick(obj);
}

void AttachReadBodyInput(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    DetachReadBodyInput(obj);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    // 页内热区：touch_feed 按下早震；业务回调里勿再 Pulse
    HapticAttachClick(obj);
    lv_obj_add_event_cb(obj, OnReadContentClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(obj, OnReadContentLongPressed, LV_EVENT_LONG_PRESSED, nullptr);
}

void ApplyReaderPageDelta() {
    auto& st = State();
    if (!st.session || !st.session->IsOpen()) {
        s_reader_page_delta.store(0, std::memory_order_release);
        return;
    }
    // 一次吃光积压：长按按档累加，每帧画到「当前指到的页」（墨水慢则中间页跳过）
    int d = s_reader_page_delta.exchange(0, std::memory_order_acq_rel);
    while (d > 0) {
        if (!st.session->NextPage()) {
            break;
        }
        --d;
    }
    while (d < 0) {
        if (!st.session->PrevPage()) {
            break;
        }
        ++d;
    }
}

/** 盖板键/长按定时器：只累加 delta，真正翻页在 RenderReaderPage（LVGL） */
bool QueueReaderPageTurn(int dir) {
    auto& st = State();
    if (dir == 0 || !st.session || !st.session->IsOpen()) {
        return false;
    }
    const int pending = s_reader_page_delta.load(std::memory_order_acquire);
    if (dir > 0) {
        if (pending <= 0 && !st.session->HasNextPage()) {
            return false;
        }
    } else if (pending >= 0 && !st.session->HasPrevPage()) {
        return false;
    }
    s_reader_page_delta.fetch_add(dir, std::memory_order_acq_rel);
    // 上屏经 ScreenPaintCoalesce：有 pending 则等交完，连点跳到最终页再画
    RequestRenderReaderPage();
    return true;
}

void GoHomeFromBookAsync(void* /*user_data*/) {
    // 勿在此 Reset detail_cover：旧详情屏可能仍引用 dsc，等 DELETE 回调再清
    CancelDetailCoverLoad();
    ReleaseReaderSessionForLeave();
    ScreenRequestBack();
}

void RequestBackHome() {
    BookLvAsync(GoHomeFromBookAsync);
}

void FinishDeferredCleanup() {
    auto& st = State();
    if (!st.deferred_cleanup) {
        return;
    }
    // worker 仍可能握着 session/font_ 计宽：busy 未清不得释字体
    if (st.opening.load() || st.layout_busy.load() || st.chapter_pages_busy.load() ||
        st.page_image_busy.load()) {
        return;
    }
    st.deferred_cleanup = false;
    EndReadingTimeTracking();
    st.session.reset();
    ReleaseBookFont();
}

bool ReaderWorkersBusy() {
    auto& st = State();
    return st.opening.load() || st.layout_busy.load() || st.chapter_pages_busy.load() ||
           st.page_image_busy.load();
}

// 离开正文（进百问等）：落盘 .pos、释放 session/字库；打开中则延后清。
void ReleaseReaderSessionForLeave() {
    auto& st = State();
    st.open_token.fetch_add(1);
    st.read_scr = nullptr;
    st.content = nullptr;
    st.page_label = nullptr;
    st.title_label = nullptr;
    st.open_bar = nullptr;
    st.open_pct_lbl = nullptr;
    st.status_bar = nullptr;
    st.status_overlay = nullptr;
    st.status_label = nullptr;
    st.status_h = 0;
    st.settings_sheet = nullptr;
    ClearSettingsSheetWidgetRefs();
    StopTtfPollTimer();
    st.ttf_mode = BookUiState::TtfMode::kNone;
    st.ttf_pick.clear();
    st.ttf_page = 0;
    st.ttf_size_px = kTtfConvertSizeDefault;
    st.viewport_w = 0;
    st.viewport_h = 0;
    st.font_entries.clear();
    st.font_list_page = 0;
    st.font_multi = false;
    st.font_selected.clear();
    st.font_suppress_click_until_us = 0;
    st.font_suppress_click_idx = -1;
    st.font_suppress_next_click = false;
    st.read_chrome = BookUiState::ReadChrome::kReading;
    st.settings_resume_after_open = false;
    st.toc_list_page = 0;
    const bool layout_running = st.layout_busy.load();
    const bool chapter_pages_running = st.chapter_pages_busy.load();
    const bool page_image_running = st.page_image_busy.load();
    st.layout_token.fetch_add(1);
    st.layout_again = false;
    st.layout_reload_font = false;
    st.layout_anchor = {};
    st.chapter_pages_token.fetch_add(1);
    st.chapter_pages_again = false;
    CancelPageImageLoad();
    if (st.session) {
        st.session->InvalidateChapterPageTable();
        st.session->AbortTxtPaginate();
    }
    CancelLayoutDebounce();
    StopLayoutHintTimer();
    st.layout_hint_done_until_us = 0;
    ScreenPaintCoalesceReset(&s_reader_paint);
    ScreenPaintCoalesceReset(&s_toc_paint);
    ScreenPaintCoalesceReset(&s_settings_sheet_paint);
    s_reader_page_delta.store(0, std::memory_order_release);
    ResetReadTouch();
    if (st.opening.load() || layout_running || chapter_pages_running || page_image_running) {
        // 勿把 *_busy 置 false，否则其它路径会误 ReleaseBookFont 与计页抢字体
        StopReadTimeCheckpointTimer();
        st.deferred_cleanup = true;
    } else {
        EndReadingTimeTracking();
        st.session.reset();
        ReleaseBookFont();
    }
}

lv_obj_t* ResumeReaderScreen() {
    auto& st = State();
    // 解析尚未结束 / 后台计页未结束：退回书库，避免与 worker 抢 session/font
    if (ReaderWorkersBusy()) {
        return BookScreen::Create();
    }
    if (st.deferred_cleanup) {
        FinishDeferredCleanup();
    } else if (st.session) {
        EndReadingTimeTracking();
        st.session.reset();
        ReleaseBookFont();
    }
    if (st.selected < 0 || st.selected >= static_cast<int>(st.books.size())) {
        return BookScreen::Create();
    }
    return CreateReaderScreen(st.books[static_cast<size_t>(st.selected)]);
}

lv_obj_t* ResumeDetailScreen() {
    auto& st = State();
    if (st.selected < 0 || st.selected >= static_cast<int>(st.books.size())) {
        return BookScreen::Create();
    }
    return CreateDetailScreen(st.selected);
}

void OpenAssistantFromReaderAsync(void* /*user_data*/) {
    ReleaseReaderSessionForLeave();
    if (AssistantScreen::IsActive()) {
        ESP_LOGI(TAG, "assistant from reader: already active");
        return;
    }
    HapticPulseIfEnabled();
    ESP_LOGI(TAG, "assistant from reader -> ScreenNavigateTo (resume via .pos)");
    ScreenNavigateTo(AssistantScreen::Create);
}

void RequestOpenAssistantFromReader() {
    if (!esp_lv_adapter_is_initialized()) {
        return;
    }
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGW(TAG, "assistant from reader: adapter lock failed");
        return;
    }
    if (lv_async_call(OpenAssistantFromReaderAsync, nullptr) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "assistant from reader: lv_async_call failed");
    }
    esp_lv_adapter_unlock();
}

/** 盖板键在 touch_feed：须 ScreenLvAsync，禁止同步 lv_obj_clean/create（否则 tlsf 双释放）。 */
bool BookLvAsync(void (*cb)(void*), void* user_data) {
    const int64_t t0 = esp_timer_get_time();
    const bool ok = ScreenLvAsync(cb, user_data);
    ESP_LOGI(TAG, "BookLvAsync queue %s in %d us", ok ? "ok" : "fail",
             static_cast<int>(esp_timer_get_time() - t0));
    return ok;
}

void PaintBookshelfPage() {
    ESP_LOGI(TAG, "async RenderBookshelfPage page=%d begin", State().list_page);
    RenderBookshelfPage();
    ESP_LOGI(TAG, "async RenderBookshelfPage done");
}

void PaintReaderPage() {
    ESP_LOGI(TAG, "async RenderReaderPage begin");
    RenderReaderPage();
    ESP_LOGI(TAG, "async RenderReaderPage done");
}

void PaintTocList() {
    ESP_LOGI(TAG, "async RenderTocList page=%d begin", State().toc_list_page);
    RenderTocList();
    ESP_LOGI(TAG, "async RenderTocList done");
}

void EnsureBookPaintFns() {
    if (s_library_paint.paint == nullptr) {
        s_library_paint.paint = PaintBookshelfPage;
    }
    if (s_reader_paint.paint == nullptr) {
        s_reader_paint.paint = PaintReaderPage;
    }
    if (s_toc_paint.paint == nullptr) {
        s_toc_paint.paint = PaintTocList;
    }
    if (s_settings_sheet_paint.paint == nullptr) {
        s_settings_sheet_paint.paint = RefreshSettingsSheet;
    }
}

/** 页码已同步改完后调用：序号合并，积压回调跳过已画过的序号。 */
void RequestRenderBookshelfPage() {
    EnsureBookPaintFns();
    ScreenPaintCoalesceRequest(&s_library_paint);
}

void RequestShelfCheckMarksPaint() {
    if (s_shelf_check_paint.paint == nullptr) {
        s_shelf_check_paint.paint = PatchShelfVisibleCheckMarks;
    }
    ScreenPaintCoalesceRequestDebounced(&s_shelf_check_paint, kShelfCheckPaintDebounceUs);
}

void RequestRenderReaderPage() {
    EnsureBookPaintFns();
    ScreenPaintCoalesceRequest(&s_reader_paint);
}

void RequestRenderTocList() {
    EnsureBookPaintFns();
    ScreenPaintCoalesceRequest(&s_toc_paint);
}

/** 设置卡状态已改完：与正文翻页同形，按下中只记序号，抬起后一次刷最终态 */
void RequestSettingsSheetPaint() {
    EnsureBookPaintFns();
    ScreenPaintCoalesceRequest(&s_settings_sheet_paint);
}

void AsyncHideReadChrome(void* /*user_data*/) {
    ESP_LOGI(TAG, "async HideReadChrome begin");
    HideReadChrome();
    ESP_LOGI(TAG, "async HideReadChrome done");
}

void ClearDetailCoverUiPtrs() {
    auto& st = State();
    st.detail_cover_host = nullptr;
    st.detail_title_lbl = nullptr;
    st.detail_meta_lbl = nullptr;
}

/** 作废进行中的文件内封面补全；须在 LVGL 任务调用。 */
void CancelDetailCoverLoad() {
    auto& st = State();
    st.detail_cover_token.fetch_add(1);
    ClearDetailCoverUiPtrs();
}

/** 作废列表/目录封面补全；须在清槽/离页前调用。 */
void CancelListCoverFill() {
    auto& st = State();
    st.list_cover_token.fetch_add(1);
    st.list_cover_pending.clear();
}

/** 作废正文插图后台解码；翻页 clean 前调用。 */
void CancelPageImageLoad() {
    auto& st = State();
    st.page_image_abort.store(true, std::memory_order_relaxed);
    st.page_image_token.fetch_add(1);
    st.page_image_pending.clear();
    st.page_image_again = false;
}

void AttachPageImageToSlot(lv_obj_t* slot, reader::RasterImage* heap_img, bool page_images) {
    if (slot == nullptr || heap_img == nullptr || !lv_obj_is_valid(slot)) {
        delete heap_img;
        return;
    }
    lv_obj_clean(slot);
    heap_img->BindDsc();
    lv_obj_t* image = lv_image_create(slot);
    lv_image_set_src(image, &heap_img->dsc);
    lv_obj_set_user_data(image, heap_img);
    lv_obj_add_event_cb(
        image,
        [](lv_event_t* ev) {
            auto* p = static_cast<reader::RasterImage*>(
                lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(ev))));
            delete p;
        },
        LV_EVENT_DELETE, nullptr);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    if (page_images) {
        lv_obj_align(image, LV_ALIGN_TOP_LEFT, 0, 0);
    }
}

void AsyncPageImageDone(void* user_data) {
    auto* msg = static_cast<PageImageDoneMsg*>(user_data);
    auto& st = State();
    const bool mine = (msg->token == st.page_image_token.load());
    if (!mine || msg->slot == nullptr || !lv_obj_is_valid(msg->slot) || !msg->ok ||
        msg->image == nullptr || msg->image->empty()) {
        delete msg->image;
        delete msg;
        return;
    }
    if (st.session && !msg->href.empty()) {
        reader::RasterImage cache_copy;
        cache_copy.pixels = msg->image->pixels;
        cache_copy.width = msg->image->width;
        cache_copy.height = msg->image->height;
        cache_copy.BindDsc();
        st.session->SetCachedPageImage(msg->href, std::move(cache_copy));
    }
    AttachPageImageToSlot(msg->slot, msg->image, msg->page_images);
    msg->image = nullptr;
    delete msg;
}

void PageImageWorker(void* arg) {
    auto* ctx = static_cast<PageImageWorkerArg*>(arg);
    auto& st = State();

    // EPUB：独立打开 ZIP，避免长时间占用 session 的 ZipReader 锁堵翻页
    std::unique_ptr<reader::EpubDocument> epub_doc;
    if (ctx->format == reader::BookFormat::kEpub && !ctx->book_path.empty()) {
        epub_doc = std::make_unique<reader::EpubDocument>();
        if (!epub_doc->Open(ctx->book_path.c_str())) {
            ESP_LOGW(TAG, "book_pimg epub open fail");
            epub_doc.reset();
        }
    }

    for (const auto& item : ctx->items) {
        if (ctx->token != st.page_image_token.load()) {
            break;
        }
        auto* img = new reader::RasterImage();
        bool ok = false;
        if (epub_doc) {
            ok = epub_doc->DecodeItemImageToL8(item.href.c_str(), item.max_w, item.max_h, *img,
                                               &st.page_image_abort) &&
                 !img->empty();
        } else if (st.session) {
            ok = st.session->LoadPageImage(item.href, item.max_w, item.max_h, *img) && !img->empty();
        }
        if (st.page_image_abort.load(std::memory_order_relaxed) ||
            ctx->token != st.page_image_token.load()) {
            delete img;
            break;
        }
        auto* done = new PageImageDoneMsg{};
        done->token = ctx->token;
        done->slot = item.slot;
        done->href = item.href;
        done->ok = ok;
        done->page_images = ctx->page_images;
        if (ok) {
            done->image = img;
        } else {
            delete img;
            done->image = nullptr;
        }
        if (!ScreenLvAsync(AsyncPageImageDone, done)) {
            delete done->image;
            delete done;
        }
    }
    if (epub_doc) {
        epub_doc->Close();
        epub_doc.reset();
    }

    auto* finish = new uint32_t(ctx->token);
    if (!ScreenLvAsync(
            [](void* p) {
                auto* tok = static_cast<uint32_t*>(p);
                auto& st = State();
                st.page_image_busy.store(false);
                st.page_image_again = false;
                (void)tok;
                if (st.deferred_cleanup && !ReaderWorkersBusy()) {
                    FinishDeferredCleanup();
                } else if (!st.page_image_pending.empty()) {
                    StartPageImageWorker();
                }
                delete tok;
            },
            finish)) {
        st.page_image_busy.store(false);
        delete finish;
    }
    delete ctx;
    vTaskDelete(nullptr);
}

void StartPageImageWorker() {
    auto& st = State();
    if (st.page_image_pending.empty() || !st.session) {
        return;
    }
    if (st.page_image_busy.load()) {
        st.page_image_again = true;
        return;
    }
    const uint32_t token = st.page_image_token.load();
    auto* ctx = new PageImageWorkerArg{};
    ctx->token = token;
    ctx->book_path = st.session->Info().path;
    ctx->format = st.session->Info().format;
    ctx->page_images = st.session->IsPageImagesMode();
    ctx->items.reserve(st.page_image_pending.size());
    for (auto& p : st.page_image_pending) {
        PageImageWorkItem item;
        item.href = std::move(p.href);
        item.max_w = p.max_w;
        item.max_h = p.max_h;
        item.slot = p.slot;
        ctx->items.push_back(std::move(item));
    }
    st.page_image_pending.clear();
    st.page_image_abort.store(false, std::memory_order_relaxed);
    st.page_image_busy.store(true);
    st.page_image_again = false;
    BaseType_t created = xTaskCreatePinnedToCoreWithCaps(
        PageImageWorker, "book_pimg", kPageImageWorkerStack, ctx, tskIDLE_PRIORITY + 1, nullptr, 0,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        ESP_LOGW(TAG, "book_pimg task failed");
        st.page_image_busy.store(false);
        delete ctx;
    }
}

bool IsCoverEmbedMiss(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    const auto& miss = State().cover_embed_miss;
    return std::find(miss.begin(), miss.end(), path) != miss.end();
}

void RememberCoverEmbedMiss(const std::string& path) {
    if (path.empty() || IsCoverEmbedMiss(path)) {
        return;
    }
    State().cover_embed_miss.push_back(path);
}

void AsyncRememberCoverMiss(void* user_data) {
    auto* msg = static_cast<CoverMissMsg*>(user_data);
    RememberCoverEmbedMiss(msg->path);
    delete msg;
}

void PostCoverEmbedMiss(const std::string& path) {
    if (path.empty()) {
        return;
    }
    auto* msg = new CoverMissMsg{};
    msg->path = path;
    if (!ScreenLvAsync(AsyncRememberCoverMiss, msg)) {
        delete msg;
    }
}

void EnqueueListCoverFill(const reader::BookInfo& info, int max_w, int max_h, lv_obj_t* host,
                          reader::RasterImage* slot) {
    if (host == nullptr || slot == nullptr || max_w <= 0 || max_h <= 0 || info.path.empty()) {
        return;
    }
    if (info.format != reader::BookFormat::kEpub && info.format != reader::BookFormat::kEbook) {
        return;
    }
    if (IsCoverEmbedMiss(info.path)) {
        return;
    }
    auto& st = State();
    BookUiState::ListCoverPending p;
    p.info = info;
    p.max_w = max_w;
    p.max_h = max_h;
    p.host = host;
    p.slot = slot;
    st.list_cover_pending.push_back(std::move(p));
}

void ApplyListCoverImage(lv_obj_t* host, reader::RasterImage& cover) {
    if (host == nullptr || !lv_obj_is_valid(host) || cover.empty()) {
        return;
    }
    cover.BindDsc();
    lv_obj_clean(host);
    lv_obj_t* img = lv_image_create(host);
    lv_image_set_src(img, &cover.dsc);
    lv_obj_center(img);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
}

void AsyncListCoverDone(void* user_data) {
    auto* msg = static_cast<ListCoverDoneMsg*>(user_data);
    auto& st = State();
    if (!msg->cover_ok) {
        RememberCoverEmbedMiss(msg->path);
    }
    if (msg->token != st.list_cover_token.load() || msg->cover == nullptr) {
        delete msg->cover;
        delete msg;
        return;
    }
    if (msg->host == nullptr || !lv_obj_is_valid(msg->host) || msg->slot == nullptr) {
        delete msg->cover;
        delete msg;
        return;
    }
    if (msg->cover_ok && !msg->cover->empty()) {
        *msg->slot = std::move(*msg->cover);
        ApplyListCoverImage(msg->host, *msg->slot);
    }
    delete msg->cover;
    delete msg;
}

void ListCoverFillWorker(void* arg) {
    auto* work = static_cast<ListCoverFillWork*>(arg);
    auto& st = State();
    for (size_t i = 0; i < work->items.size(); ++i) {
        if (work->token != st.list_cover_token.load()) {
            break;
        }
        const BookUiState::ListCoverPending& item = work->items[i];
        auto* cover = new reader::RasterImage();
        reader::BookInfo info = item.info;
        const bool ok = book_ui::LoadBookDetailCover(info, item.max_w, item.max_h, *cover) &&
                        !cover->empty();
        auto* done = new ListCoverDoneMsg{};
        done->token = work->token;
        done->host = item.host;
        done->slot = item.slot;
        done->path = item.info.path;
        done->cover_ok = ok;
        if (ok) {
            done->cover = cover;
        } else {
            delete cover;
            done->cover = nullptr;
        }
        if (!ScreenLvAsync(AsyncListCoverDone, done)) {
            if (!ok) {
                PostCoverEmbedMiss(item.info.path);
            }
            delete done->cover;
            delete done;
        }
        // 串行抽封面，让出 CPU，避免列表滑动/点击发闷
        vTaskDelay(pdMS_TO_TICKS(ok ? 15 : 5));
    }
    delete work;
    vTaskDelete(nullptr);
}

void ScheduleListCoverFill() {
    auto& st = State();
    if (st.list_cover_pending.empty()) {
        return;
    }
    ESP_LOGI(TAG, "list_cover fill n=%u", static_cast<unsigned>(st.list_cover_pending.size()));
    auto* work = new ListCoverFillWork{};
    work->token = st.list_cover_token.load();
    work->items = std::move(st.list_cover_pending);
    st.list_cover_pending.clear();
    BaseType_t created = xTaskCreatePinnedToCoreWithCaps(
        ListCoverFillWorker, "list_cover", kListCoverWorkerStack, work, tskIDLE_PRIORITY + 1,
        nullptr, 0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        ESP_LOGW(TAG, "list_cover task failed; keep placeholders");
        delete work;
    }
}

void FillDetailCoverPlaceholder(lv_obj_t* host, reader::BookFormat format) {
    auto& st = State();
    if (host == nullptr) {
        return;
    }
    lv_obj_clean(host);
    lv_obj_t* ph = lv_obj_create(host);
    lv_obj_remove_style_all(ph);
    lv_obj_set_size(ph, st.detail_cover_fw, st.detail_cover_fh);
    lv_obj_set_style_pad_all(ph, kCoverFrameBorder, 0);
    lv_obj_set_style_bg_color(ph, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(ph, LV_OPA_COVER, 0);
    StyleBookCoverFrame(ph);
    lv_obj_t* ph_lbl = lv_label_create(ph);
    lv_label_set_text(ph_lbl, reader::FormatLabel(format));
    lv_obj_set_style_text_font(ph_lbl, ListFont(), 0);
    lv_obj_center(ph_lbl);
    DisableScroll(ph);
}

void ApplyDetailCoverImage() {
    auto& st = State();
    if (st.detail_cover_host == nullptr || !lv_obj_is_valid(st.detail_cover_host) ||
        st.detail_cover.empty()) {
        return;
    }
    st.detail_cover.BindDsc();
    lv_obj_clean(st.detail_cover_host);
    lv_obj_t* frame = lv_obj_create(st.detail_cover_host);
    lv_obj_remove_style_all(frame);
    lv_obj_set_size(frame, st.detail_cover_fw, st.detail_cover_fh);
    lv_obj_set_style_pad_all(frame, kCoverFrameBorder, 0);
    lv_obj_set_style_bg_color(frame, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    StyleBookCoverFrame(frame);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(frame);
    lv_obj_t* img = lv_image_create(frame);
    lv_image_set_src(img, &st.detail_cover.dsc);
    lv_obj_center(img);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
}

void UpdateDetailMetaLabels(const reader::BookInfo& shown) {
    auto& st = State();
    if (st.detail_title_lbl != nullptr && lv_obj_is_valid(st.detail_title_lbl)) {
        lv_label_set_text(st.detail_title_lbl, shown.title.c_str());
    }
    if (st.detail_meta_lbl != nullptr && lv_obj_is_valid(st.detail_meta_lbl)) {
        // 详情页 meta 槽改为作者行；无作者显示「未知」，保持单行槽高度
        lv_label_set_text(st.detail_meta_lbl, shown.author.empty() ? Lang::Strings::COMMON_UNKNOWN : shown.author.c_str());
    }
}

void AsyncDetailCoverDone(void* user_data) {
    auto* msg = static_cast<DetailCoverDoneMsg*>(user_data);
    auto& st = State();

    if (!msg->cover_ok) {
        RememberCoverEmbedMiss(msg->path);
    }

    if (msg->token != st.detail_cover_token.load()) {
        delete msg->cover;
        msg->cover = nullptr;
        delete msg;
        return;
    }

    const bool ui_gone = st.detail_scr == nullptr || !lv_obj_is_valid(st.detail_scr) ||
                         st.detail_cover_host == nullptr || !lv_obj_is_valid(st.detail_cover_host);
    if (ui_gone) {
        delete msg->cover;
        msg->cover = nullptr;
        delete msg;
        return;
    }

    if (st.selected >= 0 && st.selected < static_cast<int>(st.books.size()) &&
        st.books[static_cast<size_t>(st.selected)].path == msg->path) {
        if (!msg->title.empty()) {
            st.books[static_cast<size_t>(st.selected)].title = msg->title;
        }
        st.books[static_cast<size_t>(st.selected)].author = msg->author;
        UpdateDetailMetaLabels(st.books[static_cast<size_t>(st.selected)]);
    }

    if (msg->cover != nullptr && msg->cover_ok && !msg->cover->empty()) {
        st.detail_cover = std::move(*msg->cover);
        delete msg->cover;
        msg->cover = nullptr;
        ApplyDetailCoverImage();
    } else {
        delete msg->cover;
        msg->cover = nullptr;
    }
    delete msg;
}

void DetailCoverWorker(void* arg) {
    auto* ctx = static_cast<DetailCoverWorkerArg*>(arg);

    auto* cover = new reader::RasterImage();
    reader::BookInfo info = ctx->info;
    // 始终抽一次；token 只决定是否刷 UI（取消后仍记 miss，避免回书架再抽）
    const bool ok = book_ui::LoadBookDetailCover(info, ctx->max_w, ctx->max_h, *cover) &&
                    !cover->empty();

    auto* done = new DetailCoverDoneMsg{};
    done->token = ctx->token;
    done->path = ctx->info.path;
    done->title = info.title;
    done->author = info.author;
    done->cover_ok = ok;
    if (ok) {
        done->cover = cover;
    } else {
        delete cover;
        done->cover = nullptr;
    }

    if (!ScreenLvAsync(AsyncDetailCoverDone, done)) {
        if (!ok) {
            PostCoverEmbedMiss(ctx->info.path);
        }
        delete done->cover;
        delete done;
    }
    delete ctx;
    vTaskDelete(nullptr);
}

/** 无旁路时后台读文件内封面；有旁路则 CreateDetailScreen 已同步显示。 */
void StartDetailCoverWorker(int book_index) {
    auto& st = State();
    if (book_index < 0 || book_index >= static_cast<int>(st.books.size())) {
        return;
    }
    const uint32_t token = st.detail_cover_token.fetch_add(1) + 1;
    st.detail_format = st.books[static_cast<size_t>(book_index)].format;

    auto* ctx = new DetailCoverWorkerArg{};
    ctx->token = token;
    ctx->info = st.books[static_cast<size_t>(book_index)];
    ctx->max_w = CoverFrameInner(st.detail_cover_fw);
    ctx->max_h = CoverFrameInner(st.detail_cover_fh);
    BaseType_t created = xTaskCreatePinnedToCoreWithCaps(
        DetailCoverWorker, "book_cover", kDetailCoverWorkerStack, ctx, tskIDLE_PRIORITY + 2, nullptr,
        0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        ESP_LOGW(TAG, "book_cover task failed; keep placeholder");
        delete ctx;
        st.detail_cover_token.fetch_add(1);
    }
}

void ShowOpenProgress(int percent) {
    auto& st = State();
    if (st.content == nullptr) {
        return;
    }
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }

    const bool need_build =
        st.open_bar == nullptr || lv_obj_get_parent(st.open_bar) != st.content ||
        st.open_pct_lbl == nullptr || lv_obj_get_parent(st.open_pct_lbl) != st.content;

    if (need_build) {
        st.open_bar = nullptr;
        st.open_pct_lbl = nullptr;
        lv_obj_clean(st.content);
        // 打开进度 UI 用书库边距；避免阅读页边距档位导致 ContentWidth 溢出
        ApplyOverlayListContentPads(0);
        lv_obj_set_style_layout(st.content, LV_LAYOUT_NONE, 0);
        DisableScroll(st.content);

        lv_obj_t* panel = lv_obj_create(st.content);
        lv_obj_remove_style_all(panel);
        lv_obj_set_size(panel, ContentWidth(), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(panel, 18, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(panel);
        lv_obj_center(panel);

        lv_obj_t* title = lv_label_create(panel);
        lv_label_set_text(title, Lang::Strings::BOOK_OPENING);
        lv_obj_set_style_text_font(title, ListFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_black(), 0);
        lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

        // 轨道：白底黑框；指示器：实心黑块（墨水屏清晰）
        lv_coord_t bar_w = kOpenBarW;
        if (bar_w > ContentWidth() - 8) {
            bar_w = ContentWidth() - 8;
        }
        lv_obj_t* bar = lv_bar_create(panel);
        lv_obj_set_size(bar, bar_w, kOpenBarH);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_mode(bar, LV_BAR_MODE_NORMAL);
        lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(bar, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_pad_all(bar, 3, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(bar, lv_color_black(), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        st.open_bar = bar;

        lv_obj_t* pct = lv_label_create(panel);
        lv_obj_set_style_text_font(pct, ListFont(), 0);
        lv_obj_set_style_text_color(pct, lv_color_black(), 0);
        lv_obj_set_style_text_align(pct, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_clear_flag(pct, LV_OBJ_FLAG_CLICKABLE);
        st.open_pct_lbl = pct;
    }

    lv_bar_set_value(st.open_bar, percent, LV_ANIM_OFF);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d%%", percent);
    lv_label_set_text(st.open_pct_lbl, buf);
}

void AsyncOpenProgress(void* user_data) {
    auto* msg = static_cast<OpenProgressMsg*>(user_data);
    auto& st = State();
    if (msg->token == st.open_token.load() && st.content != nullptr && st.opening.load()) {
        ShowOpenProgress(msg->percent);
    }
    delete msg;
}

void AsyncOpenDone(void* user_data) {
    auto* msg = static_cast<OpenDoneMsg*>(user_data);
    auto& st = State();
    st.opening.store(false);

    if (msg->token != st.open_token.load() || st.deferred_cleanup || st.content == nullptr) {
        FinishDeferredCleanup();
        delete msg;
        return;
    }

    if (!msg->ok || !st.session || !st.session->IsOpen()) {
        EndReadingTimeTracking();
        ReleaseBookFont();
        if (st.opening_format == reader::BookFormat::kTxt) {
            ShowMessage(st.content, Lang::Strings::BOOK_OPEN_FAILED_TXT);
        } else if (st.opening_format == reader::BookFormat::kEbook) {
            ShowMessage(st.content, Lang::Strings::BOOK_OPEN_FAILED_CHAPTER);
        } else {
            ShowMessage(st.content, Lang::Strings::BOOK_OPEN_FAILED_EPUB);
        }
        delete msg;
        return;
    }

    // 文档元数据回写书库，下次详情秒开可见 OPF 书名/作者
    if (st.selected >= 0 && st.selected < static_cast<int>(st.books.size()) &&
        st.books[static_cast<size_t>(st.selected)].path == st.session->Info().path) {
        auto& bi = st.books[static_cast<size_t>(st.selected)];
        if (!st.session->Title().empty()) {
            bi.title = st.session->Title();
        }
        if (!st.session->Author().empty()) {
            bi.author = st.session->Author();
        }
    }

    if (st.settings_resume_after_open) {
        st.settings_resume_after_open = false;
        st.read_chrome = BookUiState::ReadChrome::kReading;
        RenderReaderPage();
        ShowReadSettingsSheet();
        BeginReadingTimeTracking();
        if (st.session->NeedsChapterPageRebuild()) {
            StartChapterPageWorker();
        }
        delete msg;
        return;
    }

    RenderReaderPage();
    BeginReadingTimeTracking();
    if (st.session->NeedsChapterPageRebuild()) {
        StartChapterPageWorker();
    }
    delete msg;
}

void OnOpenProgress(int percent, void* user) {
    auto* msg = new OpenProgressMsg{};
    msg->token = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(user));
    msg->percent = percent;
    // OpenBookWorker 非 LVGL：须 ScreenLvAsync，裸 lv_async_call 会搞坏链表 → Cache/MMU
    if (!ScreenLvAsync(AsyncOpenProgress, msg)) {
        delete msg;
    }
}

void OpenBookWorker(void* arg) {
    auto* ctx = static_cast<OpenWorkerArg*>(arg);
    auto& st = State();
    bool ok = false;

    const uint32_t token_now = st.open_token.load();
    ESP_LOGI(TAG, "book_open worker start token=%u cur=%u fmt=%d int=%u spiram=%u path=%s",
             static_cast<unsigned>(ctx->token), static_cast<unsigned>(token_now),
             static_cast<int>(ctx->info.format),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
             ctx->info.path.c_str());

    if (st.session == nullptr) {
        ESP_LOGE(TAG, "book_open skip: session null");
    } else if (ctx->token != token_now) {
        ESP_LOGW(TAG, "book_open skip: token stale %u!=%u", static_cast<unsigned>(ctx->token),
                 static_cast<unsigned>(token_now));
    } else {
        st.session->SetOpenProgress(OnOpenProgress,
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(ctx->token)));
        ok = st.session->Open(ctx->info);
        st.session->SetOpenProgress(nullptr, nullptr);
        // 打开路径统一补旁路封面，供书架/详情/目录取用（失败不阻断阅读）
        if (ok && ctx->token == st.open_token.load() &&
            (ctx->info.format == reader::BookFormat::kEbook ||
             ctx->info.format == reader::BookFormat::kEpub)) {
            char sidecar[192];
            if (reader::BookCoverSidecarPath(ctx->info.path.c_str(), sidecar, sizeof(sidecar)) &&
                access(sidecar, R_OK) != 0) {
                reader::SaveBookCoverSidecar(ctx->info.path.c_str());
            }
        }
    }

    ESP_LOGI(TAG, "book_open worker done ok=%d int=%u spiram=%u", ok ? 1 : 0,
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));

    auto* done = new OpenDoneMsg{};
    done->token = ctx->token;
    done->ok = ok;
    if (!ScreenLvAsync(AsyncOpenDone, done)) {
        delete done;
        st.opening.store(false);
        // 禁止在 worker 里直接 reset session；标记延后清理
        st.deferred_cleanup = true;
        ESP_LOGW(TAG, "book_open done: ScreenLvAsync fail, deferred_cleanup");
    }
    delete ctx;
    vTaskDelete(nullptr);
}

void StartOpenWorker(int book_index) {
    auto& st = State();
    if (st.opening.load()) {
        ESP_LOGW(TAG, "StartOpenWorker ignored: already opening");
        return;
    }
    if (book_index < 0 || book_index >= static_cast<int>(st.books.size()) || !st.session) {
        ESP_LOGE(TAG, "StartOpenWorker bad state idx=%d books=%u session=%d", book_index,
                 static_cast<unsigned>(st.books.size()), st.session ? 1 : 0);
        ShowMessage(st.content, Lang::Strings::BOOK_OPEN_FAILED);
        return;
    }

    const uint32_t token = st.open_token.fetch_add(1) + 1;
    st.opening.store(true);
    st.deferred_cleanup = false;
    st.opening_format = st.books[static_cast<size_t>(book_index)].format;
    ShowOpenProgress(0);

    auto* ctx = new OpenWorkerArg{};
    ctx->token = token;
    ctx->info = st.books[static_cast<size_t>(book_index)];
    ESP_LOGI(TAG, "StartOpenWorker token=%u fmt=%d int=%u spiram=%u path=%s",
             static_cast<unsigned>(token), static_cast<int>(ctx->info.format),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
             ctx->info.path.c_str());
    BaseType_t created = xTaskCreatePinnedToCoreWithCaps(
        OpenBookWorker, "book_open", kOpenWorkerStack, ctx, tskIDLE_PRIORITY + 2, nullptr, 0,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        const size_t free_int = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGE(TAG, "book_open task failed: int free=%u spiram=%u stack=%u",
                 static_cast<unsigned>(free_int), static_cast<unsigned>(free_spiram),
                 static_cast<unsigned>(kOpenWorkerStack));
        delete ctx;
        st.opening.store(false);
        ShowMessage(st.content, Lang::Strings::BOOK_OPEN_FAILED_TASK);
    }
}

void StartReadAsync(void* user_data) {
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(user_data));
    auto& st = State();
    if (index < 0 || index >= static_cast<int>(st.books.size())) {
        return;
    }
    // 编排/打开 worker 仍握旧 session：勿 CreateReaderScreen 拆掉
    if (ReaderWorkersBusy()) {
        ESP_LOGW(TAG, "start read ignored: worker busy");
        return;
    }
    st.selected = index;
    CancelDetailCoverLoad();
    // 勿在此 Reset detail_cover：旧详情屏异步删除前仍引用封面缓冲
    ScreenLoadReplace(CreateReaderScreen(st.books[static_cast<size_t>(index)]));
}

void BackToLibraryAsync(void* /*user_data*/) {
    // 详情 vk_home / vk_prev → 书架或阅读首页。
    CancelDetailCoverLoad();
    // Create / CreateBookshelf 统一收尾正文 session / SD 字库（含 opening 中途离开的 deferred）。
    // detail_scr / cover 由详情屏 LV_EVENT_DELETE 清理，勿在此 Reset。
    if (State().back_root == BookUiState::NavRoot::kShelf) {
        ScreenLoadReplace(CreateBookshelfScreen());
    } else {
        ScreenLoadReplace(BookScreen::Create());
    }
}

void BackToDetailAsync(void* /*user_data*/) {
    auto& st = State();
    const int index = st.selected;
    // 与进百问同源：open_token 失效 worker 回调；opening 时 deferred_cleanup，避免 UAF/泄漏
    ReleaseReaderSessionForLeave();
    if (index < 0 || index >= static_cast<int>(st.books.size())) {
        ScreenLoadReplace(BookScreen::Create());
        return;
    }
    ScreenLoadReplace(CreateDetailScreen(index));
}

void OpenDetailAsync(void* user_data) {
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(user_data));
    auto& st = State();
    if (index < 0 || index >= static_cast<int>(st.books.size())) {
        return;
    }
    st.selected = index;
    const char* screen = VkKey_ActiveScreen();
    if (screen != nullptr && std::strcmp(screen, kScreenBookshelf) == 0) {
        st.back_root = BookUiState::NavRoot::kShelf;
    } else {
        st.back_root = BookUiState::NavRoot::kHome;
    }
    ScreenLoadReplace(CreateDetailScreen(index));
}

void OpenBookshelfAsync(void* /*user_data*/) {
    State().list_page = 0;
    ScreenLoadReplace(CreateBookshelfScreen());
}

void BackToReadingHomeAsync(void* /*user_data*/) {
    ScreenLoadReplace(BookScreen::Create());
}

void OnContinueReadingClicked(lv_event_t* e) {
    auto& st = State();
    if (ReaderWorkersBusy()) {
        return;
    }
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (index < 0 || index >= static_cast<int>(st.books.size())) {
        return;
    }
    st.back_root = BookUiState::NavRoot::kHome;
    st.selected = index;
    lv_async_call(StartReadAsync, reinterpret_cast<void*>(static_cast<intptr_t>(index)));
}

void OnOpenShelfClicked(lv_event_t* /*e*/) {
    auto& st = State();
    if (st.opening.load()) {
        return;
    }
    lv_async_call(OpenBookshelfAsync, nullptr);
}

void SyncShelfSelectedSize() {
    auto& st = State();
    if (st.shelf_selected.size() != st.books.size()) {
        st.shelf_selected.assign(st.books.size(), 0);
    }
}

int ShelfSelectedCount() {
    SyncShelfSelectedSize();
    auto& st = State();
    int n = 0;
    for (uint8_t v : st.shelf_selected) {
        if (v != 0) {
            ++n;
        }
    }
    return n;
}

bool ShelfItemSelected(int idx) {
    SyncShelfSelectedSize();
    auto& st = State();
    return idx >= 0 && idx < static_cast<int>(st.shelf_selected.size()) &&
           st.shelf_selected[static_cast<size_t>(idx)] != 0;
}

void ToggleShelfItemSelected(int idx) {
    SyncShelfSelectedSize();
    auto& st = State();
    if (idx < 0 || idx >= static_cast<int>(st.shelf_selected.size())) {
        return;
    }
    st.shelf_selected[static_cast<size_t>(idx)] =
        st.shelf_selected[static_cast<size_t>(idx)] ? 0 : 1;
}

void RefreshShelfFooterMode() {
    auto& st = State();
    if (st.shelf_multi) {
        if (st.multi_bar != nullptr) {
            lv_obj_set_width(st.multi_bar, lv_pct(100));
            lv_obj_align(st.multi_bar, LV_ALIGN_CENTER, 0, -3); // 整体居中并上移 6px
            lv_obj_clear_flag(st.multi_bar, LV_OBJ_FLAG_HIDDEN);
        }
        if (st.list_footer != nullptr) {
            lv_obj_set_width(st.list_footer, LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(st.list_footer, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_style_text_font(st.list_footer, ItemFont(), 0);
            lv_obj_align(st.list_footer, LV_ALIGN_RIGHT_MID, -10, 0);
            lv_obj_clear_flag(st.list_footer, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(st.list_footer);
        }
        if (st.status_label != nullptr && lv_obj_is_valid(st.status_label)) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), Lang::Strings::BOOK_SELECTED_FMT, ShelfSelectedCount());
            lv_label_set_text(st.status_label, buf);
        }
    } else {
        if (st.multi_bar != nullptr) {
            lv_obj_add_flag(st.multi_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_width(st.multi_bar, lv_pct(100));
            lv_obj_align(st.multi_bar, LV_ALIGN_CENTER, 0, 0);
        }
        if (st.list_footer != nullptr) {
            lv_obj_set_width(st.list_footer, LV_HOR_RES - 16);
            lv_obj_set_style_text_align(st.list_footer, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_font(st.list_footer, ItemFont(), 0);
            lv_obj_align(st.list_footer, LV_ALIGN_CENTER, 0, 0);
            lv_obj_clear_flag(st.list_footer, LV_OBJ_FLAG_HIDDEN);
        }
        if (st.status_label != nullptr && lv_obj_is_valid(st.status_label)) {
            lv_label_set_text(st.status_label, Lang::Strings::BOOK_SHELF_TITLE);
        }
    }
}

void ExitShelfMultiMode(bool rebuild) {
    auto& st = State();
    st.shelf_multi = false;
    st.shelf_suppress_click_until_us = 0;
    st.shelf_suppress_click_idx = -1;
    st.shelf_selected.assign(st.books.size(), 0);
    RefreshShelfFooterMode();
    if (rebuild) {
        RequestRenderBookshelfPage();
    }
}

void EnterShelfMultiModeSelect(int idx) {
    auto& st = State();
    st.shelf_multi = true;
    SyncShelfSelectedSize();
    st.shelf_selected.assign(st.books.size(), 0);
    if (idx >= 0 && idx < static_cast<int>(st.shelf_selected.size())) {
        st.shelf_selected[static_cast<size_t>(idx)] = 1;
    }
    RefreshShelfFooterMode();
    RequestRenderBookshelfPage();
}

bool DeleteLibraryBookFile(const reader::BookInfo& info, std::string& err_out) {
    err_out.clear();
    if (info.path.empty()) {
        err_out = Lang::Strings::BOOK_PATH_INVALID;
        return false;
    }
    const std::string books_prefix = std::string(SD_PATH_BOOKS) + "/";
    if (info.path.rfind(books_prefix, 0) != 0) {
        err_out = Lang::Strings::BOOK_PATH_INVALID;
        return false;
    }
    // 先 Peek/缓存 再删 .pos：供首页聚合扣减（删后无法再读）
    reader::BookSession::ProgressPeek peek;
    if (!reader::book_progress_cache::TryGet(info.path.c_str(), peek)) {
        peek = reader::BookSession::PeekProgress(info.path.c_str());
    }
    if (unlink(info.path.c_str()) != 0) {
        err_out = Lang::Strings::BOOK_DELETE_FAILED;
        return false;
    }
    unlink((info.path + ".tmp").c_str());
    unlink((info.path + ".pos").c_str());      // 进度+终身累计+当日阅读秒
    unlink((info.path + ".pos.tmp").c_str());  // 原子写残留
    reader::DeleteBookCoverSidecar(info.path.c_str());
    // TXT 分页索引缓存：同目录 <name>.txt.idx
    if (info.format == reader::BookFormat::kTxt) {
        unlink((info.path + ".idx").c_str());
    }
    ESP_LOGI(TAG, "deleted library book %s", info.path.c_str());
    reader::book_progress_cache::Erase(info.path.c_str());
    reader::book_home_snapshot::RemoveBook(info.path.c_str());
    reader::book_home_snapshot::SubtractAggregateContribution(
        peek.reading_seconds, peek.progress_x10 >= 1000);
    return true;
}

lv_obj_t* FindShelfCell(int index) {
    auto& st = State();
    if (st.list_body == nullptr || !lv_obj_is_valid(st.list_body)) {
        return nullptr;
    }
    const uint32_t n = lv_obj_get_child_count(st.list_body);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* cell = lv_obj_get_child(st.list_body, i);
        if (cell != nullptr &&
            static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(cell))) == index) {
            return cell;
        }
    }
    return nullptr;
}

lv_obj_t* FindShelfCheckBox(lv_obj_t* cell) {
    if (cell == nullptr) {
        return nullptr;
    }
    // 封面 host 为 cell 首子；勾选框挂在 host 上
    lv_obj_t* host = lv_obj_get_child(cell, 0);
    if (host == nullptr) {
        return nullptr;
    }
    const uint32_t n = lv_obj_get_child_count(host);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* c = lv_obj_get_child(host, i);
        if (c != nullptr && lv_obj_get_width(c) == kShelfCheckSize &&
            lv_obj_get_height(c) == kShelfCheckSize) {
            return c;
        }
    }
    return nullptr;
}

void PatchShelfRowCheckMark(int index) {
    auto& st = State();
    if (!st.shelf_multi) {
        return;
    }
    lv_obj_t* check = FindShelfCheckBox(FindShelfCell(index));
    if (check == nullptr) {
        RequestRenderBookshelfPage();
        return;
    }
    lv_obj_clean(check);
    if (!ShelfItemSelected(index)) {
        return;
    }
    lv_obj_t* mark = lv_label_create(check);
    lv_label_set_text(mark, "√");
    lv_obj_set_style_text_font(mark, ItemFont(), 0);
    lv_obj_set_style_text_color(mark, lv_color_black(), 0);
    lv_obj_center(mark);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
}

void PatchShelfVisibleCheckMarks() {
    auto& st = State();
    if (!st.shelf_multi || st.list_body == nullptr || !lv_obj_is_valid(st.list_body)) {
        return;
    }
    const uint32_t n = lv_obj_get_child_count(st.list_body);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* cell = lv_obj_get_child(st.list_body, i);
        if (cell == nullptr) {
            continue;
        }
        PatchShelfRowCheckMark(
            static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(cell))));
    }
    if (st.status_label != nullptr && lv_obj_is_valid(st.status_label)) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), Lang::Strings::BOOK_SELECTED_FMT, ShelfSelectedCount());
        lv_label_set_text(st.status_label, buf);
    }
}

void OnShelfMultiCancel(lv_event_t* /*e*/) {
    ExitShelfMultiMode(true);
}

void OnShelfMultiSelectAll(lv_event_t* /*e*/) {
    auto& st = State();
    if (!st.shelf_multi || st.books.empty()) {
        return;
    }
    SyncShelfSelectedSize();
    const int n = static_cast<int>(st.books.size());
    int selected_n = ShelfSelectedCount();
    const bool clear = (selected_n >= n);
    st.shelf_selected.assign(static_cast<size_t>(n), clear ? 0 : 1);
    RequestShelfCheckMarksPaint();
}

void OnShelfMultiRemove(lv_event_t* /*e*/) {
    auto& st = State();
    if (!st.shelf_multi || st.opening.load()) {
        return;
    }
    SyncShelfSelectedSize();
    if (ShelfSelectedCount() <= 0) {
        return;
    }
    // 自大下标删除，避免下标漂移
    for (int i = static_cast<int>(st.books.size()) - 1; i >= 0; --i) {
        if (i >= static_cast<int>(st.shelf_selected.size()) || st.shelf_selected[static_cast<size_t>(i)] == 0) {
            continue;
        }
        std::string err;
        if (!DeleteLibraryBookFile(st.books[static_cast<size_t>(i)], err)) {
            ESP_LOGW(TAG, "batch delete fail idx=%d: %s", i, err.c_str());
            continue;
        }
        st.books.erase(st.books.begin() + i);
        if (st.selected == i) {
            st.selected = -1;
        } else if (st.selected > i) {
            --st.selected;
        }
    }
    // 删除后以内存列表为权威，刷新缓存，避免下次 Load 复用含已删书的开机快照
    reader::ReplaceBookLibraryCache(st.books);
    st.shelf_multi = false;
    st.shelf_suppress_click_until_us = 0;
    st.shelf_suppress_click_idx = -1;
    st.shelf_selected.assign(st.books.size(), 0);
    ClampListPage();
    RefreshShelfFooterMode();
    RequestRenderBookshelfPage();
}

void OnBookRowLongPressed(lv_event_t* e) {
    auto& st = State();
    if (st.opening.load()) {
        return;
    }
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (index < 0 || index >= static_cast<int>(st.books.size())) {
        return;
    }
    if (!st.shelf_multi) {
        st.shelf_suppress_click_idx = index;
        st.shelf_suppress_click_until_us = esp_timer_get_time() + kShelfSuppressRowClickUs;
        EnterShelfMultiModeSelect(index);
        return;
    }
    ToggleShelfItemSelected(index);
    RequestShelfCheckMarksPaint();
}

void OnBookRowClicked(lv_event_t* e) {
    auto& st = State();
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (index == st.shelf_suppress_click_idx &&
        esp_timer_get_time() < st.shelf_suppress_click_until_us) {
        return;
    }
    if (st.shelf_multi) {
        ToggleShelfItemSelected(index);
        RequestShelfCheckMarksPaint();
        return;
    }
    lv_async_call(OpenDetailAsync, reinterpret_cast<void*>(static_cast<intptr_t>(index)));
}

void OnDetailStartClicked(lv_event_t* /*e*/) {
    auto& st = State();
    if (ReaderWorkersBusy()) {
        return;
    }
    lv_async_call(StartReadAsync, reinterpret_cast<void*>(static_cast<intptr_t>(st.selected)));
}


void SetReadStatusVisible(bool visible) {
    auto& st = State();
    if (st.status_bar != nullptr && lv_obj_is_valid(st.status_bar)) {
        if (visible) {
            // 盖在正文上时需白底，否则字迹透出
            lv_obj_set_style_bg_color(st.status_bar, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(st.status_bar, LV_OPA_COVER, 0);
            lv_obj_clear_flag(st.status_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(st.status_bar);
        } else {
            lv_obj_add_flag(st.status_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(st.status_bar, LV_OPA_TRANSP, 0);
        }
    }
    if (st.status_overlay != nullptr && lv_obj_is_valid(st.status_overlay)) {
        // overlay 叠在图标行上，保持透明以免盖住电量/网络
        if (visible) {
            lv_obj_set_style_bg_opa(st.status_overlay, LV_OPA_TRANSP, 0);
            lv_obj_clear_flag(st.status_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(st.status_overlay);
        } else {
            lv_obj_add_flag(st.status_overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/** @brief 状态栏中央（时钟位）：有文案则显示，nullptr/空则藏起 */
void SetReadStatusCenterTitle(const char* title) {
    auto& st = State();
    if (st.status_label == nullptr || !lv_obj_is_valid(st.status_label)) {
        return;
    }
    if (title != nullptr && title[0] != '\0') {
        lv_label_set_text(st.status_label, title);
        lv_obj_clear_flag(st.status_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(st.status_label, "");
        lv_obj_add_flag(st.status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void SetReadSettingsSheetVisible(bool visible) {
    auto& st = State();
    if (st.settings_sheet == nullptr || !lv_obj_is_valid(st.settings_sheet)) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(st.settings_sheet, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(st.settings_sheet);
        if (st.status_bar != nullptr && lv_obj_is_valid(st.status_bar)) {
            lv_obj_move_foreground(st.status_bar);
        }
        if (st.status_overlay != nullptr && lv_obj_is_valid(st.status_overlay)) {
            lv_obj_move_foreground(st.status_overlay);
        }
    } else {
        lv_obj_add_flag(st.settings_sheet, LV_OBJ_FLAG_HIDDEN);
    }
}

void HideReadChrome() {
    auto& st = State();
    ScreenPaintCoalesceReset(&s_settings_sheet_paint);
    st.font_multi = false;
    st.font_suppress_click_until_us = 0;
    st.font_suppress_click_idx = -1;
    st.font_suppress_next_click = false;
    st.font_selected.assign(st.font_entries.size(), 0);
    SetReadStatusCenterTitle(nullptr);
    SetReadStatusVisible(false);
    SetReadSettingsSheetVisible(false);
    const bool was_overlay = IsReadOverlayChrome(st.read_chrome);
    const bool was_settings = (st.read_chrome == BookUiState::ReadChrome::kSettings);
    st.settings_resume_after_open = false;
    st.read_chrome = BookUiState::ReadChrome::kReading;
    // 设置卡片盖在正文上：关掉即可，不必重绘；目录整页需恢复正文
    if (was_overlay && !was_settings && st.session && st.session->IsOpen() &&
        st.content != nullptr) {
        CancelListCoverFill();  // 目录封面槽将随 content 重建销毁
        ApplyReaderPageGeometry(ReaderGeomMode::kImmersive);
        RenderReaderPage();
    } else if (was_settings) {
        // 关卡：落盘未完成的 ± 排版；StartChapterPageWorker 自检 Needs（卡上曾推迟）
        FlushLayoutDebounce();
        ApplyReaderPageGeometry(ReaderGeomMode::kImmersive);
        StartChapterPageWorker();
    }
}

// 设置卡控件已缓存时原地刷；不再因按下推迟整卡 clean（±/翻页不拆树）
void ClearSettingsSheetWidgetRefs() {
    auto& st = State();
    for (int i = 0; i < kFontListPageSize; ++i) {
        st.sheet_font_rows[i] = {};
    }
    st.sheet_font_title = nullptr;
    st.sheet_font_page_prev = nullptr;
    st.sheet_font_page_next = nullptr;
    st.sheet_font_page_lab = nullptr;
    st.sheet_font_multi_bar = nullptr;
    st.sheet_margin_value = nullptr;
    st.sheet_margin_dec = nullptr;
    st.sheet_margin_inc = nullptr;
    st.sheet_margin_bound = nullptr;
    st.sheet_gap_value = nullptr;
    st.sheet_gap_dec = nullptr;
    st.sheet_gap_inc = nullptr;
    st.sheet_gap_bound = nullptr;
    st.sheet_font_import = nullptr;
    st.ttf_sheet = nullptr;
    for (int i = 0; i < kFontListPageSize; ++i) {
        st.ttf_rows[i] = {};
    }
    st.ttf_bar = nullptr;
    st.ttf_pct_lbl = nullptr;
    st.ttf_msg_lbl = nullptr;
    st.ttf_size_value = nullptr;
    st.ttf_size_dec = nullptr;
    st.ttf_size_inc = nullptr;
}

bool SettingsSheetWidgetsReady() {
    auto& st = State();
    return st.settings_sheet != nullptr && lv_obj_is_valid(st.settings_sheet) &&
           st.sheet_margin_value != nullptr && lv_obj_is_valid(st.sheet_margin_value) &&
           st.sheet_font_page_lab != nullptr && lv_obj_is_valid(st.sheet_font_page_lab);
}

void SetSheetBtnEnabled(lv_obj_t* btn, bool enabled) {
    if (btn == nullptr || !lv_obj_is_valid(btn)) {
        return;
    }
    if (enabled) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(btn, LV_OPA_COVER, 0);
    } else {
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(btn, LV_OPA_40, 0);
    }
}

void UpdateSheetBoundTip(lv_obj_t* tip, bool can_dec, bool can_inc) {
    if (tip == nullptr || !lv_obj_is_valid(tip)) {
        return;
    }
    if (!can_dec) {
        lv_label_set_text(tip, "MIN");
        lv_obj_clear_flag(tip, LV_OBJ_FLAG_HIDDEN);
    } else if (!can_inc) {
        lv_label_set_text(tip, "MAX");
        lv_obj_clear_flag(tip, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(tip, LV_OBJ_FLAG_HIDDEN);
    }
}

void RefreshSettingsSheetAdj() {
    auto& st = State();
    if (!SettingsSheetWidgetsReady()) {
        return;
    }
    const int margin_i = BookReaderPrefsMarginPreset();
    const int gap_i = BookReaderPrefsSpacingPreset();
    const bool margin_dec = margin_i > 0;
    const bool margin_inc = margin_i + 1 < kBookReaderMarginPresetCount;
    const bool gap_dec = gap_i > 0;
    const bool gap_inc = gap_i + 1 < kBookReaderSpacingPresetCount;

    lv_label_set_text(st.sheet_margin_value, BookReaderPrefsMarginLabel(margin_i));
    if (st.sheet_gap_value != nullptr && lv_obj_is_valid(st.sheet_gap_value)) {
        lv_label_set_text(st.sheet_gap_value, BookReaderPrefsSpacingLabel(gap_i));
    }
    SetSheetBtnEnabled(st.sheet_margin_dec, margin_dec);
    SetSheetBtnEnabled(st.sheet_margin_inc, margin_inc);
    SetSheetBtnEnabled(st.sheet_gap_dec, gap_dec);
    SetSheetBtnEnabled(st.sheet_gap_inc, gap_inc);
    UpdateSheetBoundTip(st.sheet_margin_bound, margin_dec, margin_inc);
    UpdateSheetBoundTip(st.sheet_gap_bound, gap_dec, gap_inc);
}

void RefreshSettingsSheetFontList() {
    auto& st = State();
    if (!SettingsSheetWidgetsReady()) {
        return;
    }
    ClampFontListPage();
    SyncFontSelectedSize();
    const int font_i = CurrentFontIndex();
    const int font_n = static_cast<int>(st.font_entries.size());
    const int font_pages = FontListPageCount();
    const int font_page_start = st.font_list_page * kFontListPageSize;

    for (int i = 0; i < kFontListPageSize; ++i) {
        auto& fr = st.sheet_font_rows[i];
        if (fr.row == nullptr || !lv_obj_is_valid(fr.row)) {
            continue;
        }
        const int idx = font_page_start + i;
        if (idx >= font_n) {
            lv_obj_clear_flag(fr.row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_border_width(fr.row, 0, 0);
            lv_obj_set_style_border_side(fr.row, LV_BORDER_SIDE_NONE, 0);
            lv_obj_set_user_data(fr.row, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
            if (fr.name != nullptr && lv_obj_is_valid(fr.name)) {
                lv_label_set_text(fr.name, "");
            }
            if (fr.px != nullptr && lv_obj_is_valid(fr.px)) {
                lv_label_set_text(fr.px, "");
            }
            if (fr.check != nullptr && lv_obj_is_valid(fr.check)) {
                lv_obj_add_flag(fr.check, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clean(fr.check);
            }
            continue;
        }

        const ReadFontEntry& fe = st.font_entries[static_cast<size_t>(idx)];
        const bool on = (!st.font_multi && idx == font_i);
        const bool next_on = (!st.font_multi) && (i + 1 < kFontListPageSize) &&
                             (font_page_start + i + 1 == font_i) &&
                             (font_page_start + i + 1 < font_n);
        if (on) {
            lv_obj_set_style_border_width(fr.row, kTocLineCur, 0);
            lv_obj_set_style_border_side(
                fr.row, static_cast<lv_border_side_t>(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM),
                0);
        } else if (next_on) {
            lv_obj_set_style_border_width(fr.row, 0, 0);
            lv_obj_set_style_border_side(fr.row, LV_BORDER_SIDE_NONE, 0);
        } else if (i + 1 < kFontListPageSize) {
            lv_obj_set_style_border_width(fr.row, kTocLineThin, 0);
            lv_obj_set_style_border_side(fr.row, LV_BORDER_SIDE_BOTTOM, 0);
        } else {
            lv_obj_set_style_border_width(fr.row, 0, 0);
            lv_obj_set_style_border_side(fr.row, LV_BORDER_SIDE_NONE, 0);
        }
        lv_obj_set_style_border_color(fr.row, lv_color_black(), 0);
        lv_obj_set_style_border_opa(fr.row, LV_OPA_COVER, 0);

        lv_obj_add_flag(fr.row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(fr.row, reinterpret_cast<void*>(static_cast<intptr_t>(idx)));

        char name_buf[40];
        FormatFontListName(fe.file, name_buf, sizeof(name_buf));
        if (fr.name != nullptr && lv_obj_is_valid(fr.name)) {
            lv_label_set_text(fr.name, name_buf);
            lv_obj_set_style_text_font(fr.name, on ? ListFont() : ItemFont(), 0);
        }
        char px_buf[16];
        if (fe.size_px > 0) {
            std::snprintf(px_buf, sizeof(px_buf), "%dpx", fe.size_px);
        } else {
            std::snprintf(px_buf, sizeof(px_buf), "--");
        }
        if (fr.px != nullptr && lv_obj_is_valid(fr.px)) {
            lv_label_set_text(fr.px, px_buf);
            lv_obj_set_style_text_opa(fr.px, on ? LV_OPA_COVER : LV_OPA_70, 0);
        }
        if (fr.check != nullptr && lv_obj_is_valid(fr.check)) {
            if (st.font_multi) {
                lv_obj_clear_flag(fr.check, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clean(fr.check);
                if (FontItemSelected(idx)) {
                    lv_obj_t* mark = lv_label_create(fr.check);
                    lv_label_set_text(mark, "√");
                    lv_obj_set_style_text_font(mark, ItemFont(), 0);
                    lv_obj_set_style_text_color(mark, lv_color_black(), 0);
                    lv_obj_center(mark);
                    lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
                }
            } else {
                lv_obj_add_flag(fr.check, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clean(fr.check);
            }
        }
    }

    const bool can_prev = st.font_list_page > 0;
    const bool can_next = st.font_list_page + 1 < font_pages;
    SetSheetBtnEnabled(st.sheet_font_page_prev, can_prev);
    SetSheetBtnEnabled(st.sheet_font_page_next, can_next);
    if (st.sheet_font_page_lab != nullptr && lv_obj_is_valid(st.sheet_font_page_lab)) {
        char page_meta[24];
        std::snprintf(page_meta, sizeof(page_meta), "%d / %d", st.font_list_page + 1, font_pages);
        lv_label_set_text(st.sheet_font_page_lab, page_meta);
    }
    RefreshFontMultiFooter();
}

void RefreshSettingsSheet() {
    if (State().read_chrome != BookUiState::ReadChrome::kSettings) {
        return;
    }
    RefreshSettingsSheetFontList();
    RefreshSettingsSheetAdj();
}

/** 排版后保卡可见；控件已由 coalesce/Rebuild 就绪则不再叠刷 */
void EnsureSettingsSheetAfterLayout() {
    SetReadStatusVisible(true);
    SetReadSettingsSheetVisible(true);
    if (!SettingsSheetWidgetsReady()) {
        EnsureFontListPageShowsSelection();
        RebuildSettingsSheet();
    }
}

void ShowReadSettingsSheet() {
    auto& st = State();
    if (st.read_scr == nullptr || st.opening.load() || !st.session || !st.session->IsOpen()) {
        return;
    }
    s_reader_page_delta.store(0, std::memory_order_release);
    st.read_chrome = BookUiState::ReadChrome::kSettings;
    SetReadStatusCenterTitle(nullptr);
    SetReadStatusVisible(true);
    SetReadSettingsSheetVisible(true);

    EnsureFontListPageShowsSelection();
    if (SettingsSheetWidgetsReady()) {
        // 与 ▲▼/± 同路：合并上屏，勿同步再刷一帧
        RequestSettingsSheetPaint();
        return;
    }
    RebuildSettingsSheet();
}

void OpenReadToc() {
    auto& st = State();
    if (st.read_scr == nullptr || st.opening.load() || !st.session || !st.session->IsOpen()) {
        return;
    }
    FlushLayoutDebounce();
    if (st.read_chrome == BookUiState::ReadChrome::kToc) {
        return;
    }
    s_reader_page_delta.store(0, std::memory_order_release);
    SetReadSettingsSheetVisible(false);
    st.read_chrome = BookUiState::ReadChrome::kToc;
    // -1：等 RenderTocList 算出本页行数后再落到当前章所在页
    st.toc_list_page = -1;
    ApplyReaderPageGeometry(ReaderGeomMode::kOverlayList);
    RenderTocList();
}

bool ParseEfSizePx(const std::string& name, int* size_px) {
    if (size_px == nullptr || name.size() < 8) {
        return false;
    }
    const size_t dot = name.rfind(".ef");
    if (dot == std::string::npos || dot + 3 != name.size()) {
        return false;
    }
    const size_t u2 = name.rfind('_', dot);
    if (u2 == std::string::npos || u2 == 0) {
        return false;
    }
    const size_t u1 = name.rfind('_', u2 - 1);
    if (u1 == std::string::npos) {
        return false;
    }
    int sz = 0;
    for (size_t i = u1 + 1; i < u2; ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return false;
        }
        sz = sz * 10 + (name[i] - '0');
    }
    if (sz <= 0) {
        return false;
    }
    *size_px = sz;
    return true;
}

bool ReadFontEntryLess(const ReadFontEntry& a, const ReadFontEntry& b) {
    if (a.size_px != b.size_px) {
        return a.size_px < b.size_px;
    }
    return a.file < b.file;
}

void ScanReadFonts() {
    auto& st = State();
    st.font_entries.clear();
    DIR* dir = opendir(SD_PATH_FONTS);
    if (dir == nullptr) {
        return;
    }
    while (dirent* ent = readdir(dir)) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        const size_t n = std::strlen(ent->d_name);
        if (n < 4 || n > 63 || std::strcmp(ent->d_name + (n - 3), ".ef") != 0) {
            continue;
        }
        char name[64];
        std::memcpy(name, ent->d_name, n + 1);
        char full[96];
        std::snprintf(full, sizeof(full), "%s/%s", SD_PATH_FONTS, name);
        struct stat st_info {};
        if (stat(full, &st_info) != 0 || !S_ISREG(st_info.st_mode)) {
            continue;
        }
        ReadFontEntry fe;
        fe.file = name;
        if (!ParseEfSizePx(fe.file, &fe.size_px)) {
            fe.size_px = 0;
        }
        st.font_entries.push_back(std::move(fe));
    }
    closedir(dir);
    std::sort(st.font_entries.begin(), st.font_entries.end(), ReadFontEntryLess);
}

int CurrentFontIndex() {
    auto& st = State();
    if (st.font_entries.empty()) {
        ScanReadFonts();
    }
    const char* cur = BookReaderPrefsFontFile();
    for (int i = 0; i < static_cast<int>(st.font_entries.size()); ++i) {
        if (st.font_entries[static_cast<size_t>(i)].file == cur) {
            return i;
        }
    }
    return st.font_entries.empty() ? -1 : 0;
}

int FontListPageCount() {
    const int n = static_cast<int>(State().font_entries.size());
    if (n <= 0) {
        return 1;
    }
    return (n + kFontListPageSize - 1) / kFontListPageSize;
}

void ClampFontListPage() {
    auto& st = State();
    const int pages = FontListPageCount();
    if (st.font_list_page < 0) {
        st.font_list_page = 0;
    }
    if (st.font_list_page >= pages) {
        st.font_list_page = pages - 1;
    }
}

// ---------- TTF 导入转换面板（覆盖设置卡；转换 worker 独立，UI 只轮询） ----------

bool IsTtfFileName(const char* name) {
    const size_t n = std::strlen(name);
    if (n < 5 || n > 63) {
        return false;
    }
    return strcasecmp(name + n - 4, ".ttf") == 0 || strcasecmp(name + n - 4, ".otf") == 0;
}

int TtfPageCount() {
    const int n = static_cast<int>(State().ttf_files.size());
    if (n <= 0) {
        return 1;
    }
    return (n + kFontListPageSize - 1) / kFontListPageSize;
}

void TtfScanFiles() {
    auto& st = State();
    st.ttf_files.clear();
    DIR* dir = opendir(SD_PATH_FONT_SRC);
    if (dir == nullptr) {
        return;
    }
    while (dirent* ent = readdir(dir)) {
        if (ent->d_name[0] == '.' || !IsTtfFileName(ent->d_name)) {
            continue;
        }
        st.ttf_files.push_back(ent->d_name);
    }
    closedir(dir);
    std::sort(st.ttf_files.begin(), st.ttf_files.end());
}

void TtfPanelClose() {
    auto& st = State();
    if (st.ttf_sheet != nullptr && lv_obj_is_valid(st.ttf_sheet)) {
        lv_obj_del(st.ttf_sheet);
    }
    st.ttf_sheet = nullptr;
    st.ttf_bar = nullptr;
    st.ttf_pct_lbl = nullptr;
    st.ttf_msg_lbl = nullptr;
    st.ttf_size_value = nullptr;
    st.ttf_size_dec = nullptr;
    st.ttf_size_inc = nullptr;
    for (int i = 0; i < kFontListPageSize; ++i) {
        st.ttf_rows[i] = {};
    }
    st.ttf_mode = BookUiState::TtfMode::kNone;
}

void StopTtfPollTimer() {
    auto& st = State();
    if (st.ttf_poll_timer != nullptr) {
        lv_timer_del(st.ttf_poll_timer);
        st.ttf_poll_timer = nullptr;
    }
}

void TtfEnsurePanel() {
    auto& st = State();
    if (st.settings_sheet == nullptr || !lv_obj_is_valid(st.settings_sheet)) {
        return;
    }
    // 设置卡是 COLUMN + SIZE_CONTENT：overlay 必须 FLOATING，否则会被 flex 排到卡底，
    // 且相对 content 父级的 pct(100) 高度常为 0 → 点「导入 TTF」像没反应。
    lv_obj_update_layout(st.settings_sheet);
    if (st.ttf_sheet != nullptr && lv_obj_is_valid(st.ttf_sheet)) {
        lv_obj_set_size(st.ttf_sheet, lv_pct(100), lv_pct(100));
        lv_obj_align(st.ttf_sheet, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_move_foreground(st.ttf_sheet);
        TtfRebuildContent();
        return;
    }
    st.ttf_sheet = lv_obj_create(st.settings_sheet);
    lv_obj_remove_style_all(st.ttf_sheet);
    lv_obj_add_flag(st.ttf_sheet, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(st.ttf_sheet, lv_pct(100), lv_pct(100));
    lv_obj_align(st.ttf_sheet, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(st.ttf_sheet, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(st.ttf_sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(st.ttf_sheet, 12, 0);
    lv_obj_set_style_pad_all(st.ttf_sheet, 16, 0);
    lv_obj_set_style_pad_row(st.ttf_sheet, 10, 0);
    lv_obj_set_flex_flow(st.ttf_sheet, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(st.ttf_sheet, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(st.ttf_sheet);
    lv_obj_move_foreground(st.ttf_sheet);
    TtfRebuildContent();
}

void TtfRebuildContent() {
    auto& st = State();
    if (st.ttf_sheet == nullptr || !lv_obj_is_valid(st.ttf_sheet)) {
        return;
    }
    lv_obj_clean(st.ttf_sheet);
    for (int i = 0; i < kFontListPageSize; ++i) {
        st.ttf_rows[i] = {};
    }
    st.ttf_bar = nullptr;
    st.ttf_pct_lbl = nullptr;
    st.ttf_msg_lbl = nullptr;
    st.ttf_size_value = nullptr;
    st.ttf_size_dec = nullptr;
    st.ttf_size_inc = nullptr;

    const bool is_progress = (st.ttf_mode == BookUiState::TtfMode::kProgress);
    auto make_action = [](lv_obj_t* parent, const char* text, lv_event_cb_t cb) -> lv_obj_t* {
        lv_obj_t* btn = lv_obj_create(parent);
        lv_obj_remove_style_all(btn);
        lv_obj_set_height(btn, 36);
        lv_obj_set_width(btn, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_hor(btn, 4, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        DisableScroll(btn);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(
            btn, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* wrap = lv_obj_create(btn);
        lv_obj_remove_style_all(wrap);
        lv_obj_set_size(wrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_border_side(wrap, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(wrap, kShelfUnderlineH, 0);
        lv_obj_set_style_border_color(wrap, lv_color_black(), 0);
        lv_obj_set_style_pad_bottom(wrap, 2, 0);
        lv_obj_set_style_pad_hor(wrap, kShelfUnderlinePadHor, 0);
        lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
        DisableScroll(wrap);
        lv_obj_clear_flag(wrap, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* lbl = lv_label_create(wrap);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, ItemFont(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        return btn;
    };

    // 头部：标题 + 关闭（进度模式不可关）
    lv_obj_t* head = lv_obj_create(st.ttf_sheet);
    lv_obj_remove_style_all(head);
    lv_obj_set_width(head, lv_pct(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(head);
    const char* title = "";
    switch (st.ttf_mode) {
        case BookUiState::TtfMode::kList: title = "选择 TTF 字体"; break;
        case BookUiState::TtfMode::kConfirm: title = "导入 TTF"; break;
        case BookUiState::TtfMode::kProgress: title = "转换中"; break;
        case BookUiState::TtfMode::kResult: title = st.ttf_result_ok ? "转换完成" : "转换失败"; break;
        default: break;
    }
    lv_obj_t* title_lbl = lv_label_create(head);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, ListFont(), 0);
    lv_obj_clear_flag(title_lbl, LV_OBJ_FLAG_CLICKABLE);
    if (!is_progress) {
        make_action(head, "关闭", OnTtfClose);
    }

    switch (st.ttf_mode) {
        case BookUiState::TtfMode::kList: {
            if (st.ttf_files.empty()) {
                lv_obj_t* empty = lv_label_create(st.ttf_sheet);
                lv_label_set_text(
                    empty, "未找到 TTF/OTF 字体\n请将字体文件放入 SD 卡 metalio/e-ink/fonts_ttf 目录");
                lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
                lv_obj_set_width(empty, lv_pct(100));
                lv_obj_set_style_text_font(empty, ItemFont(), 0);
                lv_obj_set_style_text_opa(empty, LV_OPA_70, 0);
                lv_obj_clear_flag(empty, LV_OBJ_FLAG_CLICKABLE);
                break;
            }
            lv_obj_t* list = lv_obj_create(st.ttf_sheet);
            lv_obj_remove_style_all(list);
            lv_obj_set_width(list, lv_pct(100));
            lv_obj_set_height(list, kFontListRowH * kFontListPageSize);
            lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
            lv_obj_clear_flag(list, LV_OBJ_FLAG_CLICKABLE);
            DisableScroll(list);
            const int start = st.ttf_page * kFontListPageSize;
            for (int i = 0; i < kFontListPageSize; ++i) {
                const int idx = start + i;
                lv_obj_t* row = lv_obj_create(list);
                lv_obj_remove_style_all(row);
                lv_obj_set_width(row, lv_pct(100));
                lv_obj_set_height(row, kFontListRowH);
                lv_obj_set_style_pad_hor(row, 12, 0);
                lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_CENTER);
                if (i + 1 < kFontListPageSize) {
                    lv_obj_set_style_border_width(row, kTocLineThin, 0);
                    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
                    lv_obj_set_style_border_color(row, lv_color_black(), 0);
                }
                DisableScroll(row);
                lv_obj_t* name = lv_label_create(row);
                lv_obj_set_flex_grow(name, 1);
                lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
                lv_obj_set_style_text_font(name, ItemFont(), 0);
                lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);
                st.ttf_rows[i].row = row;
                st.ttf_rows[i].name = name;
                if (idx >= static_cast<int>(st.ttf_files.size())) {
                    lv_label_set_text(name, "");
                    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_user_data(row, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
                    continue;
                }
                HapticAttachClick(row);
                lv_obj_add_event_cb(row, OnTtfRowClick, LV_EVENT_CLICKED, nullptr);
                lv_obj_add_event_cb(
                    row, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED,
                    nullptr);
                lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_user_data(row, reinterpret_cast<void*>(static_cast<intptr_t>(idx)));
                lv_label_set_text(name, st.ttf_files[static_cast<size_t>(idx)].c_str());
            }
            const int pages = TtfPageCount();
            if (pages > 1) {
                lv_obj_t* pager = lv_obj_create(st.ttf_sheet);
                lv_obj_remove_style_all(pager);
                lv_obj_set_width(pager, lv_pct(100));
                lv_obj_set_height(pager, 36);
                lv_obj_set_flex_flow(pager, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(pager, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_CENTER);
                lv_obj_clear_flag(pager, LV_OBJ_FLAG_CLICKABLE);
                DisableScroll(pager);
                make_action(pager, "▲", OnTtfPagePrev);
                char meta[32];
                std::snprintf(meta, sizeof(meta), "%d / %d", st.ttf_page + 1, pages);
                lv_obj_t* lab = lv_label_create(pager);
                lv_label_set_text(lab, meta);
                lv_obj_set_style_text_font(lab, ItemFont(), 0);
                lv_obj_set_style_text_opa(lab, LV_OPA_70, 0);
                lv_obj_clear_flag(lab, LV_OBJ_FLAG_CLICKABLE);
                make_action(pager, "▼", OnTtfPageNext);
            }
            break;
        }
        case BookUiState::TtfMode::kConfirm: {
            ClampTtfSizePx();
            lv_obj_t* msg = lv_label_create(st.ttf_sheet);
            lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(msg, lv_pct(100));
            lv_obj_set_style_text_font(msg, ItemFont(), 0);
            lv_obj_clear_flag(msg, LV_OBJ_FLAG_CLICKABLE);
            st.ttf_msg_lbl = msg;

            // 字号步进器：20–40，步进 1，默认 25；只生成一档
            lv_obj_t* size_row = lv_obj_create(st.ttf_sheet);
            lv_obj_remove_style_all(size_row);
            lv_obj_set_width(size_row, lv_pct(100));
            lv_obj_set_height(size_row, kSheetIconBtn);
            lv_obj_set_flex_flow(size_row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(size_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_clear_flag(size_row, LV_OBJ_FLAG_CLICKABLE);
            DisableScroll(size_row);
            lv_obj_t* size_lab = lv_label_create(size_row);
            lv_label_set_text(size_lab, "字号");
            lv_obj_set_style_text_font(size_lab, ItemFont(), 0);
            lv_obj_clear_flag(size_lab, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t* stepper = lv_obj_create(size_row);
            lv_obj_remove_style_all(stepper);
            lv_obj_set_height(stepper, kSheetIconBtn);
            lv_obj_set_width(stepper, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(stepper, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(stepper, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(stepper, 10, 0);
            lv_obj_clear_flag(stepper, LV_OBJ_FLAG_CLICKABLE);
            DisableScroll(stepper);
            st.ttf_size_dec =
                MakeSheetIconBtn(stepper, "-", OnTtfSizeDec, st.ttf_size_px > kTtfConvertSizeMin);
            lv_obj_t* size_val = lv_label_create(stepper);
            char size_txt[16];
            std::snprintf(size_txt, sizeof(size_txt), "%dpx", st.ttf_size_px);
            lv_label_set_text(size_val, size_txt);
            lv_obj_set_style_text_font(size_val, ListFont(), 0);
            lv_obj_clear_flag(size_val, LV_OBJ_FLAG_CLICKABLE);
            st.ttf_size_value = size_val;
            st.ttf_size_inc =
                MakeSheetIconBtn(stepper, "+", OnTtfSizeInc, st.ttf_size_px < kTtfConvertSizeMax);

            RefreshTtfConfirmTexts();

            lv_obj_t* btns = lv_obj_create(st.ttf_sheet);
            lv_obj_remove_style_all(btns);
            lv_obj_set_width(btns, lv_pct(100));
            lv_obj_set_height(btns, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_hor(btns, 24, 0);
            lv_obj_clear_flag(btns, LV_OBJ_FLAG_CLICKABLE);
            DisableScroll(btns);
            make_action(btns, "开始转换", OnTtfConfirmYes);
            make_action(btns, "取消", OnTtfConfirmNo);
            break;
        }
        case BookUiState::TtfMode::kProgress: {
            ClampTtfSizePx();
            const uint16_t sz = static_cast<uint16_t>(st.ttf_size_px);
            char out_name[80];
            ttf_convert::FormatOutputFileList(st.ttf_pick.c_str(), &sz, 1, out_name,
                                              sizeof(out_name));
            lv_obj_t* msg = lv_label_create(st.ttf_sheet);
            char mbuf[200];
            std::snprintf(mbuf, sizeof(mbuf),
                          "正在转换：\n%.40s\n→ %s\n请勿拔出 SD 卡…", st.ttf_pick.c_str(),
                          out_name);
            lv_label_set_text(msg, mbuf);
            lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(msg, lv_pct(100));
            lv_obj_set_style_text_font(msg, ItemFont(), 0);
            lv_obj_clear_flag(msg, LV_OBJ_FLAG_CLICKABLE);
            st.ttf_msg_lbl = msg;

            lv_obj_t* bar = lv_bar_create(st.ttf_sheet);
            lv_obj_set_size(bar, lv_pct(100), 16);
            lv_bar_set_range(bar, 0, 100);
            lv_bar_set_value(bar, ttf_convert::Percent(), LV_ANIM_OFF);
            lv_obj_set_style_bg_color(bar, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(bar, lv_color_black(), 0);
            lv_obj_set_style_border_width(bar, 2, 0);
            lv_obj_set_style_radius(bar, 4, 0);
            lv_obj_set_style_bg_color(bar, lv_color_black(), LV_PART_INDICATOR);
            lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
            st.ttf_bar = bar;

            lv_obj_t* pct = lv_label_create(st.ttf_sheet);
            char pbuf[16];
            std::snprintf(pbuf, sizeof(pbuf), "%d%%", ttf_convert::Percent());
            lv_label_set_text(pct, pbuf);
            lv_obj_set_style_text_font(pct, ListFont(), 0);
            lv_obj_clear_flag(pct, LV_OBJ_FLAG_CLICKABLE);
            st.ttf_pct_lbl = pct;

            lv_obj_t* tip = lv_label_create(st.ttf_sheet);
            lv_label_set_text(tip, "转换完成前，请勿退出页面");
            lv_label_set_long_mode(tip, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(tip, lv_pct(100));
            lv_obj_set_style_text_font(tip, ItemFont(), 0);
            lv_obj_set_style_text_align(tip, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_clear_flag(tip, LV_OBJ_FLAG_CLICKABLE);
            break;
        }
        case BookUiState::TtfMode::kResult: {
            lv_obj_t* msg = lv_label_create(st.ttf_sheet);
            if (st.ttf_result_ok) {
                const uint16_t sz = static_cast<uint16_t>(st.ttf_size_px);
                char out_name[80];
                ttf_convert::FormatOutputFileList(st.ttf_pick.c_str(), &sz, 1, out_name,
                                                  sizeof(out_name));
                char buf[160];
                std::snprintf(buf, sizeof(buf), "转换完成：\n%s\n已加入字库列表。", out_name);
                lv_label_set_text(msg, buf);
            } else {
                char buf[96];
                std::snprintf(buf, sizeof(buf), "转换失败：%.60s", ttf_convert::Error());
                lv_label_set_text(msg, buf);
            }
            lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(msg, lv_pct(100));
            lv_obj_set_style_text_font(msg, ItemFont(), 0);
            lv_obj_clear_flag(msg, LV_OBJ_FLAG_CLICKABLE);
            st.ttf_msg_lbl = msg;
            lv_obj_t* btns = lv_obj_create(st.ttf_sheet);
            lv_obj_remove_style_all(btns);
            lv_obj_set_width(btns, lv_pct(100));
            lv_obj_set_height(btns, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_clear_flag(btns, LV_OBJ_FLAG_CLICKABLE);
            DisableScroll(btns);
            make_action(btns, "确定", OnTtfResultOk);
            break;
        }
        default:
            break;
    }
}

void OnFontImportClick(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (st.opening.load() || ttf_convert::Busy()) {
        return;
    }
    if (st.font_multi) {
        return;
    }
    TtfScanFiles();
    st.ttf_page = 0;
    st.ttf_mode = BookUiState::TtfMode::kList;
    TtfEnsurePanel();
}

void OnTtfRowClick(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (st.opening.load()) {
        return;
    }
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const int idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
    if (idx < 0 || idx >= static_cast<int>(st.ttf_files.size())) {
        return;
    }
    st.ttf_pick = st.ttf_files[static_cast<size_t>(idx)];
    st.ttf_size_px = kTtfConvertSizeDefault;
    st.ttf_mode = BookUiState::TtfMode::kConfirm;
    TtfRebuildContent();
}

void ClampTtfSizePx() {
    auto& st = State();
    if (st.ttf_size_px < kTtfConvertSizeMin) {
        st.ttf_size_px = kTtfConvertSizeMin;
    } else if (st.ttf_size_px > kTtfConvertSizeMax) {
        st.ttf_size_px = kTtfConvertSizeMax;
    }
}

void RefreshTtfConfirmTexts() {
    auto& st = State();
    ClampTtfSizePx();
    if (st.ttf_size_value != nullptr && lv_obj_is_valid(st.ttf_size_value)) {
        char size_txt[16];
        std::snprintf(size_txt, sizeof(size_txt), "%dpx", st.ttf_size_px);
        lv_label_set_text(st.ttf_size_value, size_txt);
    }
    SetSheetBtnEnabled(st.ttf_size_dec, st.ttf_size_px > kTtfConvertSizeMin);
    SetSheetBtnEnabled(st.ttf_size_inc, st.ttf_size_px < kTtfConvertSizeMax);
    if (st.ttf_msg_lbl != nullptr && lv_obj_is_valid(st.ttf_msg_lbl)) {
        const uint16_t sz = static_cast<uint16_t>(st.ttf_size_px);
        char out_name[80];
        ttf_convert::FormatOutputFileList(st.ttf_pick.c_str(), &sz, 1, out_name, sizeof(out_name));
        char buf[200];
        std::snprintf(buf, sizeof(buf),
                      "将 %.32s 转为：\n%s\n约需 1-2 分钟，同名 .ef 将被覆盖。",
                      st.ttf_pick.c_str(), out_name);
        lv_label_set_text(st.ttf_msg_lbl, buf);
    }
}

void OnTtfSizeDec(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (st.ttf_mode != BookUiState::TtfMode::kConfirm || st.opening.load() ||
        ttf_convert::Busy()) {
        return;
    }
    if (st.ttf_size_px > kTtfConvertSizeMin) {
        --st.ttf_size_px;
        RefreshTtfConfirmTexts();
    }
}

void OnTtfSizeInc(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (st.ttf_mode != BookUiState::TtfMode::kConfirm || st.opening.load() ||
        ttf_convert::Busy()) {
        return;
    }
    if (st.ttf_size_px < kTtfConvertSizeMax) {
        ++st.ttf_size_px;
        RefreshTtfConfirmTexts();
    }
}

void OnTtfPagePrev(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (st.ttf_page > 0) {
        --st.ttf_page;
        TtfRebuildContent();
    }
}

void OnTtfPageNext(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (st.ttf_page + 1 < TtfPageCount()) {
        ++st.ttf_page;
        TtfRebuildContent();
    }
}

void TtfStartConvert() {
    auto& st = State();
    ClampTtfSizePx();
    // 字集模板：当前选中 .ef；无则回退默认 misans
    std::string tmpl;
    const int fi = CurrentFontIndex();
    if (fi >= 0 && fi < static_cast<int>(st.font_entries.size())) {
        tmpl = std::string(SD_PATH_FONTS) + "/" + st.font_entries[static_cast<size_t>(fi)].file;
    } else {
        tmpl = SD_PATH_BOOK_FONT;
    }
    const std::string ttf_path = std::string(SD_PATH_FONT_SRC) + "/" + st.ttf_pick;
    const uint16_t sizes[1] = {static_cast<uint16_t>(st.ttf_size_px)};
    if (!ttf_convert::Start(ttf_path.c_str(), tmpl.c_str(), SD_PATH_FONTS, sizes, 1)) {
        st.ttf_result_ok = false;
        st.ttf_mode = BookUiState::TtfMode::kResult;
        TtfRebuildContent();
        return;
    }
    st.ttf_mode = BookUiState::TtfMode::kProgress;
    TtfRebuildContent();
    if (st.ttf_poll_timer == nullptr) {
        st.ttf_poll_timer = lv_timer_create(TtfPollTimerCb, 500, nullptr);
    }
}

void OnTtfConfirmYes(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (st.opening.load() || ttf_convert::Busy()) {
        return;
    }
    TtfStartConvert();
}

void OnTtfConfirmNo(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    st.ttf_mode = BookUiState::TtfMode::kList;
    TtfRebuildContent();
}

void OnTtfClose(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    TtfPanelClose();
}

void OnTtfResultOk(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    TtfPanelClose();
}

void TtfPollTimerCb(lv_timer_t* t) {
    auto& st = State();
    if (ttf_convert::Busy()) {
        if (st.ttf_bar != nullptr && lv_obj_is_valid(st.ttf_bar)) {
            lv_bar_set_value(st.ttf_bar, ttf_convert::Percent(), LV_ANIM_OFF);
        }
        if (st.ttf_pct_lbl != nullptr && lv_obj_is_valid(st.ttf_pct_lbl)) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d%%", ttf_convert::Percent());
            lv_label_set_text(st.ttf_pct_lbl, buf);
        }
        return;
    }
    const int s = ttf_convert::State();
    lv_timer_del(t);
    st.ttf_poll_timer = nullptr;
    // 阅读屏已拆掉：只收尾定时器，勿碰已销毁控件
    if (st.read_scr == nullptr || !lv_obj_is_valid(st.read_scr)) {
        st.ttf_mode = BookUiState::TtfMode::kNone;
        return;
    }
    ScanReadFonts();
    st.ttf_result_ok = (s == 2);
    if (st.ttf_mode == BookUiState::TtfMode::kProgress && st.settings_sheet != nullptr &&
        lv_obj_is_valid(st.settings_sheet) && st.ttf_sheet != nullptr &&
        lv_obj_is_valid(st.ttf_sheet)) {
        st.ttf_mode = BookUiState::TtfMode::kResult;
        TtfRebuildContent();
        ClampFontListPage();
        RefreshSettingsSheetFontList();
    } else {
        // 面板已关：静默收尾；字库仍刷新，下次打开设置可见
        st.ttf_mode = BookUiState::TtfMode::kNone;
        ClampFontListPage();
        if (st.settings_sheet != nullptr && lv_obj_is_valid(st.settings_sheet)) {
            RefreshSettingsSheetFontList();
        }
    }
}

void EnsureFontListPageShowsSelection() {
    auto& st = State();
    const int cur = CurrentFontIndex();
    if (cur < 0) {
        st.font_list_page = 0;
        return;
    }
    st.font_list_page = cur / kFontListPageSize;
    ClampFontListPage();
}

/** 列表主文案：去掉 .ef；过长截断 */
void FormatFontListName(const std::string& file, char* out, size_t out_len) {
    if (out == nullptr || out_len == 0) {
        return;
    }
    std::string stem = file;
    if (stem.size() > 3 && stem.compare(stem.size() - 3, 3, ".ef") == 0) {
        stem.resize(stem.size() - 3);
    }
    if (stem.empty()) {
        stem = file;
    }
    // 墨水屏行宽有限：过长用省略
    constexpr size_t kMax = 22;
    if (stem.size() <= kMax) {
        std::snprintf(out, out_len, "%s", stem.c_str());
        return;
    }
    std::snprintf(out, out_len, "%.19s...", stem.c_str());
}

void ApplyReaderLayoutLive(bool reload_font) {
    auto& st = State();
    if (st.opening.load() || !st.session || !st.session->IsOpen()) {
        return;
    }
    if (st.viewport_w <= 0 || st.viewport_h <= 0) {
        return;
    }
    const bool layout_running = st.layout_busy.load();
    // 换字库与 worker 抢 font_：只记 again；边距/行距仍即时刷预览
    if (layout_running && reload_font) {
        st.layout_again = true;
        st.layout_reload_font = true;
        return;
    }
    if (layout_running) {
        st.layout_again = true;
    }
    // 章节页表后台正用 font_ 计宽：禁止 ReleaseBookFont，否则 InstrFetchProhibited
    if (reload_font && st.chapter_pages_busy.load()) {
        st.layout_again = true;
        st.layout_reload_font = true;
        st.chapter_pages_again = true;
        st.session->InvalidateChapterPageTable();
        return;
    }

    s_reader_page_delta.store(0, std::memory_order_release);
    if (reload_font) {
        ReleaseBookFont();
        EnsureBookFont();
    }
    BindSessionLayoutPrefs(*st.session);
    ApplyReaderPageGeometry(ReaderGeomMode::kImmersive);
    st.session->SetViewport(st.viewport_w, st.viewport_h);

    reader::BookSession::LayoutAnchor anchor = st.session->CaptureLayoutAnchor();
    if (!st.session->BeginLiveRelayout(&anchor)) {
        ESP_LOGW(TAG, "live relayout failed, fallback reopen");
        st.settings_resume_after_open = (st.read_chrome == BookUiState::ReadChrome::kSettings);
        ReopenReaderAfterPrefsChange();
        return;
    }

    const bool keep_sheet = (st.read_chrome == BookUiState::ReadChrome::kSettings);
    RenderReaderPage();
    if (keep_sheet) {
        EnsureSettingsSheetAfterLayout();
    } else {
        UpdateReadFooter();
    }

    if (st.session->NeedsTxtIndexRebuild() && st.session->LiveRelayoutPending()) {
        st.layout_anchor = anchor;
        if (layout_running) {
            // worker 结束后用最新锚点/参数再全量扫
            st.layout_again = true;
        } else {
            st.layout_again = false;
            st.layout_reload_font = false;
            StartLayoutWorker();
        }
    } else if (st.session->NeedsChapterPageRebuild()) {
        StartChapterPageWorker();
    }
}

void CancelLayoutDebounce() {
    auto& st = State();
    if (st.layout_debounce_timer != nullptr) {
        lv_timer_del(st.layout_debounce_timer);
        st.layout_debounce_timer = nullptr;
    }
    st.layout_debounce_reload_font = false;
}

bool IsLayoutHintBusy() {
    auto& st = State();
    if (st.chapter_pages_busy.load() || st.layout_busy.load()) {
        return true;
    }
    return st.session && st.session->IsOpen() && st.session->LiveRelayoutPending();
}

void StopLayoutHintTimer() {
    auto& st = State();
    if (st.layout_hint_timer != nullptr) {
        lv_timer_del(st.layout_hint_timer);
        st.layout_hint_timer = nullptr;
    }
}

void OnLayoutHintTick(lv_timer_t* /*t*/) {
    auto& st = State();
    if (st.page_label == nullptr || !lv_obj_is_valid(st.page_label)) {
        StopLayoutHintTimer();
        return;
    }
    if (IsLayoutHintBusy()) {
        UpdateReadFooter();
        return;
    }
    if (esp_timer_get_time() < st.layout_hint_done_until_us) {
        UpdateReadFooter();
        return;
    }
    st.layout_hint_done_until_us = 0;
    StopLayoutHintTimer();
    UpdateReadFooter();
}

void EnsureLayoutHintTimer() {
    auto& st = State();
    if (st.layout_hint_timer != nullptr) {
        return;
    }
    st.layout_hint_timer = lv_timer_create(OnLayoutHintTick, kLayoutHintTickMs, nullptr);
}

void MarkLayoutHintBusy() {
    auto& st = State();
    st.layout_hint_done_until_us = 0;
    StopLayoutHintTimer();
    UpdateReadFooter();
}

void MarkLayoutHintDone() {
    auto& st = State();
    st.layout_hint_done_until_us = esp_timer_get_time() + kLayoutHintDoneUs;
    EnsureLayoutHintTimer();
    UpdateReadFooter();
}

void OnLayoutDebounceTimer(lv_timer_t* t) {
    auto& st = State();
    if (st.layout_debounce_timer == t) {
        st.layout_debounce_timer = nullptr;
    }
    // 与 ScreenPaintCoalesce 同形：手指未抬 / 边沿未交完则再等一拍，避免连点中途开排版
    if (TouchUiFingerIsDown() || TouchUiHasPendingEdges()) {
        st.layout_debounce_timer =
            lv_timer_create(OnLayoutDebounceTimer, kLayoutDebounceMs, nullptr);
        if (st.layout_debounce_timer != nullptr) {
            lv_timer_set_repeat_count(st.layout_debounce_timer, 1);
        }
        return;
    }
    const bool reload = st.layout_debounce_reload_font;
    st.layout_debounce_reload_font = false;
    ApplyReaderLayoutLive(reload);
}

void ScheduleLayoutApply(bool reload_font) {
    auto& st = State();
    if (st.opening.load() || !st.session || !st.session->IsOpen()) {
        return;
    }
    // prefs/卡文案已由调用方写好；抬起且停顿后再排版（合并为最后一档）
    st.layout_debounce_reload_font = st.layout_debounce_reload_font || reload_font;
    if (st.layout_debounce_timer != nullptr) {
        lv_timer_reset(st.layout_debounce_timer);
        return;
    }
    st.layout_debounce_timer = lv_timer_create(OnLayoutDebounceTimer, kLayoutDebounceMs, nullptr);
    if (st.layout_debounce_timer != nullptr) {
        lv_timer_set_repeat_count(st.layout_debounce_timer, 1);
    }
}

void FlushLayoutDebounce() {
    auto& st = State();
    if (st.layout_debounce_timer == nullptr) {
        return;
    }
    const bool reload = st.layout_debounce_reload_font;
    CancelLayoutDebounce();
    ApplyReaderLayoutLive(reload);
}

struct LayoutDoneMsg {
    uint32_t token = 0;
    bool ok = false;
};

struct LayoutWorkerArg {
    uint32_t token = 0;
    reader::BookSession::LayoutAnchor anchor{};
};

void AsyncLayoutDone(void* user_data) {
    auto* msg = static_cast<LayoutDoneMsg*>(user_data);
    auto& st = State();
    const uint32_t token = msg->token;
    const bool ok = msg->ok;
    delete msg;

    const bool mine = (token == st.layout_token.load());
    // 无论 token 是否作废，本 worker 已结束，必须清 busy（离开时不再提前清）
    st.layout_busy.store(false);
    // 离开正文时 layout worker 仍在跑：结束后再关 session
    if (st.deferred_cleanup && !st.opening.load() && !st.layout_busy.load() &&
        !st.chapter_pages_busy.load() && !st.page_image_busy.load()) {
        FinishDeferredCleanup();
        return;
    }
    if (!mine || st.content == nullptr || !st.session) {
        return;
    }
    if (!ok) {
        ESP_LOGW(TAG, "layout worker failed");
        if (st.session &&
            (st.session->LiveRelayoutPending() || st.session->NeedsTxtIndexRebuild())) {
            st.session->ClearLivePreview();
            st.settings_resume_after_open = (st.read_chrome == BookUiState::ReadChrome::kSettings);
            ReopenReaderAfterPrefsChange();
        } else if (st.session) {
            UpdateReadFooter();
        }
        return;
    }

    const bool keep_sheet = (st.read_chrome == BookUiState::ReadChrome::kSettings);
    RenderReaderPage();
    if (keep_sheet) {
        EnsureSettingsSheetAfterLayout();
    } else {
        UpdateReadFooter();
    }

    if (st.layout_again) {
        const bool reload = st.layout_reload_font;
        st.layout_again = false;
        st.layout_reload_font = false;
        ApplyReaderLayoutLive(reload);
        return;
    }
    if (!IsLayoutHintBusy()) {
        MarkLayoutHintDone();
    }
}

void LayoutWorker(void* arg) {
    auto* ctx = static_cast<LayoutWorkerArg*>(arg);
    auto& st = State();
    bool ok = false;
    if (st.session && ctx->token == st.layout_token.load()) {
        ok = st.session->FinishTxtRelayout(ctx->anchor);
    }
    auto* done = new LayoutDoneMsg{};
    done->token = ctx->token;
    done->ok = ok;
    if (!ScreenLvAsync(AsyncLayoutDone, done)) {
        delete done;
        st.layout_busy.store(false);
    }
    delete ctx;
    vTaskDelete(nullptr);
}

struct ChapterPagesDoneMsg {
    uint32_t token = 0;
    bool ok = false;
};

void AsyncChapterPagesDone(void* user_data) {
    auto* msg = static_cast<ChapterPagesDoneMsg*>(user_data);
    auto& st = State();
    const uint32_t token = msg->token;
    delete msg;

    const bool mine = (token == st.chapter_pages_token.load());
    st.chapter_pages_busy.store(false);

    if (st.deferred_cleanup && !st.opening.load() && !st.layout_busy.load() &&
        !st.chapter_pages_busy.load() && !st.page_image_busy.load()) {
        FinishDeferredCleanup();
        return;
    }
    if (!mine || st.content == nullptr || !st.session) {
        return;
    }
    if (st.read_chrome == BookUiState::ReadChrome::kSettings) {
        EnsureSettingsSheetAfterLayout();
    } else {
        UpdateReadFooter();
    }
    // 换字号曾因页表 worker 占用 font 而 defer：worker 结束后再落地
    if (st.layout_again) {
        const bool reload = st.layout_reload_font;
        st.layout_again = false;
        st.layout_reload_font = false;
        st.chapter_pages_again = false;
        ApplyReaderLayoutLive(reload);
        return;
    }
    if (st.chapter_pages_again || st.session->NeedsChapterPageRebuild()) {
        st.chapter_pages_again = false;
        StartChapterPageWorker(); // 设置卡内会直接 return，关卡后再建
        return;
    }
    MarkLayoutHintDone();
}

void ChapterPageWorker(void* arg) {
    auto* ctx = static_cast<uint32_t*>(arg);
    const uint32_t token = *ctx;
    delete ctx;
    auto& st = State();
    bool ok = false;
    if (st.session && token == st.chapter_pages_token.load()) {
        ok = st.session->FinishChapterPageTable();
    }
    auto* done = new ChapterPagesDoneMsg{};
    done->token = token;
    done->ok = ok;
    if (!ScreenLvAsync(AsyncChapterPagesDone, done)) {
        delete done;
        st.chapter_pages_busy.store(false);
    }
    vTaskDelete(nullptr);
}

void StartChapterPageWorker() {
    auto& st = State();
    if (!st.session || !st.session->NeedsChapterPageRebuild()) {
        return;
    }
    // 设置浮层只预览当前章；Needs 仍真，关卡 HideReadChrome 再调本函数
    if (st.read_chrome == BookUiState::ReadChrome::kSettings) {
        return;
    }
    if (st.chapter_pages_busy.load()) {
        st.chapter_pages_again = true;
        return;
    }
    const uint32_t token = st.chapter_pages_token.fetch_add(1) + 1;
    st.chapter_pages_busy.store(true);
    st.chapter_pages_again = false;
    MarkLayoutHintBusy();
    auto* ctx = new uint32_t(token);
    BaseType_t created = xTaskCreatePinnedToCoreWithCaps(
        ChapterPageWorker, "book_cpages", kOpenWorkerStack, ctx, tskIDLE_PRIORITY + 1, nullptr, 0,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "book_cpages task failed");
        delete ctx;
        st.chapter_pages_busy.store(false);
        MarkLayoutHintDone();
    }
}

void StartLayoutWorker() {
    auto& st = State();
    if (st.layout_busy.load() || !st.session) {
        return;
    }
    const uint32_t token = st.layout_token.fetch_add(1) + 1;
    st.layout_busy.store(true);
    MarkLayoutHintBusy();
    auto* ctx = new LayoutWorkerArg{};
    ctx->token = token;
    ctx->anchor = st.layout_anchor;
    BaseType_t created = xTaskCreatePinnedToCoreWithCaps(
        LayoutWorker, "book_layout", kOpenWorkerStack, ctx, tskIDLE_PRIORITY + 1, nullptr, 0,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "book_layout task failed");
        delete ctx;
        st.layout_busy.store(false);
        st.session->ClearLivePreview();
        st.settings_resume_after_open = (st.read_chrome == BookUiState::ReadChrome::kSettings);
        ReopenReaderAfterPrefsChange();
    }
}

void ReopenReaderAfterPrefsChange() {
    auto& st = State();
    if (st.opening.load()) {
        return;
    }
    if (st.selected < 0 || st.selected >= static_cast<int>(st.books.size())) {
        return;
    }
    st.layout_token.fetch_add(1);
    st.layout_busy.store(false);
    st.layout_again = false;
    st.layout_reload_font = false;
    st.read_chrome = BookUiState::ReadChrome::kReading;
    SetReadStatusVisible(false);
    SetReadSettingsSheetVisible(false);
    ApplyReaderPageGeometry(ReaderGeomMode::kImmersive);
    if (st.viewport_w <= 0 || st.viewport_h <= 0) {
        ESP_LOGE(TAG, "reopen prefs: invalid viewport %d×%d", static_cast<int>(st.viewport_w),
                 static_cast<int>(st.viewport_h));
        st.settings_resume_after_open = false;
        return;
    }
    const lv_coord_t vw = st.viewport_w;
    const lv_coord_t vh = st.viewport_h;

    s_reader_page_delta.store(0, std::memory_order_release);
    EndReadingTimeTracking();
    st.session.reset();
    ReleaseBookFont();
    EnsureBookFont();
    st.session = std::make_unique<reader::BookSession>();
    BindSessionLayoutPrefs(*st.session);
    st.session->SetViewport(vw, vh);
    StartOpenWorker(st.selected);
}

lv_obj_t* MakeSheetIconBtn(lv_obj_t* parent, const char* txt, lv_event_cb_t cb, bool enabled) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, kSheetIconBtn, kSheetIconBtn);
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, kSheetBorderW, 0);
    lv_obj_set_style_border_color(btn, lv_color_black(), 0);
    lv_obj_set_style_radius(btn, kSheetIconBtn / 2, 0);
    DisableScroll(btn);
    // 始终挂事件，原地 SetSheetBtnEnabled 切换可点态（勿依赖重建）
    HapticAttachClick(btn);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(
        btn, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
    SetSheetBtnEnabled(btn, enabled);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_font(lbl, ListFont(), 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    return btn;
}

void OnSheetTocClicked(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    OpenReadToc();
}

void SyncFontSelectedSize() {
    auto& st = State();
    if (st.font_selected.size() != st.font_entries.size()) {
        st.font_selected.assign(st.font_entries.size(), 0);
    }
}

int FontSelectedCount() {
    SyncFontSelectedSize();
    int n = 0;
    for (uint8_t v : State().font_selected) {
        if (v != 0) {
            ++n;
        }
    }
    return n;
}

bool FontItemSelected(int idx) {
    SyncFontSelectedSize();
    auto& st = State();
    return idx >= 0 && idx < static_cast<int>(st.font_selected.size()) &&
           st.font_selected[static_cast<size_t>(idx)] != 0;
}

void ToggleFontItemSelected(int idx) {
    SyncFontSelectedSize();
    auto& st = State();
    if (idx < 0 || idx >= static_cast<int>(st.font_selected.size())) {
        return;
    }
    st.font_selected[static_cast<size_t>(idx)] =
        st.font_selected[static_cast<size_t>(idx)] ? 0 : 1;
}

void RefreshFontMultiFooter() {
    auto& st = State();
    if (st.font_multi) {
        if (st.sheet_font_multi_bar != nullptr && lv_obj_is_valid(st.sheet_font_multi_bar)) {
            lv_obj_clear_flag(st.sheet_font_multi_bar, LV_OBJ_FLAG_HIDDEN);
        }
        if (st.sheet_font_title != nullptr && lv_obj_is_valid(st.sheet_font_title)) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), Lang::Strings::BOOK_SELECTED_FMT, FontSelectedCount());
            lv_label_set_text(st.sheet_font_title, buf);
            lv_obj_set_style_text_opa(st.sheet_font_title, LV_OPA_COVER, 0);
        }
    } else {
        if (st.sheet_font_multi_bar != nullptr && lv_obj_is_valid(st.sheet_font_multi_bar)) {
            lv_obj_add_flag(st.sheet_font_multi_bar, LV_OBJ_FLAG_HIDDEN);
        }
        if (st.sheet_font_title != nullptr && lv_obj_is_valid(st.sheet_font_title)) {
            lv_label_set_text(st.sheet_font_title, Lang::Strings::BOOK_FONT_TITLE);
            lv_obj_set_style_text_opa(st.sheet_font_title, LV_OPA_70, 0);
        }
    }
}

void ExitFontMultiMode(bool paint) {
    auto& st = State();
    st.font_multi = false;
    st.font_suppress_click_until_us = 0;
    st.font_suppress_click_idx = -1;
    st.font_suppress_next_click = false;
    st.font_selected.assign(st.font_entries.size(), 0);
    if (paint) {
        RequestSettingsSheetPaint();
    } else {
        RefreshFontMultiFooter();
    }
}

void EnterFontMultiModeSelect(int idx) {
    auto& st = State();
    st.font_multi = true;
    st.font_selected.assign(st.font_entries.size(), 0);
    if (idx >= 0 && idx < static_cast<int>(st.font_selected.size())) {
        st.font_selected[static_cast<size_t>(idx)] = 1;
    }
    RequestSettingsSheetPaint();
}

bool DeleteReadFontFile(const std::string& file, std::string& err_out) {
    err_out.clear();
    if (file.empty() || file.size() > 63 || file.find('/') != std::string::npos ||
        file == "." || file == ".." || file.size() < 4 ||
        file.compare(file.size() - 3, 3, ".ef") != 0) {
        err_out = Lang::Strings::BOOK_FONT_NAME_INVALID;
        return false;
    }
    char path[96];
    std::snprintf(path, sizeof(path), "%s/%s", SD_PATH_FONTS, file.c_str());
    if (unlink(path) != 0 && errno != ENOENT) {
        err_out = Lang::Strings::BOOK_DELETE_FAILED;
        return false;
    }
    unlink((std::string(path) + ".tmp").c_str());
    ESP_LOGI(TAG, "deleted font %s", path);
    return true;
}

void OnFontMultiCancel(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    if (State().opening.load()) {
        return;
    }
    ExitFontMultiMode(true);
}

void OnFontMultiSelectAll(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (!st.font_multi || st.font_entries.empty() || st.opening.load()) {
        return;
    }
    SyncFontSelectedSize();
    const int n = static_cast<int>(st.font_entries.size());
    const bool clear = (FontSelectedCount() >= n);
    st.font_selected.assign(static_cast<size_t>(n), clear ? 0 : 1);
    RequestSettingsSheetPaint();
}

void OnFontMultiRemove(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (!st.font_multi || st.opening.load()) {
        return;
    }
    SyncFontSelectedSize();
    if (FontSelectedCount() <= 0) {
        return;
    }

    const std::string cur = BookReaderPrefsFontFile();
    bool touch_cur = false;
    for (int i = 0; i < static_cast<int>(st.font_entries.size()); ++i) {
        if (i < static_cast<int>(st.font_selected.size()) &&
            st.font_selected[static_cast<size_t>(i)] != 0 &&
            st.font_entries[static_cast<size_t>(i)].file == cur) {
            touch_cur = true;
            break;
        }
    }

    // 先卸当前 epdfont 并立刻把 session/正文绑到安全字体，再 unlink。
    // 只 Release 不重绑：正文 style 仍握已释放的 lv_font → InstrFetchProhibited。
    if (touch_cur) {
        if (st.chapter_pages_busy.load() || st.layout_busy.load()) {
            st.layout_token.fetch_add(1);
            st.chapter_pages_token.fetch_add(1);
            st.layout_again = true;
            st.layout_reload_font = true;
            st.chapter_pages_again = true;
            if (st.session) {
                st.session->InvalidateChapterPageTable();
            }
        }
        ReleaseBookFont();
        if (st.session && st.session->IsOpen()) {
            BindSessionLayoutPrefs(*st.session);
            const bool keep_sheet = (st.read_chrome == BookUiState::ReadChrome::kSettings);
            RenderReaderPage();
            if (keep_sheet) {
                EnsureSettingsSheetAfterLayout();
            }
        }
    }

    for (int i = static_cast<int>(st.font_entries.size()) - 1; i >= 0; --i) {
        if (i >= static_cast<int>(st.font_selected.size()) ||
            st.font_selected[static_cast<size_t>(i)] == 0) {
            continue;
        }
        std::string err;
        if (!DeleteReadFontFile(st.font_entries[static_cast<size_t>(i)].file, err)) {
            ESP_LOGW(TAG, "font batch delete fail %s: %s",
                     st.font_entries[static_cast<size_t>(i)].file.c_str(), err.c_str());
            continue;
        }
        st.font_entries.erase(st.font_entries.begin() + i);
    }

    bool need_layout = false;
    auto still_has = [&](const char* file) {
        if (file == nullptr || file[0] == '\0') {
            return false;
        }
        for (const auto& fe : st.font_entries) {
            if (fe.file == file) {
                return true;
            }
        }
        return false;
    };
    if (!still_has(BookReaderPrefsFontFile())) {
        // 还有剩余则切到第一项；全删则保持偏好名，EnsureBookFont 失败时 BookFont→fontpack
        if (!st.font_entries.empty()) {
            BookReaderPrefsSetFontFile(st.font_entries.front().file.c_str());
        }
        need_layout = true;
    } else if (touch_cur) {
        need_layout = true;
    }

    st.font_multi = false;
    st.font_suppress_click_until_us = 0;
    st.font_suppress_click_idx = -1;
    st.font_suppress_next_click = false;
    st.font_selected.assign(st.font_entries.size(), 0);
    ClampFontListPage();
    EnsureFontListPageShowsSelection();
    // 空列表需重建空态；否则原地刷行
    if (st.font_entries.empty() || !SettingsSheetWidgetsReady()) {
        RebuildSettingsSheet();
    } else {
        RequestSettingsSheetPaint();
    }
    if (need_layout) {
        ScheduleLayoutApply(true);
    }
}

void OnSheetFontLongPressed(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    if (State().opening.load()) {
        return;
    }
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const int idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
    auto& st = State();
    if (idx < 0 || idx >= static_cast<int>(st.font_entries.size())) {
        return;
    }
    if (!st.font_multi) {
        st.font_suppress_click_idx = idx;
        st.font_suppress_click_until_us = esp_timer_get_time() + kFontSuppressRowClickUs;
        st.font_suppress_next_click = true;
        EnterFontMultiModeSelect(idx);
        return;
    }
    ToggleFontItemSelected(idx);
    RequestSettingsSheetPaint();
}

void OnSheetFontSelect(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    if (State().opening.load()) {
        return;
    }
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const int idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
    auto& st = State();
    if (idx < 0 || idx >= static_cast<int>(st.font_entries.size())) {
        return;
    }
    // 长按进多选后松手常冒 CLICKED；设置卡刷屏可能超过短时间窗，故优先吃掉下一次同项点击
    if (st.font_suppress_next_click && idx == st.font_suppress_click_idx) {
        st.font_suppress_next_click = false;
        return;
    }
    if (idx == st.font_suppress_click_idx &&
        esp_timer_get_time() < st.font_suppress_click_until_us) {
        return;
    }
    if (st.font_multi) {
        ToggleFontItemSelected(idx);
        RequestSettingsSheetPaint();
        return;
    }
    const char* file = st.font_entries[static_cast<size_t>(idx)].file.c_str();
    if (std::strcmp(BookReaderPrefsFontFile(), file) == 0) {
        return;
    }
    BookReaderPrefsSetFontFile(file);
    EnsureFontListPageShowsSelection();
    RequestSettingsSheetPaint();
    ScheduleLayoutApply(true);
}

void OnSheetFontPagePrev(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (st.font_list_page <= 0) {
        return;
    }
    st.font_list_page -= 1;
    RequestSettingsSheetPaint();
}

void OnSheetFontPageNext(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    auto& st = State();
    if (st.font_list_page + 1 >= FontListPageCount()) {
        return;
    }
    st.font_list_page += 1;
    RequestSettingsSheetPaint();
}

void OnSheetMarginDec(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    if (State().opening.load()) {
        return;
    }
    const int cur = BookReaderPrefsMarginPreset();
    if (cur <= 0) {
        return;
    }
    BookReaderPrefsSetMarginPreset(cur - 1);
    RequestSettingsSheetPaint();
    ScheduleLayoutApply(false);
}

void OnSheetMarginInc(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    if (State().opening.load()) {
        return;
    }
    const int cur = BookReaderPrefsMarginPreset();
    if (cur + 1 >= kBookReaderMarginPresetCount) {
        return;
    }
    BookReaderPrefsSetMarginPreset(cur + 1);
    RequestSettingsSheetPaint();
    ScheduleLayoutApply(false);
}

void OnSheetGapDec(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    if (State().opening.load()) {
        return;
    }
    const int cur = BookReaderPrefsSpacingPreset();
    if (cur <= 0) {
        return;
    }
    BookReaderPrefsSetSpacingPreset(cur - 1);
    RequestSettingsSheetPaint();
    ScheduleLayoutApply(false);
}

void OnSheetGapInc(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    if (State().opening.load()) {
        return;
    }
    const int cur = BookReaderPrefsSpacingPreset();
    if (cur + 1 >= kBookReaderSpacingPresetCount) {
        return;
    }
    BookReaderPrefsSetSpacingPreset(cur + 1);
    RequestSettingsSheetPaint();
    ScheduleLayoutApply(false);
}

void OnSheetProgShow(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    if (BookReaderPrefsHideProgress() == 0) {
        return;
    }
    BookReaderPrefsSetHideProgress(0);
    RebuildSettingsSheet();
    if (State().layout_debounce_timer != nullptr) {
        FlushLayoutDebounce();
    } else {
        ApplyReaderLayoutLive(false);
    }
}

void OnSheetProgHide(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    if (BookReaderPrefsHideProgress() != 0) {
        return;
    }
    BookReaderPrefsSetHideProgress(1);
    RebuildSettingsSheet();
    if (State().layout_debounce_timer != nullptr) {
        FlushLayoutDebounce();
    } else {
        ApplyReaderLayoutLive(false);
    }
}

void OnSheetUnderlineMode(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    const int mode = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (mode < 0 || mode >= kBookReaderUnderlineModeCount) {
        return;
    }
    if (BookReaderPrefsUnderlineMode() == mode) {
        return;
    }
    BookReaderPrefsSetUnderlineMode(mode);
    RebuildSettingsSheet();
    RenderReaderPage();
}

void RebuildSettingsSheet() {
    auto& st = State();
    if (st.settings_sheet == nullptr || !lv_obj_is_valid(st.settings_sheet)) {
        return;
    }
    ClearSettingsSheetWidgetRefs();
    lv_obj_clean(st.settings_sheet);
    lv_obj_set_style_min_height(st.settings_sheet, 0, 0);
    // 字库列表只扫一次；点选/翻页勿每次 SD readdir
    if (st.font_entries.empty()) {
        ScanReadFonts();
    }
    SyncFontSelectedSize();
    ClampFontListPage();
    const int font_i = CurrentFontIndex();
    const int font_n = static_cast<int>(st.font_entries.size());
    const int font_pages = FontListPageCount();
    const int font_page_start = st.font_list_page * kFontListPageSize;

    auto add_title_with_bound = [&](lv_obj_t* parent, const char* title, bool can_dec,
                                    bool can_inc, bool center, lv_obj_t** out_bound) {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, center ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 6, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(row);

        lv_obj_t* t = lv_label_create(row);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_font(t, ItemFont(), 0);
        lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);

        // 始终建 tip，原地 Refresh 显隐 MIN/MAX
        lv_obj_t* tip = lv_label_create(row);
        lv_obj_set_style_text_font(tip, ItemFont(), 0);
        lv_obj_set_style_text_opa(tip, LV_OPA_70, 0);
        lv_obj_clear_flag(tip, LV_OBJ_FLAG_CLICKABLE);
        UpdateSheetBoundTip(tip, can_dec, can_inc);
        if (out_bound != nullptr) {
            *out_bound = tip;
        }
    };

    // 字体：标题行（标题 + 导入 TTF）；多选时标题下多出操作行；底栏 ▲▼ 始终可翻页
    lv_obj_t* font_title_row = lv_obj_create(st.settings_sheet);
    lv_obj_remove_style_all(font_title_row);
    lv_obj_set_width(font_title_row, lv_pct(100));
    lv_obj_set_height(font_title_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(font_title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(font_title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(font_title_row, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(font_title_row);

    lv_obj_t* font_title = lv_label_create(font_title_row);
    lv_label_set_text(font_title, Lang::Strings::BOOK_FONT_TITLE);
    lv_obj_set_style_text_font(font_title, ItemFont(), 0);
    lv_obj_set_style_text_opa(font_title, LV_OPA_70, 0);
    lv_obj_clear_flag(font_title, LV_OBJ_FLAG_CLICKABLE);
    st.sheet_font_title = font_title;

    lv_obj_t* import_btn = lv_obj_create(font_title_row);
    lv_obj_remove_style_all(import_btn);
    lv_obj_set_height(import_btn, 36);
    lv_obj_set_width(import_btn, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(import_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_hor(import_btn, 4, 0);
    lv_obj_set_flex_flow(import_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(import_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DisableScroll(import_btn);
    lv_obj_add_flag(import_btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(import_btn);
    lv_obj_add_event_cb(import_btn, OnFontImportClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(
        import_btn, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* import_wrap = lv_obj_create(import_btn);
    lv_obj_remove_style_all(import_wrap);
    lv_obj_set_size(import_wrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_border_side(import_wrap, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(import_wrap, kShelfUnderlineH, 0);
    lv_obj_set_style_border_color(import_wrap, lv_color_black(), 0);
    lv_obj_set_style_pad_bottom(import_wrap, 2, 0);
    lv_obj_set_style_pad_hor(import_wrap, kShelfUnderlinePadHor, 0);
    lv_obj_set_style_bg_opa(import_wrap, LV_OPA_TRANSP, 0);
    DisableScroll(import_wrap);
    lv_obj_clear_flag(import_wrap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* import_lbl = lv_label_create(import_wrap);
    lv_label_set_text(import_lbl, "导入 TTF");
    lv_obj_set_style_text_font(import_lbl, ItemFont(), 0);
    lv_obj_set_style_text_color(import_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(import_lbl, LV_OBJ_FLAG_CLICKABLE);
    st.sheet_font_import = import_btn;

    st.sheet_font_multi_bar = lv_obj_create(st.settings_sheet);
    lv_obj_remove_style_all(st.sheet_font_multi_bar);
    lv_obj_set_width(st.sheet_font_multi_bar, lv_pct(100));
    lv_obj_set_height(st.sheet_font_multi_bar, 36);
    lv_obj_set_style_bg_opa(st.sheet_font_multi_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(st.sheet_font_multi_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(st.sheet_font_multi_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(st.sheet_font_multi_bar, 10, 0);
    DisableScroll(st.sheet_font_multi_bar);
    lv_obj_clear_flag(st.sheet_font_multi_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(st.sheet_font_multi_bar, LV_OBJ_FLAG_HIDDEN);

    auto make_font_multi_action = [](lv_obj_t* parent, const char* text, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_obj_create(parent);
        lv_obj_remove_style_all(btn);
        lv_obj_set_height(btn, 36);
        lv_obj_set_width(btn, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_hor(btn, 4, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        DisableScroll(btn);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(
            btn, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* text_wrap = lv_obj_create(btn);
        lv_obj_remove_style_all(text_wrap);
        lv_obj_set_size(text_wrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_border_side(text_wrap, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(text_wrap, kShelfUnderlineH, 0);
        lv_obj_set_style_border_color(text_wrap, lv_color_black(), 0);
        lv_obj_set_style_pad_bottom(text_wrap, 2, 0);
        lv_obj_set_style_pad_hor(text_wrap, kShelfUnderlinePadHor, 0);
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
    auto make_font_multi_dot = [](lv_obj_t* parent) {
        lv_obj_t* dot = lv_obj_create(parent);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, kShelfMultiDotSize, kShelfMultiDotSize);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        DisableScroll(dot);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        return dot;
    };
    make_font_multi_action(st.sheet_font_multi_bar, Lang::Strings::COMMON_CANCEL, OnFontMultiCancel);
    make_font_multi_dot(st.sheet_font_multi_bar);
    make_font_multi_action(st.sheet_font_multi_bar, Lang::Strings::COMMON_SELECT_ALL, OnFontMultiSelectAll);
    make_font_multi_dot(st.sheet_font_multi_bar);
    make_font_multi_action(st.sheet_font_multi_bar, Lang::Strings::COMMON_REMOVE, OnFontMultiRemove);

    constexpr lv_coord_t kFontListH = kFontListRowH * kFontListPageSize;
    lv_obj_t* font_panel = lv_obj_create(st.settings_sheet);
    lv_obj_remove_style_all(font_panel);
    lv_obj_set_width(font_panel, lv_pct(100));
    lv_obj_set_height(font_panel, kFontListH + 40);  // 列表 + 翻页条
    lv_obj_set_style_bg_color(font_panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(font_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(font_panel, kSheetBorderW, 0);
    lv_obj_set_style_border_color(font_panel, lv_color_black(), 0);
    lv_obj_set_style_radius(font_panel, 12, 0);
    lv_obj_set_flex_flow(font_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(font_panel, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(font_panel);

    lv_obj_t* font_list = lv_obj_create(font_panel);
    lv_obj_remove_style_all(font_list);
    lv_obj_set_width(font_list, lv_pct(100));
    lv_obj_set_height(font_list, kFontListH);
    lv_obj_set_flex_flow(font_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(font_list, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(font_list);

    if (font_n <= 0) {
        lv_obj_t* empty = lv_label_create(font_list);
        lv_label_set_text(empty, Lang::Strings::BOOK_FONT_EMPTY);
        lv_obj_set_style_text_font(empty, ItemFont(), 0);
        lv_obj_set_style_pad_all(empty, 12, 0);
        lv_obj_clear_flag(empty, LV_OBJ_FLAG_CLICKABLE);
    } else {
        for (int i = 0; i < kFontListPageSize; ++i) {
            const int idx = font_page_start + i;
            lv_obj_t* row = lv_obj_create(font_list);
            lv_obj_remove_style_all(row);
            lv_obj_set_width(row, lv_pct(100));
            lv_obj_set_height(row, kFontListRowH);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_hor(row, 12, 0);
            lv_obj_set_style_pad_column(row, 8, 0);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            DisableScroll(row);

            lv_obj_t* name = lv_label_create(row);
            lv_obj_set_flex_grow(name, 1);
            lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
            lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* px = lv_label_create(row);
            lv_obj_set_style_text_font(px, ItemFont(), 0);
            lv_obj_clear_flag(px, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* check = lv_obj_create(row);
            lv_obj_remove_style_all(check);
            lv_obj_set_size(check, kShelfCheckSize, kShelfCheckSize);
            lv_obj_set_style_bg_color(check, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(check, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(check, lv_color_black(), 0);
            lv_obj_set_style_border_width(check, kRowBorderW, 0);
            lv_obj_set_style_radius(check, 4, 0);
            DisableScroll(check);
            lv_obj_clear_flag(check, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(check, LV_OBJ_FLAG_HIDDEN);

            st.sheet_font_rows[i].row = row;
            st.sheet_font_rows[i].name = name;
            st.sheet_font_rows[i].px = px;
            st.sheet_font_rows[i].check = check;

            // 始终挂选中；空槽由 Refresh / user_data=-1 拒点
            HapticAttachClick(row);
            lv_obj_add_event_cb(row, OnSheetFontSelect, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(row, OnSheetFontLongPressed, LV_EVENT_LONG_PRESSED, nullptr);
            lv_obj_add_event_cb(
                row, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            if (idx >= font_n) {
                lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_user_data(row, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
                lv_label_set_text(name, "");
                lv_label_set_text(px, "");
                continue;
            }

            const ReadFontEntry& fe = st.font_entries[static_cast<size_t>(idx)];
            const bool on = (!st.font_multi && idx == font_i);
            const bool next_on = (!st.font_multi) && (i + 1 < kFontListPageSize) &&
                                 (font_page_start + i + 1 == font_i) &&
                                 (font_page_start + i + 1 < font_n);
            // 选中：上下加粗横线（同目录当前章）；上一行勿叠细底线
            if (on) {
                lv_obj_set_style_border_width(row, kTocLineCur, 0);
                lv_obj_set_style_border_side(
                    row, static_cast<lv_border_side_t>(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM),
                    0);
            } else if (next_on) {
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_border_side(row, LV_BORDER_SIDE_NONE, 0);
            } else if (i + 1 < kFontListPageSize) {
                lv_obj_set_style_border_width(row, kTocLineThin, 0);
                lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
            } else {
                lv_obj_set_style_border_width(row, 0, 0);
            }
            lv_obj_set_style_border_color(row, lv_color_black(), 0);
            lv_obj_set_style_border_opa(row, LV_OPA_COVER, 0);

            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_user_data(row, reinterpret_cast<void*>(static_cast<intptr_t>(idx)));

            char name_buf[40];
            FormatFontListName(fe.file, name_buf, sizeof(name_buf));
            lv_label_set_text(name, name_buf);
            lv_obj_set_style_text_font(name, on ? ListFont() : ItemFont(), 0);

            char px_buf[16];
            if (fe.size_px > 0) {
                std::snprintf(px_buf, sizeof(px_buf), "%dpx", fe.size_px);
            } else {
                std::snprintf(px_buf, sizeof(px_buf), "--");
            }
            lv_label_set_text(px, px_buf);
            lv_obj_set_style_text_opa(px, on ? LV_OPA_COVER : LV_OPA_70, 0);

            if (st.font_multi) {
                lv_obj_clear_flag(check, LV_OBJ_FLAG_HIDDEN);
                if (FontItemSelected(idx)) {
                    lv_obj_t* mark = lv_label_create(check);
                    lv_label_set_text(mark, "√");
                    lv_obj_set_style_text_font(mark, ItemFont(), 0);
                    lv_obj_set_style_text_color(mark, lv_color_black(), 0);
                    lv_obj_center(mark);
                    lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
                }
            }
        }
    }

    lv_obj_t* pager = lv_obj_create(font_panel);
    lv_obj_remove_style_all(pager);
    lv_obj_set_width(pager, lv_pct(100));
    lv_obj_set_height(pager, 40);
    lv_obj_set_style_pad_hor(pager, 8, 0);
    lv_obj_set_style_bg_opa(pager, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pager, kTocLineThin, 0);
    lv_obj_set_style_border_side(pager, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(pager, lv_color_black(), 0);
    lv_obj_set_flex_flow(pager, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pager, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pager, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(pager);

    const bool can_prev = st.font_list_page > 0;
    const bool can_next = st.font_list_page + 1 < font_pages;
    auto make_page_btn = [&](const char* txt, lv_event_cb_t cb, bool enabled) -> lv_obj_t* {
        lv_obj_t* btn = lv_obj_create(pager);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 40, 32);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        DisableScroll(btn);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(
            btn, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
        SetSheetBtnEnabled(btn, enabled);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, txt);
        lv_obj_set_style_text_font(lbl, ItemFont(), 0);
        lv_obj_center(lbl);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        return btn;
    };
    st.sheet_font_page_prev = make_page_btn("▲", OnSheetFontPagePrev, can_prev);
    char page_meta[24];
    std::snprintf(page_meta, sizeof(page_meta), "%d / %d", st.font_list_page + 1, font_pages);
    lv_obj_t* page_lab = lv_label_create(pager);
    lv_label_set_text(page_lab, page_meta);
    lv_obj_set_style_text_font(page_lab, ItemFont(), 0);
    lv_obj_set_style_text_opa(page_lab, LV_OPA_70, 0);
    lv_obj_clear_flag(page_lab, LV_OBJ_FLAG_CLICKABLE);
    st.sheet_font_page_lab = page_lab;
    st.sheet_font_page_next = make_page_btn("▼", OnSheetFontPageNext, can_next);
    RefreshFontMultiFooter();

    lv_obj_t* pair = lv_obj_create(st.settings_sheet);
    lv_obj_remove_style_all(pair);
    lv_obj_set_width(pair, lv_pct(100));
    lv_obj_set_height(pair, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(pair, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(pair, kSheetGap, 0);
    lv_obj_clear_flag(pair, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(pair);

    const int margin_i = BookReaderPrefsMarginPreset();
    const int gap_i = BookReaderPrefsSpacingPreset();

    auto make_adj = [&](const char* title, const char* value, lv_event_cb_t dec, lv_event_cb_t inc,
                        bool can_dec, bool can_inc, lv_obj_t** out_value, lv_obj_t** out_dec,
                        lv_obj_t** out_inc, lv_obj_t** out_bound) {
        lv_obj_t* card = lv_obj_create(pair);
        lv_obj_remove_style_all(card);
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(card, 1);
        lv_obj_set_style_bg_color(card, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, kSheetBorderW, 0);
        lv_obj_set_style_border_color(card, lv_color_black(), 0);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_set_style_pad_row(card, 6, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(card);

        add_title_with_bound(card, title, can_dec, can_inc, true, out_bound);

        lv_obj_t* row = lv_obj_create(card);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, kSheetIconBtn);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(row);
        lv_obj_t* dec_btn = MakeSheetIconBtn(row, "-", dec, can_dec);
        lv_obj_t* v = lv_label_create(row);
        lv_label_set_text(v, value);
        lv_obj_set_style_text_font(v, ListFont(), 0);
        lv_obj_clear_flag(v, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* inc_btn = MakeSheetIconBtn(row, "+", inc, can_inc);
        if (out_value != nullptr) {
            *out_value = v;
        }
        if (out_dec != nullptr) {
            *out_dec = dec_btn;
        }
        if (out_inc != nullptr) {
            *out_inc = inc_btn;
        }
    };
    make_adj(Lang::Strings::BOOK_MARGIN, BookReaderPrefsMarginLabel(margin_i), OnSheetMarginDec, OnSheetMarginInc,
             margin_i > 0, margin_i + 1 < kBookReaderMarginPresetCount, &st.sheet_margin_value,
             &st.sheet_margin_dec, &st.sheet_margin_inc, &st.sheet_margin_bound);
    make_adj(Lang::Strings::BOOK_LINE_GAP, BookReaderPrefsSpacingLabel(gap_i), OnSheetGapDec, OnSheetGapInc, gap_i > 0,
             gap_i + 1 < kBookReaderSpacingPresetCount, &st.sheet_gap_value, &st.sheet_gap_dec,
             &st.sheet_gap_inc, &st.sheet_gap_bound);

    // 边框画在对象内侧：高度/内边距按「描边+垫+钮」对齐，避免选中黑块相对圆角偏移
    constexpr lv_coord_t kSegPad = 2;
    constexpr lv_coord_t kSegBtnH = 32;
    constexpr lv_coord_t kSegBtnW = 68;
    const lv_coord_t seg_h = kSegBtnH + kSegPad * 2 + kSheetBorderW * 2;
    auto make_on_off_row = [&](const char* title, bool on, lv_event_cb_t on_cb, lv_event_cb_t off_cb) {
        lv_obj_t* row = lv_obj_create(st.settings_sheet);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 48);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(row);
        lv_obj_t* lab = lv_label_create(row);
        lv_label_set_text(lab, title);
        lv_obj_set_style_text_font(lab, ItemFont(), 0);
        lv_obj_clear_flag(lab, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* seg = lv_obj_create(row);
        lv_obj_remove_style_all(seg);
        lv_obj_set_size(seg, LV_SIZE_CONTENT, seg_h);
        lv_obj_set_style_bg_color(seg, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(seg, kSheetBorderW, 0);
        lv_obj_set_style_border_color(seg, lv_color_black(), 0);
        lv_obj_set_style_radius(seg, seg_h / 2, 0);
        lv_obj_set_style_pad_all(seg, kSegPad, 0);
        lv_obj_set_style_pad_column(seg, 2, 0);
        lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(seg, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(seg, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(seg);
        auto make_seg = [&](const char* txt, bool selected, lv_event_cb_t cb) {
            lv_obj_t* b = lv_obj_create(seg);
            lv_obj_remove_style_all(b);
            lv_obj_set_size(b, kSegBtnW, kSegBtnH);
            lv_obj_set_style_radius(b, kSegBtnH / 2, 0);
            lv_obj_set_style_bg_color(b, selected ? lv_color_black() : lv_color_white(), 0);
            lv_obj_set_style_bg_opa(b, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
            HapticAttachClick(b);
            lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(
                b, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
            lv_obj_t* l = lv_label_create(b);
            lv_label_set_text(l, txt);
            lv_obj_set_style_text_font(l, ItemFont(), 0);
            lv_obj_set_style_text_color(l, selected ? lv_color_white() : lv_color_black(), 0);
            lv_obj_center(l);
            lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        };
        make_seg(Lang::Strings::COMMON_ON, on, on_cb);
        make_seg(Lang::Strings::COMMON_OFF, !on, off_cb);
    };
    // 含百分比与「当前页/总页」；隐藏时底栏整条不占位
    make_on_off_row(Lang::Strings::BOOK_CHAPTER_PROGRESS, BookReaderPrefsHideProgress() == 0,
                    OnSheetProgShow, OnSheetProgHide);

    // 下划线：实线 / 虚线 / 关
    {
        constexpr lv_coord_t kUlBtnW = 68;
        lv_obj_t* row = lv_obj_create(st.settings_sheet);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 48);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(row);
        lv_obj_t* lab = lv_label_create(row);
        lv_label_set_text(lab, Lang::Strings::BOOK_UNDERLINE);
        lv_obj_set_style_text_font(lab, ItemFont(), 0);
        lv_obj_clear_flag(lab, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* seg = lv_obj_create(row);
        lv_obj_remove_style_all(seg);
        lv_obj_set_size(seg, LV_SIZE_CONTENT, seg_h);
        lv_obj_set_style_bg_color(seg, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(seg, kSheetBorderW, 0);
        lv_obj_set_style_border_color(seg, lv_color_black(), 0);
        lv_obj_set_style_radius(seg, seg_h / 2, 0);
        lv_obj_set_style_pad_all(seg, kSegPad, 0);
        lv_obj_set_style_pad_column(seg, 2, 0);
        lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(seg, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(seg, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(seg);
        const int ul = BookReaderPrefsUnderlineMode();
        auto make_ul = [&](const char* txt, int mode) {
            const bool selected = (ul == mode);
            lv_obj_t* b = lv_obj_create(seg);
            lv_obj_remove_style_all(b);
            lv_obj_set_size(b, kUlBtnW, kSegBtnH);
            lv_obj_set_style_radius(b, kSegBtnH / 2, 0);
            lv_obj_set_style_bg_color(b, selected ? lv_color_black() : lv_color_white(), 0);
            lv_obj_set_style_bg_opa(b, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
            HapticAttachClick(b);
            lv_obj_add_event_cb(b, OnSheetUnderlineMode, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<intptr_t>(mode)));
            lv_obj_add_event_cb(
                b, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
            lv_obj_t* l = lv_label_create(b);
            lv_label_set_text(l, txt);
            lv_obj_set_style_text_font(l, ItemFont(), 0);
            lv_obj_set_style_text_color(l, selected ? lv_color_white() : lv_color_black(), 0);
            lv_obj_center(l);
            lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        };
        make_ul(Lang::Strings::BOOK_UNDERLINE_SOLID, kBookReaderUnderlineSolid);
        make_ul(Lang::Strings::BOOK_UNDERLINE_DASHED, kBookReaderUnderlineDashed);
        make_ul(Lang::Strings::COMMON_OFF, kBookReaderUnderlineOff);
    }

    lv_obj_t* bar = lv_obj_create(st.settings_sheet);
    lv_obj_remove_style_all(bar);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 40);
    lv_obj_set_style_pad_top(bar, 6, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(bar, kSheetBorderW, 0);
    lv_obj_set_style_border_color(bar, lv_color_black(), 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(bar);

    lv_obj_t* toc_btn = lv_obj_create(bar);
    lv_obj_remove_style_all(toc_btn);
    lv_obj_set_size(toc_btn, LV_SIZE_CONTENT, 36);
    lv_obj_set_style_pad_hor(toc_btn, 4, 0);
    lv_obj_set_flex_flow(toc_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(toc_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(toc_btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(toc_btn);
    lv_obj_add_event_cb(toc_btn, OnSheetTocClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(
        toc_btn, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
    DisableScroll(toc_btn);

    // 与清单「刷新」同款：文字底边下划线（两端各外延 5px）
    lv_obj_t* toc_text = lv_obj_create(toc_btn);
    lv_obj_remove_style_all(toc_text);
    lv_obj_set_size(toc_text, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_border_side(toc_text, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(toc_text, 1, 0);
    lv_obj_set_style_border_color(toc_text, lv_color_black(), 0);
    lv_obj_set_style_pad_bottom(toc_text, 2, 0);
    lv_obj_set_style_pad_hor(toc_text, 5, 0);
    lv_obj_set_style_bg_opa(toc_text, LV_OPA_TRANSP, 0);
    DisableScroll(toc_text);
    lv_obj_clear_flag(toc_text, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* toc_lbl = lv_label_create(toc_text);
    lv_label_set_text(toc_lbl, Lang::Strings::BOOK_TOC);
    lv_obj_set_style_text_font(toc_lbl, ItemFont(), 0);
    lv_obj_set_style_text_color(toc_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(toc_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_update_layout(st.settings_sheet);
    if (st.ttf_mode != BookUiState::TtfMode::kNone) {
        TtfEnsurePanel();
    }
}

void OnTocRowClicked(lv_event_t* e) {
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    auto& st = State();
    if (!st.session || !st.session->IsOpen()) {
        return;
    }
    if (!st.session->GoToToc(index)) {
        return;
    }
    st.read_chrome = BookUiState::ReadChrome::kReading;
    RenderReaderPage();
}

void OnReadContentLongPressed(lv_event_t* e) {
    auto& st = State();
    if (st.content == nullptr || lv_event_get_target(e) != st.content) {
        return;
    }
    if (st.opening.load() || !st.session || !st.session->IsOpen()) {
        return;
    }
    if (st.read_chrome != BookUiState::ReadChrome::kReading) {
        return;
    }
    // 重绘堵住 LVGL 时 pr_timestamp 过旧，松手后仍可能冒出 LONG_PRESSED
    if (!TouchUiFingerIsDown()) {
        return;
    }
    st.read_touch.suppress_click = true;
    // 按下已由 HapticAttachClick 早震，长按不再二次震
    ShowReadSettingsSheet();
}

void OnReadContentClicked(lv_event_t* e) {
    auto& st = State();
    if (st.content == nullptr || lv_event_get_target(e) != st.content) {
        return;
    }
    if (st.read_touch.suppress_click) {
        st.read_touch.suppress_click = false;
        return;
    }
    if (st.opening.load() || !st.session || !st.session->IsOpen()) {
        return;
    }
    if (st.read_chrome == BookUiState::ReadChrome::kSettings) {
        // 设置卡片打开时点正文只收起，不翻页
        HideReadChrome();
        return;
    }
    if (st.read_chrome == BookUiState::ReadChrome::kToc) {
        // 目录整页：页内触摸不退出、不翻页；仅盖板 vk 返回
        return;
    }
    // 左 1/3 上一页，右 2/3 下一页；震动由 HapticAttachClick 早震
    lv_point_t pt = {};
    const int dir = (ReadEventPoint(e, &pt) && pt.x < LV_HOR_RES / 3) ? -1 : 1;
    QueueReaderPageTurn(dir);
}

void ResetPointerLongPress() {
    for (lv_indev_t* indev = lv_indev_get_next(nullptr); indev != nullptr;
         indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_reset_long_press(indev);
        }
    }
}

void UpdateReadFooter() {
    auto& st = State();
    if (st.page_label == nullptr) {
        return;
    }
    char buf[112];
    const lv_font_t* footer_font = ListFont();
    const lv_coord_t footer_w = LV_HOR_RES - 16;
    if (!st.session || !st.session->IsOpen()) {
        return;
    }
    const int pct_x10 = st.session->ReadingProgressX10();
    if (st.read_chrome == BookUiState::ReadChrome::kToc) {
        const int n = st.session->TocCount();
        const int pages = std::max(1, (n + st.toc_page_size - 1) / st.toc_page_size);
        std::snprintf(buf, sizeof(buf), Lang::Strings::BOOK_TOC_PAGE_FMT, st.toc_list_page + 1, pages);
    } else {
        // 百分比右侧：全书当前页/总页；旁附编排中 / 完成
        const int cur = st.session->BookCurrentPage() + 1;
        const int total = std::max(1, st.session->BookPageCount());
        char hint[16] = "";
        const bool busy = IsLayoutHintBusy();
        const bool show_done = !busy && st.layout_hint_done_until_us > 0 &&
                               esp_timer_get_time() < st.layout_hint_done_until_us;
        if (busy) {
            std::snprintf(hint, sizeof(hint), " %s", Lang::Strings::BOOK_LAYOUT_BUSY);
        } else if (show_done) {
            std::snprintf(hint, sizeof(hint), " %s", Lang::Strings::BOOK_LAYOUT_DONE);
        }
        char suffix[64];
        std::snprintf(suffix, sizeof(suffix), " · %d.%d%%  %d/%d%s", pct_x10 / 10, pct_x10 % 10, cur,
                      total, hint);
        const lv_coord_t suffix_w = MeasureTextWidth(footer_font, suffix);
        const lv_coord_t title_budget = std::max<lv_coord_t>(0, footer_w - suffix_w);

        std::string title = st.session->CurrentTocTitle();
        if (title.empty()) {
            title = Lang::Strings::BOOK_BODY;
        }
        title = TruncateTextToWidth(title, footer_font, title_budget);
        std::snprintf(buf, sizeof(buf), "%s%s", title.c_str(), suffix);
    }
    lv_label_set_text(st.page_label, buf);
}

void RenderTocList() {
    auto& st = State();
    if (st.content == nullptr || !st.session || !st.session->IsOpen()) {
        return;
    }
    CancelListCoverFill();
    st.open_bar = nullptr;
    st.open_pct_lbl = nullptr;
    SetReadStatusVisible(true);
    SetReadStatusCenterTitle(nullptr);
    lv_obj_clean(st.content);
    // 顶栏盖住正文：书头下移，并与顶栏留一缝，避免封面顶死
    const lv_coord_t top_pad =
        (st.status_h > 0 ? st.status_h : kReadPadTop) + kTocBelowStatus;
    ApplyOverlayListContentPads(top_pad);
    lv_obj_set_style_layout(st.content, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(st.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(st.content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(st.content, kTocHeadListGap, 0);
    DisableScroll(st.content);
    // 目录页不挂正文触摸（不退出/不翻页）；章行自带点击
    DetachReadBodyInput(st.content);
    lv_obj_clear_flag(st.content, LV_OBJ_FLAG_CLICKABLE);

    const reader::BookInfo& info = st.session->Info();
    const bool toc_has_page = (info.format == reader::BookFormat::kTxt);
    const int toc_n = st.session->TocCount();

    // 书头：封面 + 书名/作者（白底黑字，顶栏仍用现有状态栏）
    lv_obj_t* head = lv_obj_create(st.content);
    lv_obj_remove_style_all(head);
    lv_obj_set_width(head, ContentWidth());
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head, kTocHeadGap, 0);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(head);

    lv_obj_t* cover_host = lv_obj_create(head);
    lv_obj_remove_style_all(cover_host);
    lv_obj_set_size(cover_host, kTocCoverW, kTocCoverH);
    lv_obj_set_style_bg_color(cover_host, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cover_host, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(cover_host, kCoverFrameBorder, 0);
    StyleBookCoverFrame(cover_host);
    lv_obj_clear_flag(cover_host, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(cover_host);

    const int toc_inner_w = CoverFrameInner(kTocCoverW);
    const int toc_inner_h = CoverFrameInner(kTocCoverH);
    // 详情已删时复用 detail_cover；否则旁路秒读，无则占位并后台抽内嵌回写 .a2i1。
    if (st.detail_scr == nullptr || !lv_obj_is_valid(st.detail_scr)) {
        if (st.detail_cover.empty() || st.detail_cover.width + 8 < toc_inner_w ||
            st.detail_cover.height + 8 < toc_inner_h) {
            st.detail_cover.Reset();
            book_ui::TryLoadBookDetailSidecar(info, toc_inner_w, toc_inner_h, st.detail_cover);
        }
    }
    if (!st.detail_cover.empty()) {
        st.detail_cover.BindDsc();
        lv_obj_t* img = lv_image_create(cover_host);
        lv_image_set_src(img, &st.detail_cover.dsc);
        lv_obj_center(img);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_t* ph = lv_label_create(cover_host);
        lv_label_set_text(ph, reader::FormatLabel(info.format));
        lv_obj_set_style_text_font(ph, ItemFont(), 0);
        lv_obj_set_style_text_color(ph, lv_color_black(), 0);
        lv_obj_center(ph);
        lv_obj_clear_flag(ph, LV_OBJ_FLAG_CLICKABLE);
        EnqueueListCoverFill(info, toc_inner_w, toc_inner_h, cover_host, &st.detail_cover);
        ScheduleListCoverFill();
    }

    lv_obj_t* meta = lv_obj_create(head);
    lv_obj_remove_style_all(meta);
    lv_obj_set_flex_grow(meta, 1);
    lv_obj_set_height(meta, kTocCoverH); // 与封面同高，便于作者贴底
    lv_obj_set_style_bg_opa(meta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_layout(meta, LV_LAYOUT_NONE, 0);
    lv_obj_clear_flag(meta, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(meta);

    const lv_coord_t meta_w = ContentWidth() - kTocCoverW - kTocHeadGap;
    std::string book_title = st.session->Title();
    if (book_title.empty()) {
        book_title = info.title;
    }
    const lv_font_t* title_font = ListFont();
    const lv_coord_t title_lh =
        (title_font != nullptr && title_font->line_height > 0) ? title_font->line_height : 30;
    lv_obj_t* title_lbl = lv_label_create(meta);
    lv_obj_set_style_text_font(title_lbl, title_font, 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_black(), 0);
    lv_obj_set_size(title_lbl, meta_w, title_lh * 3); // 最多三行，超出省略
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(title_lbl, book_title.c_str());
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, 40); // 相对 meta 顶下移
    lv_obj_clear_flag(title_lbl, LV_OBJ_FLAG_CLICKABLE);

    std::string author = st.session->Author();
    if (author.empty()) {
        author = info.author;
    }
    if (author.empty()) {
        author = Lang::Strings::COMMON_UNKNOWN;
    }
    const lv_font_t* author_font = ItemFont();
    const lv_coord_t author_lh =
        (author_font != nullptr && author_font->line_height > 0) ? author_font->line_height : 25;
    lv_obj_t* author_lbl = lv_label_create(meta);
    lv_obj_set_style_text_font(author_lbl, author_font, 0);
    lv_obj_set_style_text_color(author_lbl, lv_color_black(), 0);
    lv_obj_set_size(author_lbl, meta_w, author_lh);
    lv_label_set_long_mode(author_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(author_lbl, TruncateTextToWidth(author, author_font, meta_w).c_str());
    lv_obj_align(author_lbl, LV_ALIGN_BOTTOM_LEFT, 0, -12); // 相对 meta 底上移 12px
    lv_obj_clear_flag(author_lbl, LV_OBJ_FLAG_CLICKABLE);

    const lv_coord_t body_h = lv_obj_get_height(st.content);
    const lv_coord_t usable = body_h - top_pad - kTocCoverH - kTocHeadListGap;
    st.toc_page_size = std::max(1, static_cast<int>(usable / std::max<lv_coord_t>(1, kTocRowH)));
    // 进入目录时 toc_list_page==-1：按当前章定位（须在算出 page_size 之后）
    if (st.toc_list_page < 0) {
        const int cur0 = st.session->CurrentTocIndex();
        st.toc_list_page =
            (cur0 >= 0 && st.toc_page_size > 0) ? (cur0 / st.toc_page_size) : 0;
    }
    const int page_count =
        toc_n <= 0 ? 1
                   : static_cast<int>((toc_n + st.toc_page_size - 1) / st.toc_page_size);
    if (st.toc_list_page >= page_count) {
        st.toc_list_page = page_count - 1;
    }

    if (toc_n <= 0) {
        lv_obj_t* empty = lv_label_create(st.content);
        lv_obj_set_style_text_font(empty, ListFont(), 0);
        lv_obj_set_style_text_color(empty, lv_color_black(), 0);
        lv_label_set_text(empty, Lang::Strings::BOOK_TOC_EMPTY);
        lv_obj_set_width(empty, ContentWidth());
        lv_obj_clear_flag(empty, LV_OBJ_FLAG_CLICKABLE);
        UpdateReadFooter();
        ResetPointerLongPress();
        return;
    }

    lv_obj_t* list = lv_obj_create(st.content);
    lv_obj_remove_style_all(list);
    lv_obj_set_width(list, ContentWidth());
    lv_obj_set_height(list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list, 0, 0);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(list);

    const int begin = st.toc_list_page * st.toc_page_size;
    const int end = std::min(toc_n, begin + st.toc_page_size);
    const int cur = st.session->CurrentTocIndex();
    const lv_font_t* row_font = ItemFont();
    for (int i = begin; i < end; ++i) {
        std::string title;
        int page_1based = -1;
        if (const reader::TocEntry* ent = st.session->TocAt(i)) {
            title = ent->title;
            if (toc_has_page) {
                page_1based = ent->first_page + 1;
            }
        } else {
            char b[48];
            std::snprintf(b, sizeof(b), Lang::Strings::BOOK_CHAPTER_FMT, i + 1);
            title = b;
        }
        const bool is_cur = (i == cur);
        const bool next_is_cur = (i + 1 == cur);
        const lv_coord_t line_h =
            (row_font != nullptr && row_font->line_height > 0) ? row_font->line_height : 30;
        const lv_coord_t row_h =
            std::max(kTocRowH, static_cast<lv_coord_t>(line_h + kTocRowPad * 2));

        char page_buf[16] = {};
        if (page_1based > 0) {
            std::snprintf(page_buf, sizeof(page_buf), "%d", page_1based);
        }
        const lv_coord_t page_w = MeasureTextWidth(row_font, page_buf);
        const lv_coord_t title_budget =
            std::max<lv_coord_t>(24, ContentWidth() - page_w - (page_w > 0 ? 16 : 0));
        const std::string display_title = TruncateTextToWidth(title, row_font, title_budget);

        lv_obj_t* row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, ContentWidth(), row_h);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        // 当前章：上下横线加粗；上一行若紧挨当前章则不画底线，避免叠细线
        if (is_cur) {
            lv_obj_set_style_border_width(row, kTocLineCur, 0);
            lv_obj_set_style_border_side(
                row, static_cast<lv_border_side_t>(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM), 0);
        } else if (next_is_cur) {
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_NONE, 0);
        } else {
            lv_obj_set_style_border_width(row, kTocLineThin, 0);
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        }
        lv_obj_set_style_border_color(row, lv_color_black(), 0);
        lv_obj_set_style_pad_ver(row, kTocRowPad, 0);
        lv_obj_set_style_layout(row, LV_LAYOUT_NONE, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(row);
        DisableScroll(row);
        lv_obj_add_event_cb(row, OnTocRowClicked, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(
            row,
            [](lv_event_t* ev) { lv_event_stop_bubbling(ev); },
            LV_EVENT_CLICKED, nullptr);

        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, row_font, 0);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_set_size(lbl, title_budget, line_h);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_label_set_text(lbl, display_title.c_str());
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

        if (page_buf[0] != '\0') {
            lv_obj_t* page_lbl = lv_label_create(row);
            lv_obj_set_style_text_font(page_lbl, row_font, 0);
            lv_obj_set_style_text_color(page_lbl, lv_color_black(), 0);
            lv_label_set_text(page_lbl, page_buf);
            lv_obj_align(page_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_obj_clear_flag(page_lbl, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    UpdateReadFooter();
    ResetPointerLongPress();
    SetReadStatusVisible(true);
}

void OnReaderUnderlineDraw(lv_event_t* e) {
    lv_obj_t* label = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (label == nullptr) {
        return;
    }
    const int mode = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(label)));
    if (mode != kBookReaderUnderlineSolid && mode != kBookReaderUnderlineDashed) {
        return;
    }
    lv_layer_t* layer = lv_event_get_layer(e);
    const lv_font_t* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    if (layer == nullptr || font == nullptr || font->line_height <= 0) {
        return;
    }

    lv_area_t coords;
    lv_obj_get_content_coords(label, &coords);
    const lv_coord_t line_h = font->line_height;
    // 相对 LVGL 默认下划线再下移 4px
    const lv_coord_t y_in_line =
        static_cast<lv_coord_t>(line_h - font->base_line - font->underline_position + 4);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_black();
    dsc.width = font->underline_thickness > 0 ? font->underline_thickness : 1;
    dsc.opa = LV_OPA_COVER;
    if (mode == kBookReaderUnderlineDashed) {
        dsc.dash_width = 6;
        dsc.dash_gap = 4;
    }

    const char* txt = lv_label_get_text(label);
    lv_point_t sz = {};
    if (txt != nullptr && txt[0] != '\0') {
        lv_text_get_size(&sz, txt, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }

    const lv_coord_t content_h = lv_area_get_height(&coords);
    const int lines = content_h > 0 ? static_cast<int>((content_h + line_h - 1) / line_h) : 1;
    for (int i = 0; i < lines; ++i) {
        const lv_coord_t y = coords.y1 + i * line_h + y_in_line;
        dsc.p1.x = coords.x1;
        dsc.p1.y = y;
        lv_coord_t x2 = coords.x2;
        if (lines == 1 && sz.x > 0) {
            x2 = coords.x1 + sz.x - 1;
            if (x2 > coords.x2) {
                x2 = coords.x2;
            }
        }
        if (x2 < dsc.p1.x) {
            continue;
        }
        dsc.p2.x = x2;
        dsc.p2.y = y;
        lv_draw_line(layer, &dsc);
    }
}

void AttachReaderUnderline(lv_obj_t* label, int mode) {
    if (label == nullptr || mode == kBookReaderUnderlineOff) {
        return;
    }
    lv_obj_set_user_data(label, reinterpret_cast<void*>(static_cast<intptr_t>(mode)));
    lv_obj_add_event_cb(label, OnReaderUnderlineDraw, LV_EVENT_DRAW_MAIN_END, nullptr);
}

void RenderReaderPage() {
    ApplyReaderPageDelta();
    auto& st = State();
    if (st.content == nullptr || !st.session) {
        return;
    }
    st.open_bar = nullptr;
    st.open_pct_lbl = nullptr;
    if (st.read_chrome == BookUiState::ReadChrome::kToc) {
        RenderTocList();
        return;
    }
    // 设置卡片盖在正文上：重绘正文时保持卡片；其它情况回到沉浸
    const bool settings_overlay = (st.read_chrome == BookUiState::ReadChrome::kSettings);
    if (!settings_overlay) {
        st.read_chrome = BookUiState::ReadChrome::kReading;
        SetReadStatusVisible(false);
        SetReadSettingsSheetVisible(false);
    }
    ApplyReaderPageGeometry(ReaderGeomMode::kImmersive);
    // 目录等会把 content 设成 flex；正文用绝对坐标居中，必须关掉 layout
    lv_obj_set_style_layout(st.content, LV_LAYOUT_NONE, 0);

    const bool page_images = st.session->IsPageImagesMode();
    // 分页视口与当前几何不一致：重排（宽或高任一过期都会让末行挤进底栏 / 行宽偏短）
    if (!page_images) {
        const lv_coord_t vw = st.session->ViewportWidth();
        const lv_coord_t vh = st.session->ViewportHeight();
        if (st.viewport_w > 40 && st.viewport_h > 40 && vw > 40 && vh > 40 &&
            (vw != st.viewport_w || vh != st.viewport_h) && !st.opening.load() &&
            !st.layout_busy.load() && !st.session->LiveRelayoutPending()) {
            ESP_LOGW(TAG, "viewport mismatch session=%dx%d ui=%dx%d → relayout",
                     static_cast<int>(vw), static_cast<int>(vh),
                     static_cast<int>(st.viewport_w), static_cast<int>(st.viewport_h));
            ApplyReaderLayoutLive(false);
            return;
        }
    }

    CancelPageImageLoad();
    lv_obj_clean(st.content);
    DisableScroll(st.content);
    ResetReadTouch();
    AttachReadBodyInput(st.content);

    const reader::Page* page = st.session->CurrentPageData();
    if (page == nullptr) {
        ShowMessage(st.content, Lang::Strings::BOOK_PAGE_LOAD_FAILED);
        if (settings_overlay) {
            SetReadStatusVisible(true);
            SetReadSettingsSheetVisible(true);
        }
        return;
    }

    PrewarmPageFont(page);

    const lv_font_t* book_font = BookFont();
    const lv_coord_t max_w = page_images ? LV_HOR_RES : ReadContentWidth();

    // 按本页实际行宽收栏：行未撑满视口时居中整栏，避免满宽 label 左齐造成右空
    lv_coord_t block_w = 0;
    if (!page_images && book_font != nullptr) {
        for (const auto& item : page->items) {
            if (item.kind != reader::ContentKind::kText || item.text.empty()) {
                continue;
            }
            lv_point_t sz = {};
            lv_text_get_size(&sz, item.text.c_str(), book_font, 0, 0, LV_COORD_MAX,
                             LV_TEXT_FLAG_NONE);
            lv_coord_t w = static_cast<lv_coord_t>(sz.x);
            if (item.para_indent) {
                w = static_cast<lv_coord_t>(w + reader::ParaIndentPadPx(book_font));
            }
            if (w > block_w) {
                block_w = w;
            }
        }
    }
    lv_coord_t col_w = max_w;
    if (!page_images && block_w > 40 && block_w < max_w) {
        col_w = block_w;
    }
    const lv_coord_t col_x =
        page_images ? 0 : static_cast<lv_coord_t>((LV_HOR_RES - col_w) / 2);

    const lv_coord_t content_h = lv_obj_get_height(st.content);
    const lv_coord_t line_gap = st.session->LineGap();
    const lv_coord_t para_gap = st.session->ParaGap();
    lv_obj_t* col = lv_obj_create(st.content);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, col_w, page_images ? content_h : LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, page_images ? 0 : line_gap, 0);
    lv_obj_set_pos(col, col_x, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(col);

    for (const auto& item : page->items) {
        if (item.kind == reader::ContentKind::kText) {
            if (item.text.empty()) {
                continue;
            }
            lv_obj_t* label =
                reader::CreatePageTextLabel(col, item, col_w, book_font, line_gap, para_gap);
            AttachReaderUnderline(label, BookReaderPrefsUnderlineMode());
        } else {
            const lv_coord_t max_h =
                page_images ? content_h : (content_h * 3 / 4);
            const int img_h = max_h > 40 ? max_h : 200;
            lv_obj_t* slot = lv_obj_create(col);
            lv_obj_remove_style_all(slot);
            lv_obj_set_width(slot, col_w);
            lv_obj_set_height(slot, page_images ? content_h : LV_SIZE_CONTENT);
            lv_obj_clear_flag(slot, LV_OBJ_FLAG_CLICKABLE);

            reader::RasterImage cached;
            if (st.session->TryCopyCachedPageImage(item.image_href, cached) && !cached.empty()) {
                auto* heap_img = new reader::RasterImage(std::move(cached));
                AttachPageImageToSlot(slot, heap_img, page_images);
            } else {
                lv_obj_t* label = lv_label_create(slot);
                lv_label_set_text(label, Lang::Strings::BOOK_IMAGE_PLACEHOLDER);
                lv_obj_set_style_text_font(label, book_font, 0);
                lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
                BookUiState::PageImagePending pending;
                pending.href = item.image_href;
                pending.max_w = col_w;
                pending.max_h = img_h;
                pending.slot = slot;
                st.page_image_pending.push_back(std::move(pending));
            }
        }
    }

    StartPageImageWorker();

    UpdateReadFooter();
    // 重绘阻塞期间 tick 仍走：复位长按计时，避免恢复后一次判定就 LONG_PRESSED
    ResetPointerLongPress();
    if (settings_overlay) {
        SetReadStatusVisible(true);
        SetReadSettingsSheetVisible(true);
    }
}

lv_obj_t* CreateReaderScreen(const reader::BookInfo& info) {
    auto& st = State();
    // 硬护栏：layout/open worker 仍用旧 session 时禁止 make_unique 拆掉（UAF）
    if (ReaderWorkersBusy()) {
        ESP_LOGW(TAG, "CreateReaderScreen blocked: worker busy");
        if (st.selected >= 0 && st.selected < static_cast<int>(st.books.size())) {
            return CreateDetailScreen(st.selected);
        }
        return BookScreen::Create();
    }
    if (st.deferred_cleanup) {
        FinishDeferredCleanup();
    }
    // 仅打开正文时加载 SD 字体；界面控件用固件字
    EnsureBookFont();
    // 预扫 SD 字库列表，长按设置卡时不必再等「加载中」
    ScanReadFonts();
    EnsureFontListPageShowsSelection();
    // 换新 session 前停旧 timer/墙钟，避免回调 UAF 或漏写本段秒数
    EndReadingTimeTracking();
    st.session = std::make_unique<reader::BookSession>();
    BindSessionLayoutPrefs(*st.session);
    st.opening_format = info.format;

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, ListFont(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    DisableScroll(scr);

    EpdStatusBar status = ScreenCreateStatusBar(scr);
    st.status_bar = status.bar;
    st.status_overlay = status.overlay;
    st.status_h = status.height;
    st.status_label = status.status_label;
    if (status.status_label) {
        lv_label_set_text(status.status_label, "");
        lv_obj_add_flag(status.status_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (status.notification_label) {
        lv_obj_add_flag(status.notification_label, LV_OBJ_FLAG_HIDDEN);
    }
    // 沉浸阅读：默认藏顶栏，正文占满状态栏区域
    SetReadStatusVisible(false);

    st.title_label = nullptr;

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_HOR_RES, LV_VER_RES - kFooterH);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    DisableScroll(body);
    ResetReadTouch();
    AttachReadBodyInput(body);
    st.content = body;
    st.read_chrome = BookUiState::ReadChrome::kReading;
    st.toc_list_page = 0;

    // 顶部悬浮设置卡片（默认隐藏；页内长按唤出，盖在正文上）
    lv_obj_t* sheet = lv_obj_create(scr);
    st.settings_sheet = sheet;
    lv_obj_remove_style_all(sheet);
    lv_obj_set_width(sheet, LV_HOR_RES - kSheetSidePad * 2);
    lv_obj_set_height(sheet, LV_SIZE_CONTENT);
    lv_obj_align(sheet, LV_ALIGN_TOP_MID, 0, (st.status_h > 0 ? st.status_h : 40) + 10);
    lv_obj_set_style_bg_color(sheet, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sheet, kSheetBorderW, 0);
    lv_obj_set_style_border_color(sheet, lv_color_black(), 0);
    lv_obj_set_style_radius(sheet, kSheetRadius, 0);
    lv_obj_set_style_pad_all(sheet, kSheetInnerPad, 0);
    lv_obj_set_style_pad_row(sheet, kSheetGap, 0);
    lv_obj_set_flex_flow(sheet, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sheet, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    DisableScroll(sheet);
    lv_obj_add_flag(sheet, LV_OBJ_FLAG_CLICKABLE);
    // 点在卡片空白处不穿透到正文（避免误收）
    lv_obj_add_event_cb(
        sheet, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(sheet, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* footer = lv_label_create(scr);
    lv_obj_set_width(footer, LV_HOR_RES - 16);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer, ListFont(), 0);
    lv_obj_set_style_text_color(footer, lv_color_black(), 0);
    lv_label_set_long_mode(footer, LV_LABEL_LONG_CLIP);
    lv_label_set_text(footer, "");
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    DisableScroll(footer);
    st.page_label = footer;

    // 先展示加载页，再后台解析；避免点「开始阅读」后长时间无响应
    ShowOpenProgress(0);
    st.read_scr = scr;
    ApplyReaderPageGeometry(ReaderGeomMode::kImmersive);
    st.session->SetViewport(st.viewport_w, st.viewport_h);
    ScreenSetIsHome(false);
    // factory=ResumeReader：进百问压栈后返回依 .pos 续读；短按 HOME 回详情
    VkKey_AttachScreen(scr, kScreenRead, BookAiLongPressDesc(ResumeReaderScreen));
    StartOpenWorker(st.selected);
    return scr;
}

lv_obj_t* CreateDetailScreen(int book_index) {
    auto& st = State();
    if (book_index < 0 || book_index >= static_cast<int>(st.books.size())) {
        return BookScreen::Create();
    }
    const reader::BookInfo& info = st.books[static_cast<size_t>(book_index)];
    {
        std::string book_id;
        if (info.format == reader::BookFormat::kEbook) {
            (void)reader::EbookDocument::PeekBookId(info.path.c_str(), book_id);
        }
        ESP_LOGI(TAG, "detail book_id=%s title=%s path=%s",
                 book_id.empty() ? "-" : book_id.c_str(), info.title.c_str(),
                 info.path.c_str());
    }
    st.selected = book_index;
    CancelDetailCoverLoad();
    // 旧详情若仍在异步删除，封面由 DELETE 回调释放；此处仅在无存活详情时重置
    if (st.detail_scr == nullptr) {
        st.detail_cover.Reset();
    }
    st.detail_format = info.format;

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, ListFont(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    DisableScroll(scr);
    st.detail_scr = scr;
    lv_obj_add_event_cb(
        scr,
        [](lv_event_t* e) {
            auto& s = State();
            const auto* tgt = static_cast<lv_obj_t*>(lv_event_get_target(e));
            if (s.detail_scr == tgt) {
                s.detail_scr = nullptr;
                s.detail_cover_token.fetch_add(1);
                ClearDetailCoverUiPtrs();
            }
            // 没有更新的详情屏时才释放封面（避免替换瞬间误清）
            if (s.detail_scr == nullptr) {
                s.detail_cover.Reset();
            }
        },
        LV_EVENT_DELETE, nullptr);

    EpdStatusBar status = ScreenCreateStatusBar(scr);
    if (status.status_label) {
        lv_label_set_text(status.status_label, Lang::Strings::BOOK_DETAIL);
    }
    if (status.notification_label) {
        lv_obj_add_flag(status.notification_label, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_HOR_RES, LV_VER_RES - status.height);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_left(body, kListPad, 0);
    lv_obj_set_style_pad_right(body, kListPad, 0);
    lv_obj_set_style_pad_top(body, 10, 0);
    lv_obj_set_style_pad_bottom(body, kDetailCtaBottom, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(body, kDetailSectionGap, 0);
    DisableScroll(body);

    const reader::BookInfo& shown = st.books[static_cast<size_t>(book_index)];
    const auto peek = reader::BookSession::PeekProgress(shown.path.c_str());
    const lv_font_t* title_font = fontpack_lv_font_get(30, 4); // 书名加粗
    if (title_font == nullptr) {
        title_font = ListFont();
    }
    const lv_font_t* item_font = ItemFont();
    const lv_coord_t title_lh =
        (title_font != nullptr && title_font->line_height > 0) ? title_font->line_height : 30;
    const lv_coord_t item_lh =
        (item_font != nullptr && item_font->line_height > 0) ? item_font->line_height : 25;
    const lv_font_t* num_font = fontpack_lv_font_get(30, 4);
    if (num_font == nullptr) {
        num_font = title_font;
    }

    // 上约 3/5 封面槽略收，下区多给底边呼吸
    const lv_coord_t usable_h =
        (LV_VER_RES - status.height) - 10 - kDetailCtaBottom - kDetailSectionGap;
    const lv_coord_t cover_slot_h = usable_h * 3 / 5 - 12;
    const lv_coord_t bottom_h = usable_h - cover_slot_h;
    {
        lv_coord_t cover_h = cover_slot_h;
        lv_coord_t cover_w = cover_h * 3 / 4;
        const lv_coord_t max_w = ContentWidth();
        if (cover_w > max_w) {
            cover_w = max_w;
            cover_h = cover_w * 4 / 3;
            if (cover_h > cover_slot_h) {
                cover_h = cover_slot_h;
                cover_w = cover_h * 3 / 4;
            }
        }
        if (cover_w < 80) {
            cover_w = 80;
        }
        if (cover_h < 100) {
            cover_h = 100;
        }
        st.detail_cover_fw = cover_w;
        st.detail_cover_fh = cover_h;
    }

    lv_obj_t* cover_host = lv_obj_create(body);
    st.detail_cover_host = cover_host;
    lv_obj_remove_style_all(cover_host);
    lv_obj_set_size(cover_host, ContentWidth(), cover_slot_h);
    lv_obj_set_style_bg_opa(cover_host, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cover_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cover_host, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cover_host, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(cover_host);

    const int detail_inner_w = CoverFrameInner(st.detail_cover_fw);
    const int detail_inner_h = CoverFrameInner(st.detail_cover_fh);
    const bool sidecar_adequate =
        reader::BookCoverSidecarAdequate(info.path.c_str(), detail_inner_w, detail_inner_h);
    // 有旁路先出图（哪怕偏小）；不够大再后台升权威档，避免书架看过详情仍干等重解
    const bool preview_ok = book_ui::TryLoadBookSidecarPreview(
        st.books[static_cast<size_t>(book_index)], detail_inner_w, detail_inner_h, st.detail_cover);
    if (preview_ok && !st.detail_cover.empty()) {
        ApplyDetailCoverImage();
    } else {
        st.detail_cover.Reset();
        FillDetailCoverPlaceholder(cover_host, info.format);
    }
    if (!sidecar_adequate &&
        (info.format == reader::BookFormat::kEpub || info.format == reader::BookFormat::kEbook) &&
        !IsCoverEmbedMiss(info.path)) {
        StartDetailCoverWorker(book_index);
    }

    // 下 2/5：书名 / 作者 / 格式 / 时长 / 继续阅读
    lv_obj_t* bottom = lv_obj_create(body);
    lv_obj_remove_style_all(bottom);
    lv_obj_set_size(bottom, ContentWidth(), bottom_h);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(bottom, 6, 0);
    lv_obj_clear_flag(bottom, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(bottom);

    lv_obj_t* title = lv_label_create(bottom);
    st.detail_title_lbl = title;
    lv_label_set_text(title, shown.title.c_str());
    lv_obj_set_size(title, ContentWidth(), title_lh * kDetailTitleLines);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title, title_font, 0);
    DisableScroll(title);

    lv_obj_t* author = lv_label_create(bottom);
    st.detail_meta_lbl = author;
    lv_label_set_text(author, shown.author.empty() ? Lang::Strings::COMMON_UNKNOWN : shown.author.c_str());
    lv_obj_set_size(author, ContentWidth(), item_lh);
    lv_label_set_long_mode(author, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(author, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(author, item_font, 0);
    lv_obj_set_style_text_color(author, lv_color_hex(0x666666), 0);
    DisableScroll(author);

    {
        lv_obj_t* meta = lv_obj_create(bottom);
        lv_obj_remove_style_all(meta);
        lv_obj_set_size(meta, ContentWidth(), item_lh + 20);
        lv_obj_set_style_layout(meta, LV_LAYOUT_NONE, 0);
        lv_obj_clear_flag(meta, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(meta);

        lv_obj_t* fmt = lv_label_create(meta);
        lv_label_set_text(fmt, reader::FormatLabel(shown.format));
        lv_obj_set_style_text_font(fmt, item_font, 0);
        lv_obj_align(fmt, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_clear_flag(fmt, LV_OBJ_FLAG_CLICKABLE);

        char size_buf[16];
        reader::FormatFileSize(size_buf, sizeof(size_buf), shown.file_size);
        lv_obj_t* sz = lv_label_create(meta);
        lv_label_set_text(sz, size_buf);
        lv_obj_set_style_text_font(sz, item_font, 0);
        lv_obj_align(sz, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_clear_flag(sz, LV_OBJ_FLAG_CLICKABLE);

        // 双分界线：上粗(4) 下细(2)
        lv_obj_t* line_bot = lv_obj_create(meta);
        lv_obj_remove_style_all(line_bot);
        lv_obj_set_size(line_bot, ContentWidth(), 2);
        lv_obj_set_style_bg_color(line_bot, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(line_bot, LV_OPA_COVER, 0);
        lv_obj_align(line_bot, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_clear_flag(line_bot, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(line_bot);

        lv_obj_t* line_top = lv_obj_create(meta);
        lv_obj_remove_style_all(line_top);
        lv_obj_set_size(line_top, ContentWidth(), 4);
        lv_obj_set_style_bg_color(line_top, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(line_top, LV_OPA_COVER, 0);
        lv_obj_align(line_top, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_obj_clear_flag(line_top, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(line_top);
    }

    {
        lv_obj_t* stats = lv_obj_create(bottom);
        lv_obj_remove_style_all(stats);
        lv_obj_set_size(stats, ContentWidth(), item_lh + 4 + 34);
        lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(stats, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(stats, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(stats);

        auto make_stat = [&](const char* key, uint32_t seconds) {
            lv_obj_t* col = lv_obj_create(stats);
            lv_obj_remove_style_all(col);
            lv_obj_set_width(col, (ContentWidth() - 20) / 2);
            lv_obj_set_height(col, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(col, 4, 0);
            lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);
            DisableScroll(col);
            lv_obj_t* k = lv_label_create(col);
            lv_label_set_text(k, key);
            lv_obj_set_style_text_font(k, item_font, 0);
            lv_obj_set_style_text_color(k, lv_color_hex(0x666666), 0);
            lv_obj_clear_flag(k, LV_OBJ_FLAG_CLICKABLE);
            char val[24];
            FormatHomeDurationShort(val, sizeof(val), seconds);
            lv_obj_t* v = lv_label_create(col);
            lv_label_set_text(v, val);
            lv_obj_set_style_text_font(v, num_font, 0);
            lv_obj_clear_flag(v, LV_OBJ_FLAG_CLICKABLE);
        };
        make_stat(Lang::Strings::BOOK_TODAY_DURATION, peek.daily_seconds);
        lv_obj_t* sep = lv_obj_create(stats);
        lv_obj_remove_style_all(sep);
        lv_obj_set_size(sep, 2, 40);
        lv_obj_set_style_bg_color(sep, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);
        make_stat(Lang::Strings::BOOK_READ_DURATION, peek.reading_seconds);
    }

    {
        const int px10 = peek.progress_x10 >= 0 ? peek.progress_x10 : 0;
        const int bar_pct = px10 / 10;

        lv_obj_t* start_btn = lv_obj_create(bottom);
        lv_obj_remove_style_all(start_btn);
        lv_obj_set_size(start_btn, ContentWidth(), kHomeShelfBtnH);
        lv_obj_set_style_bg_color(start_btn, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(start_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(start_btn, 14, 0);
        lv_obj_set_style_pad_hor(start_btn, 16, 0);
        lv_obj_set_flex_flow(start_btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(start_btn, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(start_btn, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(start_btn);
        HapticAttachClick(start_btn);
        lv_obj_add_event_cb(start_btn, OnDetailStartClicked, LV_EVENT_CLICKED, nullptr);

        constexpr lv_coord_t kCtaBarH = 16;
        constexpr lv_coord_t kCtaBarInset = 3; // 白底与黑填细间隙（墨水屏 2px 易糊）
        const lv_coord_t cta_bar_w = ContentWidth() / 2; // 略短，避免压到「继续阅读」
        lv_obj_t* bar_track = lv_obj_create(start_btn);
        lv_obj_remove_style_all(bar_track);
        lv_obj_set_size(bar_track, cta_bar_w, kCtaBarH);
        lv_obj_set_style_bg_color(bar_track, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(bar_track, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar_track, kCtaBarH / 2, 0);
        lv_obj_set_style_clip_corner(bar_track, true, 0);
        lv_obj_set_style_pad_all(bar_track, 0, 0);
        lv_obj_set_style_layout(bar_track, LV_LAYOUT_NONE, 0);
        lv_obj_clear_flag(bar_track, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(bar_track, LV_OBJ_FLAG_SCROLLABLE);
        DisableScroll(bar_track);

        if (bar_pct > 0) {
            const lv_coord_t inner_h = kCtaBarH - 2 * kCtaBarInset;
            lv_coord_t fg_w =
                static_cast<lv_coord_t>((cta_bar_w - 2 * kCtaBarInset) * bar_pct / 100);
            if (fg_w < inner_h) {
                fg_w = inner_h;
            }
            if (fg_w > cta_bar_w - 2 * kCtaBarInset) {
                fg_w = cta_bar_w - 2 * kCtaBarInset;
            }
            lv_obj_t* bar_fg = lv_obj_create(bar_track);
            lv_obj_remove_style_all(bar_fg);
            lv_obj_set_size(bar_fg, fg_w, inner_h);
            lv_obj_set_pos(bar_fg, kCtaBarInset, kCtaBarInset);
            lv_obj_set_style_bg_color(bar_fg, lv_color_black(), 0);
            lv_obj_set_style_bg_opa(bar_fg, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(bar_fg, inner_h / 2, 0);
            lv_obj_clear_flag(bar_fg, LV_OBJ_FLAG_CLICKABLE);
            DisableScroll(bar_fg);
        }

        lv_obj_t* cta_row = lv_obj_create(start_btn);
        lv_obj_remove_style_all(cta_row);
        lv_obj_set_size(cta_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(cta_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cta_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(cta_row, 8, 0);
        lv_obj_clear_flag(cta_row, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(cta_row);

        lv_obj_t* start_lbl = lv_label_create(cta_row);
        lv_label_set_text(start_lbl,
                          peek.progress_x10 >= 0 ? Lang::Strings::BOOK_CONTINUE
                                                : Lang::Strings::BOOK_START);
        lv_obj_set_style_text_font(start_lbl, ListFont(), 0);
        lv_obj_set_style_text_color(start_lbl, lv_color_white(), 0);
        lv_obj_clear_flag(start_lbl, LV_OBJ_FLAG_CLICKABLE);

        // 三角边长约等于「读」字高（ListFont ~30）
        const lv_coord_t play_h = title_lh > 0 ? title_lh : 28;
        const lv_coord_t play_w = play_h * 7 / 8;
        lv_obj_t* play = lv_obj_create(cta_row);
        lv_obj_remove_style_all(play);
        lv_obj_set_size(play, play_w, play_h);
        lv_obj_set_style_bg_opa(play, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(play, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(play);
        lv_obj_add_event_cb(
            play,
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) {
                    return;
                }
                lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
                lv_layer_t* layer = lv_event_get_layer(e);
                if (obj == nullptr || layer == nullptr) {
                    return;
                }
                lv_area_t a;
                lv_obj_get_content_coords(obj, &a);
                lv_draw_triangle_dsc_t tri;
                lv_draw_triangle_dsc_init(&tri);
                tri.color = lv_color_white();
                tri.opa = LV_OPA_COVER;
                tri.p[0].x = a.x1;
                tri.p[0].y = a.y1 + 1;
                tri.p[1].x = a.x1;
                tri.p[1].y = a.y2 - 1;
                tri.p[2].x = a.x2;
                tri.p[2].y = (a.y1 + a.y2) / 2;
                lv_draw_triangle(layer, &tri);
            },
            LV_EVENT_DRAW_MAIN, nullptr);
    }

    ScreenSetIsHome(false);
    // factory=ResumeDetail：进百问后返回当前书详情
    VkKey_AttachScreen(scr, kScreenDetail, BookAiLongPressDesc(ResumeDetailScreen));
    return scr;
}

/** 封面槽：同步旁路 .a2i1 或格式占位；无旁路时由调用方排队后台抽内嵌 */
lv_obj_t* CreateCoverSlot(lv_obj_t* parent, const reader::BookInfo& info, lv_coord_t w, lv_coord_t h,
                          reader::RasterImage* cover_out, bool* need_embed_fill) {
    if (need_embed_fill != nullptr) {
        *need_embed_fill = false;
    }
    lv_obj_t* host = lv_obj_create(parent);
    lv_obj_remove_style_all(host);
    lv_obj_set_size(host, w, h);
    lv_obj_set_style_pad_all(host, kCoverFrameBorder, 0);
    lv_obj_set_style_bg_color(host, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(host, LV_OPA_COVER, 0);
    StyleBookCoverFrame(host);
    lv_obj_clear_flag(host, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(host);

    const int inner_w = CoverFrameInner(w);
    const int inner_h = CoverFrameInner(h);
    if (cover_out != nullptr &&
        book_ui::TryLoadBookDetailSidecar(info, inner_w, inner_h, *cover_out) &&
        !cover_out->empty()) {
        cover_out->BindDsc();
        lv_obj_t* img = lv_image_create(host);
        lv_image_set_src(img, &cover_out->dsc);
        lv_obj_center(img);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    } else {
        if (cover_out != nullptr) {
            cover_out->Reset();
        }
        lv_obj_t* ph_lbl = lv_label_create(host);
        lv_label_set_text(ph_lbl, reader::FormatLabel(info.format));
        lv_obj_set_style_text_font(ph_lbl, ListFont(), 0);
        lv_obj_set_style_text_color(ph_lbl, lv_color_black(), 0);
        lv_obj_center(ph_lbl);
        lv_obj_clear_flag(ph_lbl, LV_OBJ_FLAG_CLICKABLE);
        if (need_embed_fill != nullptr &&
            (info.format == reader::BookFormat::kEpub ||
             info.format == reader::BookFormat::kEbook)) {
            *need_embed_fill = true;
        }
    }
    return host;
}

/** 书架 / 最近阅读：封面 + 可选书名 */
lv_obj_t* CreateBookCoverCell(lv_obj_t* parent, const reader::BookInfo& info, int index,
                              lv_coord_t cell_w, lv_coord_t cover_h,
                              std::vector<std::unique_ptr<reader::RasterImage>>& cover_store,
                              bool long_press_delete, lv_coord_t title_gap, bool show_title) {
    auto& st = State();
    lv_obj_t* cell = lv_obj_create(parent);
    lv_obj_remove_style_all(cell);
    lv_obj_set_width(cell, cell_w);
    lv_obj_set_height(cell, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cell, show_title ? title_gap : 0, 0);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(cell);
    lv_obj_set_user_data(cell, reinterpret_cast<void*>(static_cast<intptr_t>(index)));
    HapticAttachClick(cell);
    lv_obj_add_event_cb(cell, OnBookRowClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));
    if (long_press_delete) {
        lv_obj_add_event_cb(cell, OnBookRowLongPressed, LV_EVENT_LONG_PRESSED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(index)));
    }

    auto cover = std::make_unique<reader::RasterImage>();
    bool need_embed_fill = false;
    lv_obj_t* host =
        CreateCoverSlot(cell, info, cell_w, cover_h, cover.get(), &need_embed_fill);
    cover_store.push_back(std::move(cover));
    if (need_embed_fill) {
        EnqueueListCoverFill(info, CoverFrameInner(cell_w), CoverFrameInner(cover_h), host,
                             cover_store.back().get());
    }

    if (long_press_delete && st.shelf_multi) {
        lv_obj_t* check = lv_obj_create(host);
        lv_obj_remove_style_all(check);
        lv_obj_set_size(check, kShelfCheckSize, kShelfCheckSize);
        lv_obj_set_style_bg_color(check, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(check, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(check, lv_color_black(), 0);
        lv_obj_set_style_border_width(check, kRowBorderW, 0);
        lv_obj_set_style_radius(check, 4, 0);
        lv_obj_align(check, LV_ALIGN_TOP_RIGHT, -4, 4);
        DisableScroll(check);
        lv_obj_clear_flag(check, LV_OBJ_FLAG_CLICKABLE);
        if (ShelfItemSelected(index)) {
            lv_obj_t* mark = lv_label_create(check);
            lv_label_set_text(mark, "√");
            lv_obj_set_style_text_font(mark, ItemFont(), 0);
            lv_obj_set_style_text_color(mark, lv_color_black(), 0);
            lv_obj_center(mark);
            lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    if (show_title) {
        const lv_font_t* item_font = ItemFont();
        lv_obj_t* title = lv_label_create(cell);
        lv_obj_set_width(title, cell_w);
        lv_obj_set_style_text_font(title, item_font, 0);
        lv_obj_set_style_text_color(title, lv_color_black(), 0);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        const std::string clipped = TruncateTextToWidth(info.title, item_font, cell_w);
        lv_label_set_text(title, clipped.c_str());
        lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);
    }
    return cell;
}

void DrawHomeBarChart(lv_obj_t* parent, lv_coord_t area_w, lv_coord_t area_h, int finished,
                      int book_count) {
    lv_obj_t* chart = lv_obj_create(parent);
    lv_obj_remove_style_all(chart);
    lv_obj_set_size(chart, area_w, area_h);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(chart, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(chart);

    constexpr int kBars = 6;
    constexpr lv_coord_t kBarW = 12; // 竖条加细
    const lv_coord_t bar_w = kBarW;
    const int seed = std::max(1, book_count) + finished * 3;
    const lv_coord_t heights[kBars] = {
        static_cast<lv_coord_t>(area_h * (35 + (seed * 11) % 45) / 100),
        static_cast<lv_coord_t>(area_h * (50 + (seed * 7) % 40) / 100),
        static_cast<lv_coord_t>(area_h * (70 + (seed * 3) % 25) / 100),
        static_cast<lv_coord_t>(area_h * (45 + (seed * 13) % 40) / 100),
        static_cast<lv_coord_t>(area_h * (55 + (seed * 5) % 35) / 100),
        static_cast<lv_coord_t>(area_h * (40 + (seed * 17) % 40) / 100),
    };
    const lv_coord_t stride = (area_w - bar_w * kBars) / (kBars - 1);
    for (int i = 0; i < kBars; ++i) {
        lv_obj_t* bar = lv_obj_create(chart);
        lv_obj_remove_style_all(bar);
        const lv_coord_t bh = std::max<lv_coord_t>(bar_w + 4, heights[i]);
        lv_obj_set_size(bar, bar_w, bh);
        lv_obj_set_style_bg_color(bar, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar, bar_w / 2, 0); // 顶底圆弧
        lv_obj_set_pos(bar, i * (bar_w + stride), area_h - bh);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(bar);
    }
}

void RenderReadingHome() {
    auto& st = State();
    if (st.list_body == nullptr) {
        return;
    }
    CancelListCoverFill();
    lv_obj_set_style_layout(st.list_body, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(st.list_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(st.list_body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(st.list_body, 0, 0);

    // 先拆控件再释放封面，避免 lv_image 引用失效
    lv_obj_clean(st.list_body);
    st.recent_covers.clear();
    DisableScroll(st.list_body);

    if (st.list_footer != nullptr) {
        lv_label_set_text(st.list_footer, "");
        lv_obj_add_flag(st.list_footer, LV_OBJ_FLAG_HIDDEN);
    }

    if (st.books.empty()) {
        char tip[160];
        std::snprintf(tip, sizeof(tip), Lang::Strings::BOOK_EMPTY_HOME_FMT,
                      SdUserPath(reader::kDefaultBooksDir));
        ShowMessage(st.list_body, tip, ListFont());
        return;
    }

    const HomeStats stats = CollectHomeStats(st.books);
    const lv_coord_t content_w = ContentWidth();

    const lv_font_t* item_f = ItemFont();
    const lv_font_t* list_f = ListFont();
    const lv_font_t* num_font = fontpack_lv_font_get(30, 4);
    if (num_font == nullptr) {
        num_font = list_f;
    }
    const lv_coord_t lh =
        (list_f != nullptr && list_f->line_height > 0) ? list_f->line_height : 30;
    const lv_coord_t ih =
        (item_f != nullptr && item_f->line_height > 0) ? item_f->line_height : 25;
    const lv_coord_t nh =
        (num_font != nullptr && num_font->line_height > 0) ? num_font->line_height : lh;

    // 继续阅读黑卡略抬；余量留给最近阅读封面下单行书名
    constexpr lv_coord_t kContPad = 20;
    constexpr lv_coord_t kContBtnH = 52;
    constexpr lv_coord_t kBarH = 16;
    constexpr lv_coord_t kThumb = 22;
    const lv_coord_t dash_h =
        kContPad * 2 + lh + 16 + std::max(kBarH, kThumb) + 16 + kContBtnH + 36;

    lv_obj_t* dash = lv_obj_create(st.list_body);
    lv_obj_remove_style_all(dash);
    lv_obj_set_size(dash, content_w, dash_h);
    lv_obj_set_style_bg_opa(dash, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(dash, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dash, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dash, kHomeGap, 0);
    lv_obj_clear_flag(dash, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(dash);

    // 继续阅读 / 阅读数据 对半
    const lv_coord_t cont_w = (content_w - kHomeGap) / 2;
    const lv_coord_t stats_w = content_w - kHomeGap - cont_w;

    lv_obj_t* cont = lv_obj_create(dash);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, cont_w, dash_h);
    lv_obj_set_style_bg_color(cont, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cont, kHomeCardRadius, 0);
    lv_obj_set_style_pad_all(cont, kContPad, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(cont);

    char prog_txt[32];
    int bar_pct = 0;
    if (stats.continue_progress_x10 >= 0) {
        std::snprintf(prog_txt, sizeof(prog_txt), Lang::Strings::BOOK_READ_PCT_FMT,
                      stats.continue_progress_x10 / 10, stats.continue_progress_x10 % 10);
        bar_pct = stats.continue_progress_x10 / 10;
    } else if (stats.continue_index >= 0) {
        std::snprintf(prog_txt, sizeof(prog_txt), "%s", Lang::Strings::BOOK_START);
    } else {
        std::snprintf(prog_txt, sizeof(prog_txt), "%s", Lang::Strings::BOOK_NO_CONTINUE);
    }

    // 百分比 + 进度条成组贴顶，避免 SPACE_BETWEEN 把条甩到中间
    lv_obj_t* top_block = lv_obj_create(cont);
    lv_obj_remove_style_all(top_block);
    lv_obj_set_width(top_block, cont_w - kContPad * 2);
    lv_obj_set_height(top_block, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(top_block, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(top_block, 8, 0);
    lv_obj_clear_flag(top_block, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(top_block);

    lv_obj_t* cont_sub = lv_label_create(top_block);
    lv_label_set_text(cont_sub, prog_txt);
    lv_obj_set_style_text_font(cont_sub, list_f, 0);
    lv_obj_set_style_text_color(cont_sub, lv_color_white(), 0);
    lv_obj_set_width(cont_sub, cont_w - kContPad * 2);
    lv_obj_clear_flag(cont_sub, LV_OBJ_FLAG_CLICKABLE);

    const lv_coord_t bar_w = cont_w - kContPad * 2;
    lv_obj_t* bar_wrap = lv_obj_create(top_block);
    lv_obj_remove_style_all(bar_wrap);
    lv_obj_set_size(bar_wrap, bar_w, std::max(kBarH, kThumb));
    lv_obj_set_style_bg_opa(bar_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_layout(bar_wrap, LV_LAYOUT_NONE, 0);
    lv_obj_clear_flag(bar_wrap, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(bar_wrap);

    lv_obj_t* bar_track = lv_obj_create(bar_wrap);
    lv_obj_remove_style_all(bar_track);
    lv_obj_set_size(bar_track, bar_w, kBarH);
    lv_obj_align(bar_track, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(bar_track, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bar_track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar_track, kBarH / 2, 0);
    lv_obj_set_style_clip_corner(bar_track, true, 0);
    lv_obj_clear_flag(bar_track, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(bar_track);

    // 白底轨道 + 黑色已读；内缩留白缝，避免黑填与卡面黑底糊边
    constexpr lv_coord_t kBarInset = 2;
    if (bar_pct > 0) {
        const lv_coord_t inner_h = kBarH - 2 * kBarInset;
        lv_coord_t fg_w =
            static_cast<lv_coord_t>((bar_w - 2 * kBarInset) * bar_pct / 100);
        if (fg_w < inner_h) {
            fg_w = inner_h;
        }
        if (fg_w > bar_w - 2 * kBarInset) {
            fg_w = bar_w - 2 * kBarInset;
        }
        lv_obj_t* bar_fg = lv_obj_create(bar_track);
        lv_obj_remove_style_all(bar_fg);
        lv_obj_set_size(bar_fg, fg_w, inner_h);
        lv_obj_set_pos(bar_fg, kBarInset, kBarInset);
        lv_obj_set_style_bg_color(bar_fg, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(bar_fg, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar_fg, inner_h / 2, 0);
        lv_obj_clear_flag(bar_fg, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(bar_fg);
    }

    lv_obj_t* thumb = lv_obj_create(bar_wrap);
    lv_obj_remove_style_all(thumb);
    lv_obj_set_size(thumb, kThumb, kThumb);
    lv_obj_set_style_bg_color(thumb, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(thumb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(thumb, 2, 0);
    lv_obj_set_style_border_color(thumb, lv_color_black(), 0);
    lv_coord_t thumb_x = 0;
    if (bar_pct > 0) {
        thumb_x = static_cast<lv_coord_t>((bar_w - kThumb) * bar_pct / 100);
    }
    lv_obj_set_pos(thumb, thumb_x, (std::max(kBarH, kThumb) - kThumb) / 2);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(thumb);

    lv_obj_t* cont_btn = lv_obj_create(cont);
    lv_obj_remove_style_all(cont_btn);
    lv_obj_set_size(cont_btn, bar_w, kContBtnH);
    lv_obj_set_style_bg_color(cont_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cont_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cont_btn, 8, 0);
    DisableScroll(cont_btn);
    if (stats.continue_index >= 0) {
        lv_obj_add_flag(cont_btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(cont_btn);
        lv_obj_add_event_cb(cont_btn, OnContinueReadingClicked, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(stats.continue_index)));
    } else {
        lv_obj_clear_flag(cont_btn, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t* cont_btn_lbl = lv_label_create(cont_btn);
    lv_label_set_text(cont_btn_lbl, Lang::Strings::BOOK_CONTINUE);
    lv_obj_set_style_text_font(cont_btn_lbl, item_f, 0);
    lv_obj_set_style_text_color(cont_btn_lbl, lv_color_black(), 0);
    lv_obj_center(cont_btn_lbl);
    lv_obj_clear_flag(cont_btn_lbl, LV_OBJ_FLAG_CLICKABLE);

    // 右侧统计：柱图高度封顶，下方数字区固定，避免重叠
    lv_obj_t* stats_box = lv_obj_create(dash);
    lv_obj_remove_style_all(stats_box);
    lv_obj_set_size(stats_box, stats_w, dash_h);
    lv_obj_set_style_bg_opa(stats_box, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(stats_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(stats_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(stats_box, 6, 0);
    lv_obj_clear_flag(stats_box, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(stats_box);

    // 数字+下划线+文案（含 pad_row×2），柱图高度据此封顶，避免叠到竖条上
    const lv_coord_t stats_block_h = nh + 4 + 2 + 4 + ih;
    const lv_coord_t chart_h =
        std::max<lv_coord_t>(36, dash_h - stats_block_h - 6);
    DrawHomeBarChart(stats_box, stats_w, chart_h, stats.finished_count,
                     static_cast<int>(st.books.size()));

    lv_obj_t* stats_row = lv_obj_create(stats_box);
    lv_obj_remove_style_all(stats_row);
    lv_obj_set_size(stats_row, stats_w, stats_block_h);
    lv_obj_set_flex_flow(stats_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stats_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_clip_corner(stats_row, true, 0);
    lv_obj_clear_flag(stats_row, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(stats_row);

    auto make_stat_col = [&](const char* value, const char* label) {
        const lv_coord_t col_w = (stats_w - 8) / 2;
        lv_obj_t* col = lv_obj_create(stats_row);
        lv_obj_remove_style_all(col);
        lv_obj_set_size(col, col_w, stats_block_h);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(col, 4, 0);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(col);

        lv_obj_t* num = lv_label_create(col);
        lv_label_set_text(num, value);
        lv_obj_set_style_text_font(num, num_font, 0);
        lv_obj_set_width(num, col_w);
        lv_label_set_long_mode(num, LV_LABEL_LONG_CLIP);
        lv_obj_clear_flag(num, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* line = lv_obj_create(col);
        lv_obj_remove_style_all(line);
        lv_obj_set_size(line, col_w, 2);
        lv_obj_set_style_bg_color(line, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(line);

        lv_obj_t* lab = lv_label_create(col);
        lv_label_set_text(lab, label);
        lv_obj_set_style_text_font(lab, item_f, 0);
        lv_obj_set_width(lab, col_w);
        lv_label_set_long_mode(lab, LV_LABEL_LONG_CLIP);
        lv_obj_clear_flag(lab, LV_OBJ_FLAG_CLICKABLE);
    };

    char dur_val[24];
    FormatHomeDurationShort(dur_val, sizeof(dur_val), stats.total_seconds);
    char fin_val[24];
    std::snprintf(fin_val, sizeof(fin_val), Lang::Strings::BOOK_COUNT_FMT, stats.finished_count);
    make_stat_col(dur_val, Lang::Strings::BOOK_READ_DURATION);
    make_stat_col(fin_val, Lang::Strings::BOOK_FINISHED);

    // 封面恢复比例高度；余白落在继续阅读与最近阅读之间
    auto make_vgap = [&](lv_coord_t h) {
        lv_obj_t* g = lv_obj_create(st.list_body);
        lv_obj_remove_style_all(g);
        lv_obj_set_size(g, content_w, h);
        lv_obj_set_style_bg_opa(g, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(g, LV_OBJ_FLAG_CLICKABLE);
    };

    lv_obj_t* mid_spacer = lv_obj_create(st.list_body);
    lv_obj_remove_style_all(mid_spacer);
    lv_obj_set_width(mid_spacer, content_w);
    lv_obj_set_flex_grow(mid_spacer, 1);
    lv_obj_set_style_bg_opa(mid_spacer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(mid_spacer, LV_OBJ_FLAG_CLICKABLE);

    // 最近阅读：始终保留标题 + 两个封面边框；空框内各自居中「暂无最近阅读」
    {
        lv_obj_t* recent_lbl = lv_label_create(st.list_body);
        lv_label_set_text(recent_lbl, Lang::Strings::BOOK_RECENT);
        lv_obj_set_style_text_font(recent_lbl, list_f, 0);
        lv_obj_set_width(recent_lbl, content_w);
        lv_obj_set_style_text_align(recent_lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_clear_flag(recent_lbl, LV_OBJ_FLAG_CLICKABLE);

        make_vgap(8);

        const lv_coord_t recent_cell_w = (content_w - kHomeGap) / 2;
        lv_coord_t recent_cover_h =
            std::min(kRecentCoverH, static_cast<lv_coord_t>(recent_cell_w * 4 / 3));
        if (recent_cover_h < 140) {
            recent_cover_h = 140;
        }

        lv_obj_t* recent_row = lv_obj_create(st.list_body);
        lv_obj_remove_style_all(recent_row);
        lv_obj_set_width(recent_row, content_w);
        lv_obj_set_height(recent_row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(recent_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(recent_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(recent_row, kHomeGap, 0);
        lv_obj_clear_flag(recent_row, LV_OBJ_FLAG_CLICKABLE);
        DisableScroll(recent_row);

        auto make_empty_recent_slot = [&](lv_obj_t* parent) {
            lv_obj_t* host = lv_obj_create(parent);
            lv_obj_remove_style_all(host);
            lv_obj_set_size(host, recent_cell_w, recent_cover_h);
            lv_obj_set_style_pad_all(host, kCoverFrameBorder, 0);
            lv_obj_set_style_bg_color(host, lv_color_hex(0xF0F0F0), 0);
            lv_obj_set_style_bg_opa(host, LV_OPA_COVER, 0);
            StyleBookCoverFrame(host);
            lv_obj_clear_flag(host, LV_OBJ_FLAG_CLICKABLE);
            DisableScroll(host);

            lv_obj_t* empty_lbl = lv_label_create(host);
            lv_label_set_text(empty_lbl, Lang::Strings::BOOK_NO_RECENT);
            lv_obj_set_style_text_font(empty_lbl, list_f, 0);
            lv_obj_set_style_text_color(empty_lbl, lv_color_black(), 0);
            lv_obj_set_style_text_align(empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(empty_lbl, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(empty_lbl, CoverFrameInner(recent_cell_w) - 8);
            lv_obj_clear_flag(empty_lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_align(empty_lbl, LV_ALIGN_CENTER, 0, 0);
            return host;
        };

        for (int k = 0; k < 2; ++k) {
            const int idx = stats.has_recent ? stats.recent_indices[k] : -1;
            if (idx >= 0 && idx < static_cast<int>(st.books.size())) {
                CreateBookCoverCell(recent_row, st.books[static_cast<size_t>(idx)], idx,
                                    recent_cell_w, recent_cover_h, st.recent_covers, false,
                                    kRecentTitleGap, true);
            } else {
                make_empty_recent_slot(recent_row);
            }
        }

        make_vgap(kHomeShelfTopGap);
    }

    // —— 底部「我的书架 >」——
    lv_obj_t* shelf_btn = lv_obj_create(st.list_body);
    lv_obj_remove_style_all(shelf_btn);
    lv_obj_set_size(shelf_btn, content_w, kHomeShelfBtnH);
    lv_obj_set_style_bg_color(shelf_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(shelf_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(shelf_btn, kHomeShelfBtnRadius, 0);
    lv_obj_add_flag(shelf_btn, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(shelf_btn);
    HapticAttachClick(shelf_btn);
    lv_obj_add_event_cb(shelf_btn, OnOpenShelfClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* shelf_lbl = lv_label_create(shelf_btn);
    char shelf_txt[64];
    std::snprintf(shelf_txt, sizeof(shelf_txt), "%s  >", Lang::Strings::BOOK_SHELF_TITLE);
    lv_label_set_text(shelf_lbl, shelf_txt);
    lv_obj_set_style_text_font(shelf_lbl, list_f, 0);
    lv_obj_set_style_text_color(shelf_lbl, lv_color_white(), 0);
    lv_obj_center(shelf_lbl);
    lv_obj_clear_flag(shelf_lbl, LV_OBJ_FLAG_CLICKABLE);

    ScheduleListCoverFill();
}

void RenderBookshelfPage() {
    auto& st = State();
    if (st.list_body == nullptr) {
        return;
    }
    CancelListCoverFill();
    lv_obj_set_style_layout(st.list_body, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(st.list_body, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(st.list_body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(st.list_body, kShelfRowGap, 0);
    lv_obj_set_style_pad_column(st.list_body, kShelfColGap, 0);

    ClampListPage();
    lv_obj_clean(st.list_body);
    st.shelf_covers.clear();
    DisableScroll(st.list_body);

    if (st.books.empty()) {
        lv_obj_set_flex_flow(st.list_body, LV_FLEX_FLOW_COLUMN);
        char tip[160];
        std::snprintf(tip, sizeof(tip), Lang::Strings::BOOK_EMPTY_SHELF_FMT,
                      SdUserPath(reader::kDefaultBooksDir));
        ShowMessage(st.list_body, tip, ListFont());
        if (st.list_footer != nullptr) {
            lv_label_set_text(st.list_footer, "1/1");
        }
        RefreshShelfFooterMode();
        return;
    }

    const lv_coord_t content_w = ContentWidth();
    const lv_coord_t cell_w =
        (content_w - kShelfColGap * (kShelfCols - 1)) / kShelfCols;
    const lv_coord_t cover_h =
        st.shelf_cover_h > 0 ? st.shelf_cover_h : (cell_w * 4 / 3);

    const int start = st.list_page * st.list_page_size;
    const int end = std::min(start + st.list_page_size, static_cast<int>(st.books.size()));
    for (int i = start; i < end; ++i) {
        CreateBookCoverCell(st.list_body, st.books[static_cast<size_t>(i)], i, cell_w, cover_h,
                            st.shelf_covers, true, kShelfTitleGap, true);
    }

    if (st.list_footer != nullptr) {
        char foot[48];
        std::snprintf(foot, sizeof(foot), "%d/%d", st.list_page + 1, ListPageCount());
        lv_label_set_text(st.list_footer, foot);
    }
    RefreshShelfFooterMode();
    ScheduleListCoverFill();
}

void RequestOpenAssistantFromBook() {
    const char* screen = VkKey_ActiveScreen();
    if (screen != nullptr && std::strcmp(screen, kScreenRead) == 0) {
        ESP_LOGI(TAG, "AI long (%s) -> release session, assistant", screen);
        RequestOpenAssistantFromReader();
        return;
    }
    CancelDetailCoverLoad();
    CancelListCoverFill();
    ESP_LOGI(TAG, "AI long (%s) -> assistant", screen != nullptr ? screen : "?");
    AssistantScreen::RequestOpen();
}

}  // namespace

lv_obj_t* BookScreen::Create() {
    auto& st = State();
    const bool workers_busy = st.opening.load() || st.layout_busy.load() ||
                              st.chapter_pages_busy.load() || st.page_image_busy.load();
    if (workers_busy) {
        st.open_token.fetch_add(1);
        st.layout_token.fetch_add(1);
        st.chapter_pages_token.fetch_add(1);
        CancelPageImageLoad();
        if (st.session) {
            st.session->InvalidateChapterPageTable();
            st.session->AbortTxtPaginate();
        }
        StopReadTimeCheckpointTimer();
        st.deferred_cleanup = true;
    } else {
        // 收尾：含 worker 投递失败后留下的 deferred_cleanup
        if (st.deferred_cleanup) {
            FinishDeferredCleanup();
        } else {
            EndReadingTimeTracking();
            st.session.reset();
            ReleaseBookFont();
        }
    }
    // 封面由详情屏 DELETE 释放；此处不 Reset，避免旧屏仍引用时 UAF
    st.read_scr = nullptr;
    st.content = nullptr;
    st.page_label = nullptr;
    st.title_label = nullptr;
    st.status_label = nullptr;
    st.status_bar = nullptr;
    st.status_overlay = nullptr;
    st.status_h = 0;
    st.settings_sheet = nullptr;
    ClearSettingsSheetWidgetRefs();
    StopTtfPollTimer();
    st.ttf_mode = BookUiState::TtfMode::kNone;
    st.ttf_pick.clear();
    st.ttf_page = 0;
    st.ttf_size_px = kTtfConvertSizeDefault;
    st.viewport_w = 0;
    st.viewport_h = 0;
    st.font_entries.clear();
    st.font_list_page = 0;
    st.font_multi = false;
    st.font_selected.clear();
    st.font_suppress_click_until_us = 0;
    st.font_suppress_click_idx = -1;
    st.font_suppress_next_click = false;
    st.read_chrome = BookUiState::ReadChrome::kReading;
    st.settings_resume_after_open = false;
    st.layout_again = false;
    st.layout_reload_font = false;
    st.layout_anchor = {};
    st.chapter_pages_again = false;
    CancelLayoutDebounce();
    StopLayoutHintTimer();
    st.layout_hint_done_until_us = 0;
    st.open_bar = nullptr;
    st.open_pct_lbl = nullptr;
    st.shelf_multi = false;
    st.shelf_selected.clear();
    st.shelf_suppress_click_until_us = 0;
    st.shelf_suppress_click_idx = -1;
    st.multi_bar = nullptr;
    st.back_root = BookUiState::NavRoot::kHome;
    CancelListCoverFill();
    st.recent_covers.clear();
    st.shelf_covers.clear();
    ResetReadTouch();

    ScreenSetIsHome(false);

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, ListFont(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    DisableScroll(scr);
    st.list_scr = scr;
    lv_obj_add_event_cb(
        scr,
        [](lv_event_t* e) {
            auto& s = State();
            const auto* tgt = static_cast<lv_obj_t*>(lv_event_get_target(e));
            if (s.list_scr == tgt) {
                CancelListCoverFill();
                s.list_scr = nullptr;
                s.list_body = nullptr;
                s.list_footer = nullptr;
                s.multi_bar = nullptr;
                s.shelf_multi = false;
                s.shelf_selected.clear();
                s.recent_covers.clear();
                s.shelf_covers.clear();
            }
        },
        LV_EVENT_DELETE, nullptr);

    EpdStatusBar status = ScreenCreateStatusBar(scr);
    st.status_label = status.status_label;
    if (status.notification_label) {
        lv_obj_add_flag(status.notification_label, LV_OBJ_FLAG_HIDDEN);
    }

    const lv_coord_t body_h = LV_VER_RES - status.height;

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_HOR_RES, body_h);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_left(body, kListPad, 0);
    lv_obj_set_style_pad_right(body, kListPad, 0);
    lv_obj_set_style_pad_top(body, kListPad, 0);
    lv_obj_set_style_pad_bottom(body, kListPad + kHomeBottomPad, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(body, kHomeGap, 0);
    DisableScroll(body);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* list_body = lv_obj_create(body);
    lv_obj_remove_style_all(list_body);
    lv_obj_set_width(list_body, ContentWidth());
    lv_obj_set_flex_grow(list_body, 1);
    lv_obj_set_flex_flow(list_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list_body, 0, 0);
    DisableScroll(list_body);
    st.list_body = list_body;
    st.list_footer = nullptr;
    st.multi_bar = nullptr;

    auto& sd = SdCardManager::GetInstance();
    if (!sd.IsMounted() && !sd.Mount()) {
        ShowMessage(list_body, Lang::Strings::BOOK_NO_SD, ListFont());
        VkKey_AttachScreen(scr, kScreenLibrary, BookAiLongPressDesc(BookScreen::Create));
        return scr;
    }

    st.books.clear();
    // 优先复用开机/上次扫库缓存，未命中再扫盘
    (void)reader::LoadBookLibrary(reader::kDefaultBooksDir, st.books);
    // 仅丢掉已不在书库的 miss；保留仍在库中的，避免回首页再进又重复 Open 抽封面
    if (!st.cover_embed_miss.empty()) {
        st.cover_embed_miss.erase(
            std::remove_if(st.cover_embed_miss.begin(), st.cover_embed_miss.end(),
                           [&](const std::string& path) {
                               for (const auto& b : st.books) {
                                   if (b.path == path) {
                                       return false;
                                   }
                               }
                               return true;
                           }),
            st.cover_embed_miss.end());
    }
    RenderReadingHome();

    // factory=Create：进百问后返回阅读首页
    VkKey_AttachScreen(scr, kScreenLibrary, BookAiLongPressDesc(BookScreen::Create));
    return scr;
}

namespace {

lv_obj_t* CreateBookshelfScreen() {
    auto& st = State();
    const bool workers_busy = st.opening.load() || st.layout_busy.load() ||
                              st.chapter_pages_busy.load() || st.page_image_busy.load();
    if (workers_busy) {
        st.open_token.fetch_add(1);
        st.layout_token.fetch_add(1);
        st.chapter_pages_token.fetch_add(1);
        CancelPageImageLoad();
        if (st.session) {
            st.session->InvalidateChapterPageTable();
            st.session->AbortTxtPaginate();
        }
        StopReadTimeCheckpointTimer();
        st.deferred_cleanup = true;
    } else if (st.deferred_cleanup) {
        FinishDeferredCleanup();
    } else {
        EndReadingTimeTracking();
        st.session.reset();
        ReleaseBookFont();
    }

    st.read_scr = nullptr;
    st.content = nullptr;
    st.page_label = nullptr;
    st.title_label = nullptr;
    st.status_bar = nullptr;
    st.status_overlay = nullptr;
    st.status_h = 0;
    st.settings_sheet = nullptr;
    ClearSettingsSheetWidgetRefs();
    StopTtfPollTimer();
    st.shelf_multi = false;
    st.shelf_selected.clear();
    st.shelf_suppress_click_until_us = 0;
    st.shelf_suppress_click_idx = -1;
    st.multi_bar = nullptr;
    st.back_root = BookUiState::NavRoot::kShelf;
    CancelListCoverFill();
    st.recent_covers.clear();
    st.shelf_covers.clear();
    ResetReadTouch();
    CancelLayoutDebounce();
    StopLayoutHintTimer();
    ScreenPaintCoalesceReset(&s_shelf_check_paint);

    ScreenSetIsHome(false);

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, ListFont(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    DisableScroll(scr);
    st.list_scr = scr;
    lv_obj_add_event_cb(
        scr,
        [](lv_event_t* e) {
            auto& s = State();
            const auto* tgt = static_cast<lv_obj_t*>(lv_event_get_target(e));
            if (s.list_scr == tgt) {
                CancelListCoverFill();
                s.list_scr = nullptr;
                s.list_body = nullptr;
                s.list_footer = nullptr;
                s.multi_bar = nullptr;
                s.shelf_multi = false;
                s.shelf_selected.clear();
                ScreenPaintCoalesceReset(&s_shelf_check_paint);
                s.recent_covers.clear();
                s.shelf_covers.clear();
            }
        },
        LV_EVENT_DELETE, nullptr);

    EpdStatusBar status = ScreenCreateStatusBar(scr);
    st.status_label = status.status_label;
    if (status.status_label) {
        lv_label_set_text(status.status_label, Lang::Strings::BOOK_SHELF_TITLE);
    }
    if (status.notification_label) {
        lv_obj_add_flag(status.notification_label, LV_OBJ_FLAG_HIDDEN);
    }

    const lv_coord_t body_h = LV_VER_RES - status.height - kFooterH;
    if (st.list_page < 0) {
        st.list_page = 0;
    }

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_HOR_RES, body_h);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(body, kListPad, 0);
    DisableScroll(body);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* list_body = lv_obj_create(body);
    lv_obj_remove_style_all(list_body);
    lv_obj_set_size(list_body, ContentWidth(), body_h - kListPad * 2);
    lv_obj_align(list_body, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(list_body, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(list_body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list_body, kShelfRowGap, 0);
    lv_obj_set_style_pad_column(list_body, kShelfColGap, 0);
    DisableScroll(list_body);
    st.list_body = list_body;

    // 底栏槽：页码与多选共用，不改书籍区高度
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
    lv_obj_set_style_text_font(page_lbl, ItemFont(), 0);
    lv_obj_set_style_text_color(page_lbl, lv_color_black(), 0);
    lv_label_set_text(page_lbl, "1/1");
    lv_obj_align(page_lbl, LV_ALIGN_CENTER, 0, 0);
    DisableScroll(page_lbl);
    st.list_footer = page_lbl;

    st.multi_bar = lv_obj_create(foot);
    lv_obj_remove_style_all(st.multi_bar);
    lv_obj_set_width(st.multi_bar, lv_pct(100));
    lv_obj_set_height(st.multi_bar, kShelfMultiBtnH);
    lv_obj_set_style_bg_opa(st.multi_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(st.multi_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(st.multi_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(st.multi_bar, 10, 0);
    lv_obj_align(st.multi_bar, LV_ALIGN_CENTER, 0, 0);
    DisableScroll(st.multi_bar);
    lv_obj_clear_flag(st.multi_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(st.multi_bar, LV_OBJ_FLAG_HIDDEN);

    auto make_multi_action = [](lv_obj_t* parent, const char* text, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_obj_create(parent);
        lv_obj_remove_style_all(btn);
        lv_obj_set_height(btn, kShelfMultiBtnH);
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
        lv_obj_set_style_border_width(text_wrap, kShelfUnderlineH, 0);
        lv_obj_set_style_border_color(text_wrap, lv_color_black(), 0);
        lv_obj_set_style_pad_bottom(text_wrap, 2, 0);
        lv_obj_set_style_pad_hor(text_wrap, kShelfUnderlinePadHor, 0);
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
        lv_obj_set_size(dot, kShelfMultiDotSize, kShelfMultiDotSize);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        DisableScroll(dot);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        return dot;
    };
    make_multi_action(st.multi_bar, Lang::Strings::COMMON_CANCEL, OnShelfMultiCancel);
    make_multi_dot(st.multi_bar);
    make_multi_action(st.multi_bar, Lang::Strings::COMMON_SELECT_ALL, OnShelfMultiSelectAll);
    make_multi_dot(st.multi_bar);
    make_multi_action(st.multi_bar, Lang::Strings::COMMON_REMOVE, OnShelfMultiRemove);

    // 固定 3×3：按可用高度反推封面高，保证一页九本
    st.list_page_size = kShelfCols * kShelfRows;
    const lv_coord_t cell_w =
        (ContentWidth() - kShelfColGap * (kShelfCols - 1)) / kShelfCols;
    const lv_coord_t title_h =
        (ItemFont() != nullptr && ItemFont()->line_height > 0)
            ? ItemFont()->line_height + kShelfTitleGap + 2
            : 34;
    const lv_coord_t usable = body_h - kListPad * 2;
    const lv_coord_t cell_budget =
        (usable - kShelfRowGap * (kShelfRows - 1)) / kShelfRows;
    lv_coord_t cover_h = cell_budget - title_h;
    const lv_coord_t cover_max = cell_w * 4 / 3;
    if (cover_h > cover_max) {
        cover_h = cover_max;
    }
    if (cover_h < 72) {
        cover_h = 72;
    }
    st.shelf_cover_h = cover_h;

    auto& sd = SdCardManager::GetInstance();
    if (!sd.IsMounted() && !sd.Mount()) {
        ShowMessage(list_body, Lang::Strings::BOOK_NO_SD, ListFont());
        VkKey_AttachScreen(scr, kScreenBookshelf, BookAiLongPressDesc(CreateBookshelfScreen));
        return scr;
    }

    // 与首页一致走 Load：缓存命中只拷贝；USB/云端 Invalidate 后在此重扫
    (void)reader::LoadBookLibrary(reader::kDefaultBooksDir, st.books);
    RenderBookshelfPage();

    VkKey_AttachScreen(scr, kScreenBookshelf, BookAiLongPressDesc(CreateBookshelfScreen));
    return scr;
}

}  // namespace

bool BookScreen::IsReadingActive() {
    // 仅正文沉浸翻页；目录/设置浮层仍走音量。编排中仍算阅读中，以便音量键走翻页提示。
    if (std::strcmp(VkKey_ActiveScreen(), kScreenRead) != 0) {
        return false;
    }
    auto& st = State();
    return !IsReadOverlayChrome(st.read_chrome);
}

bool BookPageRepeatStep(int page_delta) {
    const char* screen = VkKey_ActiveScreen();
    auto& st = State();
    if (page_delta == 0) {
        return false;
    }

    if (std::strcmp(screen, kScreenBookshelf) == 0) {
        const int last = std::max(0, ListPageCount() - 1);
        int next = st.list_page + page_delta;
        if (next < 0) {
            next = 0;
        } else if (next > last) {
            next = last;
        }
        if (next == st.list_page) {
            return false;
        }
        st.list_page = next;
        RequestRenderBookshelfPage();
        return page_delta < 0 ? st.list_page > 0 : st.list_page < last;
    }

    if (std::strcmp(screen, kScreenLibrary) == 0) {
        return false;
    }

    if (std::strcmp(screen, kScreenRead) != 0 || st.opening.load() || !st.session ||
        !st.session->IsOpen()) {
        return false;
    }

    if (st.read_chrome == BookUiState::ReadChrome::kToc) {
        const int pages =
            std::max(1, (st.session->TocCount() + st.toc_page_size - 1) /
                            std::max(1, st.toc_page_size));
        int next = st.toc_list_page + page_delta;
        const int last = std::max(0, pages - 1);
        if (next < 0) {
            next = 0;
        } else if (next > last) {
            next = last;
        }
        if (next == st.toc_list_page) {
            return false;
        }
        st.toc_list_page = next;
        RequestRenderTocList();
        return page_delta < 0 ? st.toc_list_page > 0 : st.toc_list_page < last;
    }
    if (st.read_chrome == BookUiState::ReadChrome::kSettings) {
        // 悬浮设置卡片：长按连翻不翻页
        return false;
    }

    // 正文：±10 写入积压，Apply 一次吃到目标页（或边界）
    if (!QueueReaderPageTurn(page_delta)) {
        return false;
    }
    return page_delta < 0 ? st.session->HasPrevPage() : st.session->HasNextPage();
}

bool BookScreen::OnBootLongPress() {
    RequestOpenAssistantFromBook();
    return true;
}

bool BookScreen::OnVkKeyLongPress(const char* key_name) {
    if (key_name == nullptr) {
        return false;
    }
    // 盖板 vk_home 长按不进百问（仅 BOOT）；交默认策略一键回系统首页
    if (std::strcmp(key_name, "vk_home") == 0) {
        return false;
    }
    return VkPageRepeatTryStart(key_name, BookPageRepeatStep);
}

bool BookScreen::OnVkKeyPressUp(const char* key_name) {
    if (!VkPageRepeatOnPressUp(key_name)) {
        return false;
    }
    // 松手时若还有未画的 delta，补一帧（通常定时器已停、积压很小）
    if (s_reader_page_delta.load(std::memory_order_acquire) != 0) {
        RequestRenderReaderPage();
    }
    return true;
}

bool BookScreen::OnVkKey(const char* key_name) {
    if (key_name == nullptr) {
        return true;
    }
    const char* screen = VkKey_ActiveScreen();
    auto& st = State();

    if (std::strcmp(screen, kScreenRead) == 0) {
        // 正文 vk_home 短按：设置/目录浮层→收起；长按由 VkKey 默认回系统首页
        if (std::strcmp(key_name, "vk_home") == 0) {
            if (st.read_chrome != BookUiState::ReadChrome::kReading) {
                BookLvAsync(AsyncHideReadChrome);
                return true;
            }
            BookLvAsync(BackToDetailAsync);
            return true;
        }
        if (st.opening.load()) {
            return true;  // 解析中：翻页键无效；HOME 已在上方处理
        }
        if (!st.session || !st.session->IsOpen()) {
            // 打开失败页：上一页回详情
            if (std::strcmp(key_name, "vk_prev") == 0) {
                BookLvAsync(BackToDetailAsync);
            }
            return true;
        }
        if (st.read_chrome == BookUiState::ReadChrome::kSettings) {
            // 悬浮设置：多选中 prev 取消批量；否则收起；next 忽略
            if (std::strcmp(key_name, "vk_prev") == 0) {
                if (st.font_multi) {
                    ExitFontMultiMode(true);
                } else {
                    BookLvAsync(AsyncHideReadChrome);
                }
            }
            return true;
        }
        // 目录列表：vk 翻列表页
        if (st.read_chrome == BookUiState::ReadChrome::kToc) {
            const int pages =
                std::max(1, (st.session->TocCount() + st.toc_page_size - 1) /
                                std::max(1, st.toc_page_size));
            if (std::strcmp(key_name, "vk_prev") == 0) {
                if (st.toc_list_page > 0) {
                    --st.toc_list_page;
                    RequestRenderTocList();
                } else {
                    BookLvAsync(AsyncHideReadChrome);
                }
                return true;
            }
            if (std::strcmp(key_name, "vk_next") == 0) {
                if (st.toc_list_page + 1 < pages) {
                    ++st.toc_list_page;
                    RequestRenderTocList();
                }
                return true;
            }
            return true;
        }
        if (std::strcmp(key_name, "vk_prev") == 0) {
            if (!QueueReaderPageTurn(-1)) {
                BookLvAsync(BackToDetailAsync);
            }
            return true;
        }
        if (std::strcmp(key_name, "vk_next") == 0) {
            QueueReaderPageTurn(1);
            return true;
        }
        return true;
    }

    if (std::strcmp(screen, kScreenDetail) == 0) {
        if (std::strcmp(key_name, "vk_home") == 0) {
            BookLvAsync(BackToLibraryAsync);
            return true;
        }
        if (std::strcmp(key_name, "vk_prev") == 0) {
            BookLvAsync(BackToLibraryAsync);
            return true;
        }
        if (std::strcmp(key_name, "vk_next") == 0) {
            if (!st.opening.load()) {
                BookLvAsync(StartReadAsync,
                            reinterpret_cast<void*>(static_cast<intptr_t>(st.selected)));
            }
            return true;
        }
        return true;
    }

    if (std::strcmp(screen, kScreenLibrary) == 0) {
        if (std::strcmp(key_name, "vk_home") == 0) {
            RequestBackHome();
            return true;
        }
        if (std::strcmp(key_name, "vk_prev") == 0) {
            return false; // 阅读首页：出栈回系统首页
        }
        if (std::strcmp(key_name, "vk_next") == 0) {
            BookLvAsync(OpenBookshelfAsync);
            return true;
        }
        return true;
    }

    if (std::strcmp(screen, kScreenBookshelf) == 0) {
        if (std::strcmp(key_name, "vk_home") == 0) {
            BookLvAsync(BackToReadingHomeAsync);
            return true;
        }
        if (std::strcmp(key_name, "vk_prev") == 0) {
            if (st.list_page > 0) {
                --st.list_page;
                RequestRenderBookshelfPage();
                return true;
            }
            // 第一页：多选中则取消批量，否则回阅读首页
            if (st.shelf_multi) {
                ExitShelfMultiMode(true);
                return true;
            }
            BookLvAsync(BackToReadingHomeAsync);
            return true;
        }
        if (std::strcmp(key_name, "vk_next") == 0) {
            if (st.list_page + 1 < ListPageCount()) {
                ++st.list_page;
                RequestRenderBookshelfPage();
            }
            return true;
        }
        return true;
    }
    return true;
}

void BookScreen::OnReadingFontPrefChanged() {
    if (!State().opening.load()) {
        ReleaseBookFont();
    }
}

void BookScreen::OnEnterStandby() {
    PauseReadingTimeForStandby();
}

void BookScreen::OnResumeFromStandby() {
    ResumeReadingTimeAfterStandby();
}

void BookScreen::OpenDownloadedBook(const reader::BookInfo& info) {
    if (info.path.empty()) {
        return;
    }
    auto& st = State();
    if (ReaderWorkersBusy()) {
        ESP_LOGW(TAG, "OpenDownloadedBook ignored: worker busy");
        return;
    }
    CancelDetailCoverLoad();
    CancelListCoverFill();
    const int index = EnsureBookInLibrary(info);
    st.selected = index;
    ScreenLoadReplace(CreateReaderScreen(info));
}
