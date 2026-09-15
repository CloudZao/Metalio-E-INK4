#include "assistant_screen.h"

#include "a2ui.h"
#include "a2ui_image.h"
#include "a2ui_math.h"
#include "application.h"
#include "assets/lang_config.h"
#include "assistant_boot_photo.h"
#include "assistant_chat_store.h"
#include "board.h"
#include "boot_key_handler.h"
#include "display.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "image_util.h"
#include "lv_adapter_display.h"
#include "power_policy.h"
#include "reader_types.h"
#include "screen_common.h"
#include "task_screen/task_screen.h"
#include "text_encoding.h"
#include "vk_key_handler.h"
#include "vk_page_repeat.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <cJSON.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_lv_adapter.h>
#include <esp_timer.h>

namespace {

constexpr const char* TAG = "AssistantScreen";
/** 无会话时全屏提示图：中文走 resources；英文嵌入固件（可随 app OTA）。 */
constexpr const char* kIdleHintAssetZh = "ic_s_assistant_hint.a2i1";
constexpr int kIdleHintMaxW = 480;
constexpr int kIdleHintMaxH = 800;

// CMake EMBED：assets/embedded/ic_s_assistant_hint_en.a2i1
extern const uint8_t ic_s_assistant_hint_en_a2i1_start[] asm("_binary_ic_s_assistant_hint_en_a2i1_start");
extern const uint8_t ic_s_assistant_hint_en_a2i1_end[] asm("_binary_ic_s_assistant_hint_en_a2i1_end");
/** 屏触 PTT 臂听阈值：与 BOOT 共用 kBootLongPressMs，勿各写各的。 */
constexpr uint32_t kTouchPttLongPressMs = kBootLongPressMs;
constexpr uint16_t kUiFontSize = 30;
constexpr uint16_t kUiFontBpp = 2;
constexpr uint16_t kUiFontBoldBpp = 4;

constexpr lv_coord_t kContentPad = 8;
constexpr lv_coord_t kFooterH = 36;
/** 与 a2ui Column 默认 pad_row 一致，分页高度估算才准 */
constexpr lv_coord_t kBlockGap = 8;
/** UI 会话最多保留页数；超出从 flow 头裁，整表重分页 */
constexpr int kMaxPages = 1500;
/** 流式包合并窗口：主循环不再每包重排，翻页才有空档 */
constexpr uint64_t kStreamCoalesceUs = 400000;
/** 流式跟刷最少间隔（仅影响页）；翻页本身不节流 */
constexpr int64_t kStreamPaintMinUs = 500000;

/** 会话 flow/pages 与其中字符串一律 SPIRAM，减轻内部 DRAM。 */
template <typename T>
struct SpirAlloc {
    using value_type = T;
    SpirAlloc() noexcept = default;
    template <typename U>
    SpirAlloc(const SpirAlloc<U>&) noexcept {}
    T* allocate(std::size_t n) {
        const size_t bytes = n * sizeof(T);
        void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (p == nullptr) {
            p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
        }
        if (p == nullptr) {
            ESP_LOGE(TAG, "SpirAlloc OOM %u bytes", static_cast<unsigned>(bytes));
            abort();
        }
        return static_cast<T*>(p);
    }
    void deallocate(T* p, std::size_t) noexcept {
        heap_caps_free(p);
    }
};
template <typename T, typename U>
constexpr bool operator==(const SpirAlloc<T>&, const SpirAlloc<U>&) {
    return true;
}
template <typename T, typename U>
constexpr bool operator!=(const SpirAlloc<T>&, const SpirAlloc<U>&) {
    return false;
}

using SpirString = std::basic_string<char, std::char_traits<char>, SpirAlloc<char>>;
template <typename T>
using SpirVec = std::vector<T, SpirAlloc<T>>;

/** 屏内 hit 层长按已臂听且未松手。 */
std::atomic<bool> s_touch_ptt_held{false};
/** 百问页在前台（供主循环等非 LVGL 线程读；禁止在那些线程 lv_obj_is_valid） */
std::atomic<bool> s_active{false};

lv_obj_t* s_scr = nullptr;
lv_obj_t* s_content = nullptr;  // a2ui host：只渲染当前页
lv_obj_t* s_page_label = nullptr;
lv_obj_t* s_hint_img = nullptr;  // 无会话时全屏提示图
lv_obj_t* s_status_label = nullptr;       // 状态栏居中时钟/文案
lv_obj_t* s_notification_label = nullptr;
lv_obj_t* s_ptt_wave = nullptr;           // 状态栏居中录音波形（代码绘制竖条）
lv_obj_t* s_touch_ptt_hit = nullptr;      // 全屏透明 PTT hit；盖板 VK 不经此层
lv_timer_t* s_ptt_wave_timer = nullptr;
lv_timer_t* s_touch_ptt_arm_timer = nullptr;  // 屏触 one-shot 臂听；离页/松手必须删
bool s_touch_ptt_finger_down = false;         // hit 层 PRESSED 尚未抬起（LVGL 任务内）
bool s_touch_suppress_click = false;          // 长按 PTT 臂听后抑制紧随 CLICKED
bool s_ptt_wave_visible = false;
reader::RasterImage* s_hint_raster = nullptr;  // 由 hint_img DELETE 释放

constexpr int kPttWaveBars = 27; // 相对原 11 条约 2.5 倍横向跨度
constexpr int kPttWaveFrames = 16;
constexpr lv_coord_t kPttWaveBarW = 2;
constexpr lv_coord_t kPttWaveBarGap = 2;
constexpr uint32_t kPttWaveFrameMs = 55;
/** 竖条最大高度（Create 时按状态栏高度的约一半写入，整体偏矮）。 */
lv_coord_t s_ptt_wave_max_h = 20;
lv_obj_t* s_ptt_wave_bars[kPttWaveBars] = {};
uint8_t s_ptt_wave_frame = 0;

/** 单周期高度曲线（相对 0..44），按 s_ptt_wave_max_h 等比缩放；中间条先动、向两侧扩散。 */
constexpr uint8_t kPttWaveCurve[kPttWaveFrames] = {
    12, 16, 22, 30, 38, 44, 42, 36, 28, 20, 14, 10, 12, 18, 26, 34,
};
bool s_a2ui_ready = false;
bool s_has_a2ui_content = false;
bool s_replaying_history = false;
const lv_font_t* s_ui_font = nullptr;
const lv_font_t* s_ui_font_bold = nullptr;

lv_coord_t s_viewport_w = 0;
lv_coord_t s_viewport_h = 0;

struct SpanRun {
    SpirString text;
    bool bold = false;
};

enum class FlowKind : uint8_t {
    Text,
    RichText,
    Math,
    Header,
    ListItem,
    Badge,
    Button,
    Status,
    Progress,
    Image,
    Divider,
    Spacer,
    CardBegin,
    CardEnd,
    MsgGap,  // 多轮之间的间距（不渲染控件）
};

struct FlowItem {
    FlowKind kind = FlowKind::Text;
    SpirString text;
    SpirString text2;
    SpirString variant = "body";
    SpirString image_url;
    SpirString action_name;
    SpirVec<SpanRun> spans;
    bool bold = false;
    bool done = false;
    bool wrap = true;
    bool center = false;     // Image/Math：父 Column/Row 或自身 align=center
    bool img_border = true;  // Image：与 a2ui 默认一致；false 时重建也要带上
    int img_w = 440;
    int img_h = 280;
    int spacer_h = 16;
    int pct = 0;
    bool math_display = false;
};

using FlowList = SpirVec<FlowItem>;
using PageList = SpirVec<FlowList>;

FlowList s_flow;
PageList s_pages;
/** 与 s_flow 等长：每项首次落入的页码；RebuildPages 填充，-1 表示未入页（如 MsgGap）。 */
SpirVec<int> s_flow_first_page;
/** 与 s_pages 等长：该页开始填充时的 s_flow 下标（供尾部增量重排）。 */
SpirVec<size_t> s_page_flow_begin;
/** 正在排版的 flow 下标；当前未入册页的首个内容 flow 下标。 */
size_t s_building_flow_fi = 0;
size_t s_open_page_flow_begin = static_cast<size_t>(-1);
int s_page_index = 0;
/** 本轮用户 ASR 在 s_flow 中的起点；AI 流式跟此锚点所在页，不跟末页。-1=无。 */
int s_pin_flow_idx = -1;

/** 保护 s_flow / s_pages / 页码；重排不持 LVGL 锁，与翻页互斥即可 */
std::mutex s_session_mu;
FlowList s_coalesce_buf;
bool s_coalesce_jump = false;
bool s_coalesce_pending = false;
esp_timer_handle_t s_coalesce_timer = nullptr;
int64_t s_last_stream_paint_us = 0;

void RenderCurrentPage();
void RebuildPages();
/** @return 本次重排起始页码（用于判断当前页是否需刷屏） */
int RebuildPagesTail();
void AppendFlowAndShow(FlowList&& chunk, bool jump_to_new);
void QueueOrShowChunk(FlowList&& chunk, bool jump_to_new);
void RequestSyncPttOverlay();
void StopListeningIfNoPttHeld(const char* why);

const lv_font_t* UiFont() {
    if (s_ui_font != nullptr) {
        return s_ui_font;
    }
    s_ui_font = fontpack_lv_font_get(kUiFontSize, kUiFontBpp);
    if (s_ui_font == nullptr) {
        ESP_LOGW(TAG, "fontpack %u/%ubpp unavailable, fallback UI 25@2", kUiFontSize, kUiFontBpp);
        s_ui_font = fontpack_lv_font_ui();
    } else {
        ESP_LOGI(TAG, "using fontpack regular size=%u req_bpp=%u got_bpp=%u", kUiFontSize,
                 kUiFontBpp, fontpack_lv_font_bpp(s_ui_font));
    }
    return s_ui_font;
}

const lv_font_t* UiFontBold() {
    if (s_ui_font_bold != nullptr) {
        return s_ui_font_bold;
    }
    s_ui_font_bold = fontpack_lv_font_get(kUiFontSize, kUiFontBoldBpp);
    if (s_ui_font_bold == nullptr) {
        ESP_LOGW(TAG, "fontpack bold %u/%ubpp unavailable, fallback regular", kUiFontSize,
                 kUiFontBoldBpp);
        s_ui_font_bold = UiFont();
    } else {
        ESP_LOGI(TAG, "using fontpack bold size=%u req_bpp=%u got_bpp=%u", kUiFontSize,
                 kUiFontBoldBpp, fontpack_lv_font_bpp(s_ui_font_bold));
    }
    return s_ui_font_bold;
}

void DisableScroll(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

lv_coord_t FontLineHeight(const lv_font_t* font) {
    return font != nullptr ? font->line_height : 28;
}

lv_coord_t GlyphWidth(const lv_font_t* font, uint32_t cp) {
    if (font == nullptr) {
        return 12;
    }
    const lv_coord_t w = static_cast<lv_coord_t>(lv_font_get_glyph_width(font, cp, 0));
    return w > 0 ? w : 1;
}

lv_coord_t VariantLetterSpace(const char* variant) {
    if (variant == nullptr) {
        return 0;
    }
    if (std::strcmp(variant, "title") == 0) {
        return 2;
    }
    if (std::strcmp(variant, "subtitle") == 0) {
        return 1;
    }
    if (std::strcmp(variant, "display") == 0) {
        return 3;
    }
    return 0;
}

const lv_font_t* FontFor(bool bold) {
    return bold ? UiFontBold() : UiFont();
}

void EnsureA2ui(lv_obj_t* scr) {
    if (s_a2ui_ready) {
        return;
    }
    lv_display_t* disp = lv_obj_get_display(scr);
    if (disp == nullptr) {
        disp = lv_display_get_default();
    }
    if (disp == nullptr) {
        ESP_LOGE(TAG, "a2ui init: no lv_display");
        return;
    }
    a2ui_config_t cfg = A2UI_CONFIG_DEFAULT();
    cfg.disp = disp;
    cfg.font_regular = UiFont();
    cfg.font_bold = UiFontBold();
    cfg.on_action = nullptr;
    if (a2ui_init(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "a2ui_init failed");
        return;
    }
    s_a2ui_ready = true;
}

void HideIdleHint() {
    if (s_hint_img != nullptr && lv_obj_is_valid(s_hint_img)) {
        lv_obj_add_flag(s_hint_img, LV_OBJ_FLAG_HIDDEN);
    }
}

void ShowIdleHint() {
    if (s_hint_img == nullptr || !lv_obj_is_valid(s_hint_img) || s_hint_raster == nullptr) {
        return;
    }
    lv_obj_remove_flag(s_hint_img, LV_OBJ_FLAG_HIDDEN);
}

/** 从固件嵌入（en）或 resources（zh）解码空状态全屏图到 L8。失败则控件保持隐藏。 */
void LoadIdleHintImage(lv_obj_t* img) {
    if (img == nullptr) {
        return;
    }

    const uint8_t* mem = nullptr;
    size_t size = 0;
    const bool use_en = (Lang::CODE != nullptr && std::strcmp(Lang::CODE, "en-US") == 0);
    if (use_en) {
        mem = ic_s_assistant_hint_en_a2i1_start;
        size = static_cast<size_t>(ic_s_assistant_hint_en_a2i1_end - ic_s_assistant_hint_en_a2i1_start);
        if (mem == nullptr || size == 0) {
            ESP_LOGW(TAG, "idle hint: embedded en a2i1 empty");
            return;
        }
    } else {
        auto* disp = LVAdapterDisplay::Instance();
        if (disp == nullptr) {
            ESP_LOGW(TAG, "idle hint: no display");
            return;
        }
        if (!disp->TryGetResource(kIdleHintAssetZh, &mem, &size) || mem == nullptr || size == 0) {
            ESP_LOGW(TAG, "idle hint: missing %s (rebuild resources?)", kIdleHintAssetZh);
            return;
        }
    }

    auto* raster = new reader::RasterImage();
    if (!reader::DecodeImageToL8(mem, size, kIdleHintMaxW, kIdleHintMaxH, *raster) ||
        raster->empty()) {
        ESP_LOGW(TAG, "idle hint: decode failed (%s)", use_en ? "en-embed" : "zh-res");
        delete raster;
        return;
    }
    s_hint_raster = raster;
    lv_obj_set_user_data(img, raster);
    lv_image_set_src(img, &raster->dsc);
    lv_obj_set_size(img, raster->width, raster->height);
    lv_obj_align(img, LV_ALIGN_TOP_LEFT, 0, 0);
    ESP_LOGI(TAG, "idle hint ready %ux%u (%s)", static_cast<unsigned>(raster->width),
             static_cast<unsigned>(raster->height), use_en ? "en-embed" : "zh-res");
}

/** PTT 任一源仍按住（BOOT 任意按下 ∪ 屏触已臂听）。跨线程可读。 */
bool IsAnyPttHeld() {
    return BootKey_IsHeld() || s_touch_ptt_held.load(std::memory_order_acquire);
}

/** PTT 状态栏波形：仍按住，且已进 Listening（联网/通道就绪后），Connecting 期间不显示。 */
bool IsPttWaveWanted() {
    if (Application::GetInstance().GetDeviceState() != kDeviceStateListening) {
        return false;
    }
    if (s_touch_ptt_held.load(std::memory_order_acquire)) {
        return true;
    }
    return BootKey_IsHeld() && BootKey_DidLongPress();
}

/**
 * 某一 PTT 源刚松开后：若其它源仍持有则保持聆听，否则 StopListening。
 * 调用方须先清掉本源标志（或 BOOT 层已清 IsHeld），再调本函数。
 */
void StopListeningIfNoPttHeld(const char* why) {
    RequestSyncPttOverlay();
    if (IsAnyPttHeld()) {
        ESP_LOGI(TAG, "%s: other PTT still held, keep listening", why);
        return;
    }
    ESP_LOGI(TAG, "%s -> StopListening", why);
    Application::GetInstance().StopListening();
}

void CancelTouchPttArmTimer() {
    if (s_touch_ptt_arm_timer == nullptr) {
        return;
    }
    lv_timer_delete(s_touch_ptt_arm_timer);
    s_touch_ptt_arm_timer = nullptr;
}

void ResetTouchPttState() {
    CancelTouchPttArmTimer();
    s_touch_ptt_finger_down = false;
    s_touch_suppress_click = false;
    s_touch_ptt_held.store(false, std::memory_order_release);
}

void ArmTouchPttFromTimer(lv_timer_t* /*t*/) {
    // one-shot：先摘掉句柄，避免 Release/Unload 二次 delete
    s_touch_ptt_arm_timer = nullptr;
    if (!s_active.load(std::memory_order_acquire) || !s_touch_ptt_finger_down) {
        return;
    }
    // 重绘堵住 LVGL 时 timer 可能迟到：抬起已进 touch_feed 则丢弃
    if (!TouchUiFingerIsDown()) {
        ESP_LOGI(TAG, "touch PTT arm ignored: finger already up");
        s_touch_ptt_finger_down = false;
        return;
    }
    if (s_touch_ptt_held.exchange(true, std::memory_order_acq_rel)) {
        return;  // 已臂听（重复 timer 不应发生）
    }
    s_touch_suppress_click = true;
    // 按下已由 HapticAttachClick 早震，臂听不再二次震（与阅读长按出浮层同）
    ESP_LOGI(TAG, "touch long-press (%ums) -> StartListening (same as BOOT PTT)",
             static_cast<unsigned>(kTouchPttLongPressMs));
    RequestSyncPttOverlay();
    Application::GetInstance().StartListening();
}

void OnTouchPttPressed() {
    if (!s_active.load(std::memory_order_acquire)) {
        return;
    }
    s_touch_ptt_finger_down = true;
    CancelTouchPttArmTimer();
    s_touch_ptt_arm_timer =
        lv_timer_create(ArmTouchPttFromTimer, kTouchPttLongPressMs, nullptr);
    if (s_touch_ptt_arm_timer == nullptr) {
        ESP_LOGW(TAG, "touch PTT: lv_timer_create failed");
        s_touch_ptt_finger_down = false;
        return;
    }
    lv_timer_set_repeat_count(s_touch_ptt_arm_timer, 1);
}

void OnTouchPttReleased(const char* why) {
    s_touch_ptt_finger_down = false;
    CancelTouchPttArmTimer();
    if (!s_touch_ptt_held.exchange(false, std::memory_order_acq_rel)) {
        return;  // 未满阈值抬起：由 CLICKED 处理翻页
    }
    StopListeningIfNoPttHeld(why);
}

void RequestRenderCurrentPage();

bool TouchEventPoint(lv_event_t* e, lv_point_t* out) {
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

/** dir: -1 上一页，+1 下一页。已在首页时上一页返回 false；末页下一页 no-op 仍返回 true。 */
bool AssistantTurnPage(int dir) {
    {
        std::lock_guard<std::mutex> g(s_session_mu);
        if (!s_has_a2ui_content || s_pages.empty()) {
            return dir > 0;
        }
        if (dir < 0) {
            if (s_page_index <= 0) {
                return false;
            }
            --s_page_index;
            ESP_LOGI(TAG, "page turn -> %d/%d", s_page_index + 1, static_cast<int>(s_pages.size()));
        } else if (dir > 0) {
            if (s_page_index + 1 < static_cast<int>(s_pages.size())) {
                ++s_page_index;
                ESP_LOGI(TAG, "page turn -> %d/%d", s_page_index + 1, static_cast<int>(s_pages.size()));
            } else {
                return true;
            }
        } else {
            return false;
        }
    }
    RequestRenderCurrentPage();
    return true;
}

void OnTouchPageClicked(lv_event_t* e) {
    if (lv_event_get_target(e) != s_touch_ptt_hit) {
        return;
    }
    if (s_touch_suppress_click) {
        s_touch_suppress_click = false;
        return;
    }
    if (!s_has_a2ui_content || s_pages.empty()) {
        return;
    }
    // 震动已在按下早震；此处只投递翻页（与阅读：AttachClick 早震 + CLICKED 换页 一致）
    // 左 1/3 上一页，右 2/3 下一页；首页再点左区停住，不退出
    lv_point_t pt = {};
    const int dir = (TouchEventPoint(e, &pt) && pt.x < LV_HOR_RES / 3) ? -1 : 1;
    AssistantTurnPage(dir);
}

void OnTouchPttEvent(lv_event_t* e) {
    if (lv_event_get_target(e) != s_touch_ptt_hit) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        OnTouchPttPressed();
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        OnTouchPttReleased(code == LV_EVENT_PRESS_LOST ? "touch press-lost"
                                                       : "touch press-up");
    }
}

lv_obj_t* CreateTouchPttHitLayer(lv_obj_t* scr) {
    // 全屏透明 hit：叠在内容之上吃 pointer；非 clickable 的顶栏字/波形仍可穿透到本层。
    // 盖板虚拟键在 touch_feed 坐标命中，不走 LVGL hit，故不受本层影响。
    lv_obj_t* hit = lv_obj_create(scr);
    lv_obj_set_size(hit, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(hit, 0, 0);
    lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hit, 0, 0);
    lv_obj_set_style_pad_all(hit, 0, 0);
    lv_obj_set_style_radius(hit, 0, 0);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(hit, LV_OBJ_FLAG_GESTURE_BUBBLE);
    DisableScroll(hit);
    // 页内热区：touch_feed 按下早震；短按翻页 / 长按 PTT 业务侧勿再 Pulse
    HapticAttachClick(hit);
    lv_obj_add_event_cb(hit, OnTouchPttEvent, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(hit, OnTouchPttEvent, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(hit, OnTouchPttEvent, LV_EVENT_PRESS_LOST, nullptr);
    lv_obj_add_event_cb(hit, OnTouchPageClicked, LV_EVENT_CLICKED, nullptr);
    return hit;
}

lv_coord_t PttWaveBarHeight(uint8_t curve_v) {
    if (s_ptt_wave_max_h <= 0) {
        return 2;
    }
    lv_coord_t h = static_cast<lv_coord_t>((static_cast<int>(curve_v) * s_ptt_wave_max_h) / 44);
    if (h < 2) {
        h = 2;
    }
    if (h > s_ptt_wave_max_h) {
        h = s_ptt_wave_max_h;
    }
    return h;
}

lv_obj_t* CreatePttWaveBar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_width(bar, kPttWaveBarW);
    lv_obj_set_height(bar, 2);
    lv_obj_set_style_bg_color(bar, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 1, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(bar);
    return bar;
}

lv_obj_t* CreatePttWaveHost(lv_obj_t* parent, lv_coord_t bar_h) {
    lv_obj_t* host = lv_obj_create(parent);
    lv_obj_set_size(host, LV_SIZE_CONTENT, bar_h);
    lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(host, 0, 0);
    lv_obj_set_style_pad_all(host, 0, 0);
    lv_obj_set_style_pad_column(host, kPttWaveBarGap, 0);
    lv_obj_set_style_radius(host, 0, 0);
    lv_obj_set_flex_flow(host, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(host, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(host, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(host);
    for (int i = 0; i < kPttWaveBars; ++i) {
        s_ptt_wave_bars[i] = CreatePttWaveBar(host);
    }
    return host;
}

void UpdatePttWaveBars() {
    constexpr int kCenter = kPttWaveBars / 2;
    for (int i = 0; i < kPttWaveBars; ++i) {
        if (s_ptt_wave_bars[i] == nullptr || !lv_obj_is_valid(s_ptt_wave_bars[i])) {
            continue;
        }
        const int dist = (i >= kCenter) ? (i - kCenter) : (kCenter - i);
        const int phase = (s_ptt_wave_frame + dist) % kPttWaveFrames;
        lv_obj_set_height(s_ptt_wave_bars[i], PttWaveBarHeight(kPttWaveCurve[phase]));
    }
}

void OnPttWaveTimer(lv_timer_t* /*t*/) {
    if (!s_ptt_wave_visible) {
        return;
    }
    s_ptt_wave_frame = static_cast<uint8_t>((s_ptt_wave_frame + 1) % kPttWaveFrames);
    UpdatePttWaveBars();
}

void StopPttWaveAnim() {
    if (s_ptt_wave_timer != nullptr) {
        lv_timer_delete(s_ptt_wave_timer);
        s_ptt_wave_timer = nullptr;
    }
    s_ptt_wave_frame = 0;
}

void StartPttWaveAnim() {
    StopPttWaveAnim();
    for (int i = 0; i < kPttWaveBars; ++i) {
        if (s_ptt_wave_bars[i] == nullptr || !lv_obj_is_valid(s_ptt_wave_bars[i])) {
            return;
        }
    }
    UpdatePttWaveBars();
    s_ptt_wave_timer = lv_timer_create(OnPttWaveTimer, kPttWaveFrameMs, nullptr);
    if (s_ptt_wave_timer == nullptr) {
        ESP_LOGW(TAG, "StartPttWaveAnim: lv_timer_create failed");
    }
}

void HideStatusCenterText() {
    if (s_status_label != nullptr && lv_obj_is_valid(s_status_label)) {
        lv_obj_add_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_notification_label != nullptr && lv_obj_is_valid(s_notification_label)) {
        lv_obj_add_flag(s_notification_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void RestoreStatusCenterText() {
    if (s_status_label != nullptr && lv_obj_is_valid(s_status_label)) {
        lv_obj_remove_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void ApplyPttWave(bool show) {
    if (s_ptt_wave == nullptr || !lv_obj_is_valid(s_ptt_wave)) {
        return;
    }
    if (!AssistantScreen::IsActive()) {
        return;
    }
    if (show == s_ptt_wave_visible) {
        return;
    }
    s_ptt_wave_visible = show;
    if (show) {
        HideStatusCenterText();
        lv_obj_remove_flag(s_ptt_wave, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_ptt_wave);
        StartPttWaveAnim();
    } else {
        StopPttWaveAnim();
        lv_obj_add_flag(s_ptt_wave, LV_OBJ_FLAG_HIDDEN);
        RestoreStatusCenterText();
    }
}

void AsyncSyncPttOverlay(void* /*arg*/) {
    ApplyPttWave(IsPttWaveWanted());
}

void RequestSyncPttOverlay() {
    // BOOT/触摸回调不在 LVGL 任务：经 ScreenLvAsync，勿在本线程死等锁
    ScreenLvAsync(AsyncSyncPttOverlay);
}

void AdjustPinAfterDrop(size_t drop) {
    if (s_pin_flow_idx < 0) {
        return;
    }
    if (static_cast<size_t>(s_pin_flow_idx) < drop) {
        s_pin_flow_idx = 0;
    } else {
        s_pin_flow_idx -= static_cast<int>(drop);
    }
}

/** 总页超过 kMaxPages 时从 flow 头丢掉旧内容，保留最近 kMaxPages 页。
 *  @return true 发生了裁剪并全量重排
 */
bool TrimFlowToMaxPages() {
    if (static_cast<int>(s_pages.size()) <= kMaxPages) {
        return false;
    }
    const int keep_from_page = static_cast<int>(s_pages.size()) - kMaxPages;
    size_t keep_from = s_flow.size();
    for (size_t i = 0; i < s_flow.size(); ++i) {
        const int fp = (i < s_flow_first_page.size()) ? s_flow_first_page[i] : -1;
        if (fp >= keep_from_page) {
            keep_from = i;
            break;
        }
    }
    if (keep_from == 0 || keep_from >= s_flow.size()) {
        return false;
    }
    s_flow.erase(s_flow.begin(),
                 s_flow.begin() + static_cast<std::ptrdiff_t>(keep_from));
    AdjustPinAfterDrop(keep_from);
    ESP_LOGW(TAG, "flow trimmed for max %d pages (drop %u items, remain %u)", kMaxPages,
             static_cast<unsigned>(keep_from), static_cast<unsigned>(s_flow.size()));
    RebuildPages();
    return true;
}

/** 按视口宽把 UTF-8 文本折成物理行（含 letter_space）。 */
void WrapTextToLines(std::string_view text, const lv_font_t* font, lv_coord_t letter_space,
                     lv_coord_t viewport_w, std::vector<std::string>& lines) {
    lines.clear();
    std::string line;
    lv_coord_t line_w = 0;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(text.data());
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\n') {
            lines.push_back(std::move(line));
            line.clear();
            line_w = 0;
            ++i;
            continue;
        }
        uint32_t cp = 0;
        const size_t n = reader::Utf8Next(data + i, text.size() - i, &cp);
        if (n == 0) {
            break;
        }
        const lv_coord_t cw = GlyphWidth(font, cp) + (line.empty() ? 0 : letter_space);
        if (!line.empty() && line_w + cw > viewport_w) {
            lines.push_back(std::move(line));
            line.clear();
            line_w = 0;
        }
        if (line.empty()) {
            line_w = GlyphWidth(font, cp);
        } else {
            line_w += cw;
        }
        line.append(text.data() + i, n);
        i += n;
    }
    if (!line.empty() || lines.empty()) {
        lines.push_back(std::move(line));
    }
}

std::string JoinLines(const std::vector<std::string>& lines, size_t begin, size_t end) {
    std::string out;
    for (size_t i = begin; i < end && i < lines.size(); ++i) {
        if (i > begin) {
            out.push_back('\n');
        }
        out += lines[i];
    }
    return out;
}

/** 按字节范围切片 RichText spans（UTF-8 字节偏移）。 */
SpirVec<SpanRun> SliceSpans(const SpirVec<SpanRun>& spans, size_t byte_begin,
                            size_t byte_end) {
    SpirVec<SpanRun> out;
    size_t off = 0;
    for (const auto& sp : spans) {
        const size_t sp_begin = off;
        const size_t sp_end = off + sp.text.size();
        off = sp_end;
        if (sp_end <= byte_begin || sp_begin >= byte_end) {
            continue;
        }
        const size_t a = byte_begin > sp_begin ? byte_begin - sp_begin : 0;
        const size_t b = byte_end < sp_end ? byte_end - sp_begin : sp.text.size();
        if (a >= b) {
            continue;
        }
        SpanRun cut;
        cut.bold = sp.bold;
        cut.text = sp.text.substr(a, b - a);
        out.push_back(std::move(cut));
    }
    return out;
}

std::string SpansConcat(const SpirVec<SpanRun>& spans) {
    std::string s;
    for (const auto& sp : spans) {
        s.append(sp.text.data(), sp.text.size());
    }
    return s;
}

lv_coord_t EstimateItemHeight(const FlowItem& item, lv_coord_t viewport_w, lv_coord_t viewport_h) {
    const lv_coord_t line_h = FontLineHeight(UiFont());
    switch (item.kind) {
        case FlowKind::MsgGap:
            return kBlockGap;
        case FlowKind::Divider:
            return 2;
        case FlowKind::Spacer:
            return item.spacer_h > 0 ? item.spacer_h : 16;
        case FlowKind::CardBegin:
            return 12;  // pad_top 近似；真正高度在子项累计
        case FlowKind::CardEnd:
            return 12;
        case FlowKind::Image: {
            lv_coord_t h = item.img_h > 0 ? item.img_h : 280;
            if (h > viewport_h) {
                h = viewport_h;
            }
            if (h < 40) {
                h = 40;
            }
            return h;
        }
        case FlowKind::Button:
            return 56;
        case FlowKind::Badge:
            return line_h + 8;
        case FlowKind::Progress:
            return line_h + 6 + 18;
        case FlowKind::Status:
            return line_h;
        case FlowKind::Header: {
            lv_coord_t h = line_h + 10;
            if (!item.text2.empty()) {
                h += line_h + 2;
            }
            return h;
        }
        case FlowKind::ListItem: {
            lv_coord_t h = line_h + 20;
            if (!item.text2.empty()) {
                h += line_h + 2;
            }
            return h;
        }
        case FlowKind::Text: {
            std::vector<std::string> lines;
            WrapTextToLines(item.text, FontFor(item.bold), VariantLetterSpace(item.variant.c_str()),
                            viewport_w, lines);
            return static_cast<lv_coord_t>(lines.size()) * line_h;
        }
        case FlowKind::RichText: {
            std::vector<std::string> lines;
            // 折行宽度按 regular 估算（bold 略宽，偏保守多留一行）
            WrapTextToLines(SpansConcat(item.spans), UiFont(),
                            VariantLetterSpace(item.variant.c_str()), viewport_w, lines);
            return static_cast<lv_coord_t>(lines.size()) * line_h;
        }
        case FlowKind::Math: {
            int32_t h = a2ui_math_measure_height(item.text.c_str(), item.math_display, viewport_w);
            if (h < line_h) {
                h = line_h * 2;
            }
            if (h > viewport_h) {
                h = viewport_h;
            }
            return static_cast<lv_coord_t>(h);
        }
    }
    return line_h;
}

bool IsTextLike(FlowKind k) {
    return k == FlowKind::Text || k == FlowKind::RichText;
}

void ReopenCards(FlowList& cur, int card_depth, lv_coord_t& y) {
    for (int i = 0; i < card_depth; ++i) {
        FlowItem begin;
        begin.kind = FlowKind::CardBegin;
        cur.push_back(std::move(begin));
        y += 12;
    }
}

void NotePageContent(size_t fi) {
    if (s_open_page_flow_begin == static_cast<size_t>(-1)) {
        s_open_page_flow_begin = fi;
    }
}

void CommitPage(FlowList& cur) {
    if (cur.empty()) {
        return;
    }
    const size_t begin =
        s_open_page_flow_begin == static_cast<size_t>(-1) ? s_building_flow_fi : s_open_page_flow_begin;
    s_pages.push_back(std::move(cur));
    s_page_flow_begin.push_back(begin);
    s_open_page_flow_begin = static_cast<size_t>(-1);
    cur.clear();
}

void PushPage(FlowList& cur, int card_depth, lv_coord_t& y) {
    if (cur.empty()) {
        return;
    }
    CommitPage(cur);
    y = 0;
    ReopenCards(cur, card_depth, y);
}

int CardDepthBefore(size_t fi) {
    int d = 0;
    for (size_t i = 0; i < fi && i < s_flow.size(); ++i) {
        if (s_flow[i].kind == FlowKind::CardBegin) {
            ++d;
        } else if (s_flow[i].kind == FlowKind::CardEnd && d > 0) {
            --d;
        }
    }
    return d;
}

/** 把可折行文本块按行装进页面，保证不半行截断。first_page_out：本项首次入页的页码。 */
void PaginateTextLike(const FlowItem& item, FlowList& cur, lv_coord_t& y,
                      lv_coord_t viewport_w, lv_coord_t viewport_h, int card_depth,
                      int* first_page_out) {
    const lv_coord_t line_h = FontLineHeight(UiFont());
    std::vector<std::string> lines;
    std::string full;
    if (item.kind == FlowKind::RichText) {
        full = SpansConcat(item.spans);
        WrapTextToLines(full, UiFont(), VariantLetterSpace(item.variant.c_str()), viewport_w, lines);
    } else {
        full.assign(item.text.data(), item.text.size());
        WrapTextToLines(item.text, FontFor(item.bold), VariantLetterSpace(item.variant.c_str()),
                        viewport_w, lines);
    }

    // 与 Wrap 同步扫描：建立每物理行在 full 中的字节区间
    std::vector<std::pair<size_t, size_t>> ranges;
    ranges.reserve(lines.size());
    size_t scan = 0;
    for (size_t li = 0; li < lines.size(); ++li) {
        const std::string& ln = lines[li];
        if (ln.empty()) {
            ranges.push_back({scan, scan});
            if (scan < full.size() && full[scan] == '\n') {
                ++scan;
            }
            continue;
        }
        if (scan + ln.size() <= full.size() && full.compare(scan, ln.size(), ln) == 0) {
            ranges.push_back({scan, scan + ln.size()});
            scan += ln.size();
        } else {
            const size_t begin = full.find(ln, scan);
            if (begin == std::string::npos) {
                ranges.push_back({scan, scan});
            } else {
                ranges.push_back({begin, begin + ln.size()});
                scan = begin + ln.size();
            }
        }
        if (scan < full.size() && full[scan] == '\n') {
            ++scan;
        }
    }

    size_t i = 0;
    while (i < lines.size()) {
        if (y + line_h > viewport_h && !cur.empty()) {
            PushPage(cur, card_depth, y);
        }
        size_t take = 0;
        lv_coord_t used = 0;
        while (i + take < lines.size()) {
            if (y + used + line_h > viewport_h) {
                break;
            }
            used += line_h;
            ++take;
        }
        if (take == 0) {
            take = 1;
            used = line_h;
        }

        FlowItem slice = item;
        if (item.kind == FlowKind::RichText) {
            const size_t b0 = ranges[i].first;
            const size_t b1 = ranges[i + take - 1].second;
            slice.spans = SliceSpans(item.spans, b0, b1);
            slice.text.clear();
        } else {
            const std::string joined = JoinLines(lines, i, i + take);
            slice.text.assign(joined.data(), joined.size());
        }
        cur.push_back(std::move(slice));
        NotePageContent(s_building_flow_fi);
        if (first_page_out != nullptr && *first_page_out < 0) {
            *first_page_out = static_cast<int>(s_pages.size());
        }
        y += used;
        i += take;
        if (i < lines.size()) {
            PushPage(cur, card_depth, y);
        }
    }
}

// 拆出循环体：避免 xtensa-gcc 在 RebuildPages 的 lambda+CFG 上 ICE（fwprop1/try_forward_edges）
void EnsureRoom(FlowList& cur, lv_coord_t& y, int card_depth, lv_coord_t vh, lv_coord_t need) {
    if (y + need > vh && !cur.empty()) {
        PushPage(cur, card_depth, y);
    }
}

void MarkFlowFirstPage(size_t fi) {
    NotePageContent(fi);
    if (fi < s_flow_first_page.size() && s_flow_first_page[fi] < 0) {
        s_flow_first_page[fi] = static_cast<int>(s_pages.size());
    }
}

void AppendOneFlowItem(size_t fi, FlowList& cur, lv_coord_t& y, int& card_depth, lv_coord_t vw,
                       lv_coord_t vh) {
    s_building_flow_fi = fi;
    const auto& item = s_flow[fi];
    if (item.kind == FlowKind::MsgGap) {
        if (!cur.empty()) {
            y += kBlockGap;
        }
        return;
    }
    if (item.kind == FlowKind::CardBegin) {
        EnsureRoom(cur, y, card_depth, vh, 12 + FontLineHeight(UiFont()));
        MarkFlowFirstPage(fi);
        cur.push_back(item);
        ++card_depth;
        y += 12;
        return;
    }
    if (item.kind == FlowKind::CardEnd) {
        MarkFlowFirstPage(fi);
        cur.push_back(item);
        if (card_depth > 0) {
            --card_depth;
        }
        y += 12;
        return;
    }
    if (IsTextLike(item.kind)) {
        PaginateTextLike(item, cur, y, vw, vh, card_depth, &s_flow_first_page[fi]);
        if (!cur.empty()) {
            y += kBlockGap;
        }
        return;
    }

    const lv_coord_t h = EstimateItemHeight(item, vw, vh);
    EnsureRoom(cur, y, card_depth, vh, h);
    if (h > vh && !cur.empty()) {
        PushPage(cur, card_depth, y);
    }
    MarkFlowFirstPage(fi);
    cur.push_back(item);
    y += h + kBlockGap;
}

void ClampPageIndex() {
    if (s_page_index >= static_cast<int>(s_pages.size())) {
        s_page_index = static_cast<int>(s_pages.size()) - 1;
    }
    if (s_page_index < 0) {
        s_page_index = 0;
    }
}

void RebuildPages() {
    s_pages.clear();
    s_page_flow_begin.clear();
    s_open_page_flow_begin = static_cast<size_t>(-1);
    s_flow_first_page.assign(s_flow.size(), -1);
    const lv_coord_t vw = s_viewport_w > 0 ? s_viewport_w : (LV_HOR_RES - kContentPad * 2);
    const lv_coord_t vh =
        s_viewport_h > 0 ? s_viewport_h : (LV_VER_RES - kFooterH - kContentPad * 2);

    FlowList cur;
    lv_coord_t y = 0;
    int card_depth = 0;
    for (size_t fi = 0; fi < s_flow.size(); ++fi) {
        AppendOneFlowItem(fi, cur, y, card_depth, vw, vh);
    }
    CommitPage(cur);
    if (s_pages.empty()) {
        s_pages.push_back({});
        s_page_flow_begin.push_back(0);
    }
    ClampPageIndex();
    ESP_LOGI(TAG, "paginated %u pages (flow=%u, vw=%d vh=%d)",
             static_cast<unsigned>(s_pages.size()), static_cast<unsigned>(s_flow.size()),
             static_cast<int>(vw), static_cast<int>(vh));
}

int RebuildPagesTail() {
    if (s_pages.empty() || s_page_flow_begin.size() != s_pages.size()) {
        RebuildPages();
        return 0;
    }
    size_t fi0 = s_page_flow_begin.back();
    if (fi0 >= s_flow.size()) {
        RebuildPages();
        return 0;
    }
    int page0 = static_cast<int>(s_pages.size()) - 1;
    // 末页若是长文续页，须退回到该 flow 项首次入页处再排
    if (fi0 < s_flow_first_page.size() && s_flow_first_page[fi0] >= 0 &&
        s_flow_first_page[fi0] < page0) {
        page0 = s_flow_first_page[fi0];
        fi0 = s_page_flow_begin[static_cast<size_t>(page0)];
    }
    if (page0 < 0 || static_cast<size_t>(page0) > s_pages.size() || fi0 >= s_flow.size()) {
        RebuildPages();
        return 0;
    }

    s_pages.resize(static_cast<size_t>(page0));
    s_page_flow_begin.resize(static_cast<size_t>(page0));
    s_open_page_flow_begin = static_cast<size_t>(-1);

    if (s_flow_first_page.size() < s_flow.size()) {
        s_flow_first_page.resize(s_flow.size(), -1);
    }
    for (size_t i = 0; i < s_flow_first_page.size(); ++i) {
        if (i >= fi0 || s_flow_first_page[i] >= page0) {
            s_flow_first_page[i] = -1;
        }
    }

    const lv_coord_t vw = s_viewport_w > 0 ? s_viewport_w : (LV_HOR_RES - kContentPad * 2);
    const lv_coord_t vh =
        s_viewport_h > 0 ? s_viewport_h : (LV_VER_RES - kFooterH - kContentPad * 2);

    FlowList cur;
    lv_coord_t y = 0;
    int card_depth = CardDepthBefore(fi0);
    if (card_depth > 0) {
        ReopenCards(cur, card_depth, y);
    }
    for (size_t fi = fi0; fi < s_flow.size(); ++fi) {
        AppendOneFlowItem(fi, cur, y, card_depth, vw, vh);
    }
    CommitPage(cur);
    if (s_pages.empty()) {
        s_pages.push_back({});
        s_page_flow_begin.push_back(0);
        page0 = 0;
    }
    ClampPageIndex();
    ESP_LOGI(TAG, "paginated tail flow=%u page=%d -> %u pages (flow=%u)",
             static_cast<unsigned>(fi0), page0, static_cast<unsigned>(s_pages.size()),
             static_cast<unsigned>(s_flow.size()));
    return page0;
}

cJSON* MakeComp(const char* id, const char* type) {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", id);
    cJSON_AddStringToObject(o, "component", type);
    return o;
}

/** 把一页 FlowItem 编成 updateComponents，交给 a2ui 按原样式渲染。 */
bool BuildPageJson(const FlowList& page, std::string& out_json) {
    cJSON* root = cJSON_CreateObject();
    cJSON* body = cJSON_AddObjectToObject(root, "updateComponents");
    cJSON_AddStringToObject(body, "root", "root");
    cJSON* comps = cJSON_AddArrayToObject(body, "components");

    cJSON* col = MakeComp("root", "Column");
    cJSON_AddNumberToObject(col, "gap", kBlockGap);
    cJSON_AddNumberToObject(col, "padding", 0);
    cJSON* children = cJSON_AddArrayToObject(col, "children");
    cJSON_AddItemToArray(comps, col);

    std::vector<std::string> open_cards;  // card ids awaiting children
    int seq = 0;
    auto next_id = [&]() {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "n%d", seq++);
        return std::string(buf);
    };
    auto add_child_to_parent = [&](const std::string& id) {
        if (!open_cards.empty()) {
            // 挂到当前 Card：在 comps 里找到该 card 的 children 数组
            const char* card_id = open_cards.back().c_str();
            const cJSON* it = nullptr;
            cJSON_ArrayForEach(it, comps) {
                const cJSON* id_j = cJSON_GetObjectItemCaseSensitive(it, "id");
                if (cJSON_IsString(id_j) && id_j->valuestring &&
                    std::strcmp(id_j->valuestring, card_id) == 0) {
                    cJSON* ch = cJSON_GetObjectItemCaseSensitive(it, "children");
                    if (!cJSON_IsArray(ch)) {
                        ch = cJSON_AddArrayToObject(const_cast<cJSON*>(it), "children");
                    }
                    cJSON_AddItemToArray(ch, cJSON_CreateString(id.c_str()));
                    return;
                }
            }
        }
        cJSON_AddItemToArray(children, cJSON_CreateString(id.c_str()));
    };

    for (const auto& item : page) {
        if (item.kind == FlowKind::MsgGap) {
            continue;
        }
        if (item.kind == FlowKind::CardBegin) {
            const std::string id = next_id();
            cJSON* card = MakeComp(id.c_str(), "Card");
            cJSON_AddArrayToObject(card, "children");
            cJSON_AddItemToArray(comps, card);
            add_child_to_parent(id);
            open_cards.push_back(id);
            continue;
        }
        if (item.kind == FlowKind::CardEnd) {
            if (!open_cards.empty()) {
                open_cards.pop_back();
            }
            continue;
        }

        const std::string id = next_id();
        cJSON* comp = nullptr;
        switch (item.kind) {
            case FlowKind::Text:
                comp = MakeComp(id.c_str(), "Text");
                cJSON_AddStringToObject(comp, "text", item.text.c_str());
                cJSON_AddStringToObject(comp, "variant", item.variant.c_str());
                if (item.bold) {
                    cJSON_AddBoolToObject(comp, "bold", true);
                }
                if (!item.wrap) {
                    cJSON_AddBoolToObject(comp, "wrap", false);
                }
                break;
            case FlowKind::RichText: {
                comp = MakeComp(id.c_str(), "RichText");
                cJSON_AddStringToObject(comp, "variant", item.variant.c_str());
                cJSON* spans = cJSON_AddArrayToObject(comp, "spans");
                for (const auto& sp : item.spans) {
                    cJSON* s = cJSON_CreateObject();
                    cJSON_AddStringToObject(s, "text", sp.text.c_str());
                    if (sp.bold) {
                        cJSON_AddBoolToObject(s, "bold", true);
                    }
                    cJSON_AddItemToArray(spans, s);
                }
                break;
            }
            case FlowKind::Math: {
                auto fill_math = [&](cJSON* math) {
                    cJSON_AddStringToObject(math, "latex", item.text.c_str());
                    if (item.math_display) {
                        cJSON_AddBoolToObject(math, "display", true);
                    }
                    cJSON_AddNumberToObject(math, "width", s_viewport_w > 0 ? s_viewport_w : 440);
                };
                if (item.center) {
                    // 与 Image 相同：分页扁平化会丢掉嵌套 Column；居中需重建 wrap
                    const std::string wrap_id = id;
                    const std::string math_id = next_id();
                    cJSON* wrap = MakeComp(wrap_id.c_str(), "Column");
                    cJSON_AddStringToObject(wrap, "align", "center");
                    cJSON_AddNumberToObject(wrap, "gap", 0);
                    cJSON_AddNumberToObject(wrap, "padding", 0);
                    cJSON* wch = cJSON_AddArrayToObject(wrap, "children");
                    cJSON_AddItemToArray(wch, cJSON_CreateString(math_id.c_str()));
                    cJSON_AddItemToArray(comps, wrap);
                    add_child_to_parent(wrap_id);

                    cJSON* math = MakeComp(math_id.c_str(), "Math");
                    fill_math(math);
                    cJSON_AddItemToArray(comps, math);
                    continue;
                }
                comp = MakeComp(id.c_str(), "Math");
                fill_math(comp);
                break;
            }
            case FlowKind::Header:
                comp = MakeComp(id.c_str(), "Header");
                cJSON_AddStringToObject(comp, "title", item.text.c_str());
                if (!item.text2.empty()) {
                    cJSON_AddStringToObject(comp, "subtitle", item.text2.c_str());
                }
                if (item.bold) {
                    cJSON_AddBoolToObject(comp, "bold", true);
                }
                break;
            case FlowKind::ListItem:
                comp = MakeComp(id.c_str(), "ListItem");
                cJSON_AddStringToObject(comp, "text", item.text.c_str());
                if (!item.text2.empty()) {
                    cJSON_AddStringToObject(comp, "hint", item.text2.c_str());
                }
                if (item.done) {
                    cJSON_AddBoolToObject(comp, "done", true);
                }
                if (item.bold) {
                    cJSON_AddBoolToObject(comp, "bold", true);
                }
                break;
            case FlowKind::Badge:
                comp = MakeComp(id.c_str(), "Badge");
                cJSON_AddStringToObject(comp, "text", item.text.c_str());
                if (item.bold) {
                    cJSON_AddBoolToObject(comp, "bold", true);
                }
                break;
            case FlowKind::Button:
                comp = MakeComp(id.c_str(), "Button");
                cJSON_AddStringToObject(comp, "text", item.text.c_str());
                if (item.bold) {
                    cJSON_AddBoolToObject(comp, "bold", true);
                }
                if (!item.action_name.empty()) {
                    cJSON* action = cJSON_AddObjectToObject(comp, "action");
                    cJSON_AddStringToObject(action, "name", item.action_name.c_str());
                }
                break;
            case FlowKind::Status:
                comp = MakeComp(id.c_str(), "Status");
                cJSON_AddStringToObject(comp, "label", item.text.c_str());
                cJSON_AddStringToObject(comp, "value", item.text2.c_str());
                if (item.bold) {
                    cJSON_AddBoolToObject(comp, "bold", true);
                }
                break;
            case FlowKind::Progress:
                comp = MakeComp(id.c_str(), "Progress");
                if (!item.text.empty()) {
                    cJSON_AddStringToObject(comp, "label", item.text.c_str());
                }
                cJSON_AddNumberToObject(comp, "value", item.pct);
                break;
            case FlowKind::Image: {
                auto fill_image = [&](cJSON* img) {
                    cJSON_AddStringToObject(img, "url", item.image_url.c_str());
                    cJSON_AddNumberToObject(img, "width", item.img_w);
                    cJSON_AddNumberToObject(img, "height", item.img_h);
                    if (!item.img_border) {
                        cJSON_AddBoolToObject(img, "border", false);
                    }
                };
                if (item.center) {
                    // 分页扁平化会丢掉嵌套 Column；居中需重建 wrap（gap/pad=0 避免撑高）
                    const std::string wrap_id = id;
                    const std::string img_id = next_id();
                    cJSON* wrap = MakeComp(wrap_id.c_str(), "Column");
                    cJSON_AddStringToObject(wrap, "align", "center");
                    cJSON_AddNumberToObject(wrap, "gap", 0);
                    cJSON_AddNumberToObject(wrap, "padding", 0);
                    cJSON* wch = cJSON_AddArrayToObject(wrap, "children");
                    cJSON_AddItemToArray(wch, cJSON_CreateString(img_id.c_str()));
                    cJSON_AddItemToArray(comps, wrap);
                    add_child_to_parent(wrap_id);

                    cJSON* img = MakeComp(img_id.c_str(), "Image");
                    fill_image(img);
                    cJSON_AddItemToArray(comps, img);
                    continue;
                }
                comp = MakeComp(id.c_str(), "Image");
                fill_image(comp);
                break;
            }
            case FlowKind::Divider:
                comp = MakeComp(id.c_str(), "Divider");
                break;
            case FlowKind::Spacer:
                comp = MakeComp(id.c_str(), "Spacer");
                cJSON_AddNumberToObject(comp, "height", item.spacer_h);
                break;
            default:
                break;
        }
        if (comp != nullptr) {
            cJSON_AddItemToArray(comps, comp);
            add_child_to_parent(id);
        }
    }

    // 未闭合 Card：保持打开即可（跨页卡片下半页）
    char* raw = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (raw == nullptr) {
        return false;
    }
    out_json.assign(raw);
    cJSON_free(raw);
    return true;
}

void UpdatePageIndicator() {
    if (s_page_label == nullptr || !lv_obj_is_valid(s_page_label)) {
        return;
    }
    const int pages = static_cast<int>(s_pages.size());
    const int page = pages > 0 ? (s_page_index + 1) : 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d / %d", page, pages > 0 ? pages : 1);
    lv_label_set_text(s_page_label, buf);
    if (!s_has_a2ui_content) {
        lv_obj_add_flag(s_page_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_page_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void RenderCurrentPage() {
    if (s_content == nullptr || !lv_obj_is_valid(s_content)) {
        return;
    }
    if (!s_a2ui_ready) {
        EnsureA2ui(s_scr);
    }
    if (!s_a2ui_ready) {
        return;
    }
    a2ui_set_host(s_content);

    std::string json;
    {
        std::lock_guard<std::mutex> g(s_session_mu);
        if (s_pages.empty() || !s_has_a2ui_content) {
            lv_obj_clean(s_content);
            UpdatePageIndicator();
            return;
        }
        if (s_page_index < 0 || s_page_index >= static_cast<int>(s_pages.size())) {
            s_page_index = 0;
        }
        if (!BuildPageJson(s_pages[static_cast<size_t>(s_page_index)], json)) {
            ESP_LOGW(TAG, "BuildPageJson failed");
            return;
        }
    }
    const esp_err_t err = a2ui_handle_json(json.c_str());
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "a2ui_handle_json page failed: %s", esp_err_to_name(err));
    }
    a2ui_take_pending_refresh();

    // a2ui root 默认 100% 高会拉满；改为随内容，避免裁切
    if (lv_obj_get_child_cnt(s_content) > 0) {
        lv_obj_t* root = lv_obj_get_child(s_content, 0);
        if (root != nullptr) {
            lv_obj_set_height(root, LV_SIZE_CONTENT);
            DisableScroll(root);
        }
    }
    DisableScroll(s_content);
    UpdatePageIndicator();
}

/** 盖板键在 touch_feed：合并多次翻页为一次绘制（页码已同步累加）。 */
ScreenPaintCoalesce s_page_paint{};

void PaintCurrentPage() {
    RenderCurrentPage();
}

void RequestRenderCurrentPage() {
    if (s_page_paint.paint == nullptr) {
        s_page_paint.paint = PaintCurrentPage;
    }
    ScreenPaintCoalesceRequest(&s_page_paint);
}

bool AssistantPageRepeatStep(int page_delta) {
    bool ok = false;
    {
        std::lock_guard<std::mutex> g(s_session_mu);
        if (!s_has_a2ui_content || s_pages.empty() || page_delta == 0) {
            return false;
        }
        const int last = static_cast<int>(s_pages.size()) - 1;
        int next = s_page_index + page_delta;
        if (next < 0) {
            next = 0;
        } else if (next > last) {
            next = last;
        }
        if (next == s_page_index) {
            return false;
        }
        s_page_index = next;
        ESP_LOGI(TAG, "page-repeat %+d -> %d/%d", page_delta, s_page_index + 1, last + 1);
        ok = page_delta < 0 ? s_page_index > 0 : s_page_index < last;
    }
    RequestRenderCurrentPage();
    return ok;
}

void ClearSession() {
    if (s_coalesce_timer != nullptr) {
        esp_timer_stop(s_coalesce_timer);
    }
    s_coalesce_buf.clear();
    s_coalesce_jump = false;
    s_coalesce_pending = false;
    {
        std::lock_guard<std::mutex> g(s_session_mu);
        s_flow.clear();
        s_pages.clear();
        s_flow_first_page.clear();
        s_page_flow_begin.clear();
        s_open_page_flow_begin = static_cast<size_t>(-1);
        s_page_index = 0;
        s_pin_flow_idx = -1;
        s_has_a2ui_content = false;
    }
    ScreenPaintCoalesceReset(&s_page_paint);
    // 与 a2ui updateComponents / deleteSurface 一致：abort → 毁控件 → 释像素
    a2ui_image_abort_loads();
    if (s_content != nullptr && lv_obj_is_valid(s_content)) {
        lv_obj_clean(s_content);
    }
    a2ui_image_clear();
    UpdatePageIndicator();
}

bool ParseBold(const cJSON* comp) {
    const cJSON* weight = cJSON_GetObjectItemCaseSensitive(comp, "weight");
    if (cJSON_IsString(weight) && weight->valuestring) {
        if (std::strcmp(weight->valuestring, "bold") == 0 ||
            std::strcmp(weight->valuestring, "700") == 0) {
            return true;
        }
    }
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(comp, "bold"));
}

cJSON* FindComponentById(cJSON* comps, const char* id) {
    if (!cJSON_IsArray(comps) || id == nullptr) {
        return nullptr;
    }
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, comps) {
        if (!cJSON_IsObject(item)) {
            continue;
        }
        const cJSON* id_j = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (cJSON_IsString(id_j) && id_j->valuestring && std::strcmp(id_j->valuestring, id) == 0) {
            return const_cast<cJSON*>(item);
        }
    }
    return nullptr;
}

bool ParseAlignCenter(const cJSON* comp) {
    const cJSON* align = cJSON_GetObjectItemCaseSensitive(comp, "align");
    return cJSON_IsString(align) && align->valuestring &&
           std::strcmp(align->valuestring, "center") == 0;
}

void WalkToFlow(cJSON* comps, cJSON* comp, FlowList& out, bool parent_center = false) {
    if (!cJSON_IsObject(comp)) {
        return;
    }
    const cJSON* type_j = cJSON_GetObjectItemCaseSensitive(comp, "component");
    if (!cJSON_IsString(type_j) || type_j->valuestring == nullptr) {
        return;
    }
    const char* type = type_j->valuestring;

    if (std::strcmp(type, "Column") == 0 || std::strcmp(type, "Row") == 0) {
        const bool center = parent_center || ParseAlignCenter(comp);
        const cJSON* children = cJSON_GetObjectItemCaseSensitive(comp, "children");
        if (cJSON_IsArray(children)) {
            const cJSON* cid = nullptr;
            cJSON_ArrayForEach(cid, children) {
                if (cJSON_IsString(cid) && cid->valuestring) {
                    WalkToFlow(comps, FindComponentById(comps, cid->valuestring), out, center);
                }
            }
        }
        const cJSON* child_one = cJSON_GetObjectItemCaseSensitive(comp, "child");
        if (cJSON_IsString(child_one) && child_one->valuestring) {
            WalkToFlow(comps, FindComponentById(comps, child_one->valuestring), out, center);
        }
        return;
    }

    if (std::strcmp(type, "Card") == 0) {
        FlowItem begin;
        begin.kind = FlowKind::CardBegin;
        out.push_back(std::move(begin));
        const cJSON* children = cJSON_GetObjectItemCaseSensitive(comp, "children");
        if (cJSON_IsArray(children)) {
            const cJSON* cid = nullptr;
            cJSON_ArrayForEach(cid, children) {
                if (cJSON_IsString(cid) && cid->valuestring) {
                    WalkToFlow(comps, FindComponentById(comps, cid->valuestring), out,
                               parent_center);
                }
            }
        }
        const cJSON* child_one = cJSON_GetObjectItemCaseSensitive(comp, "child");
        if (cJSON_IsString(child_one) && child_one->valuestring) {
            WalkToFlow(comps, FindComponentById(comps, child_one->valuestring), out, parent_center);
        }
        FlowItem end;
        end.kind = FlowKind::CardEnd;
        out.push_back(std::move(end));
        return;
    }

    FlowItem item;
    if (std::strcmp(type, "Text") == 0) {
        item.kind = FlowKind::Text;
        const cJSON* text = cJSON_GetObjectItemCaseSensitive(comp, "text");
        const cJSON* variant = cJSON_GetObjectItemCaseSensitive(comp, "variant");
        const cJSON* wrap = cJSON_GetObjectItemCaseSensitive(comp, "wrap");
        item.text = cJSON_IsString(text) ? text->valuestring : "";
        item.variant = cJSON_IsString(variant) ? variant->valuestring : "body";
        item.bold = ParseBold(comp);
        item.wrap = !cJSON_IsFalse(wrap);
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "RichText") == 0) {
        item.kind = FlowKind::RichText;
        const cJSON* variant = cJSON_GetObjectItemCaseSensitive(comp, "variant");
        item.variant = cJSON_IsString(variant) ? variant->valuestring : "body";
        const cJSON* arr = cJSON_GetObjectItemCaseSensitive(comp, "spans");
        if (cJSON_IsArray(arr)) {
            const cJSON* sp = nullptr;
            cJSON_ArrayForEach(sp, arr) {
                if (!cJSON_IsObject(sp)) {
                    continue;
                }
                const cJSON* text = cJSON_GetObjectItemCaseSensitive(sp, "text");
                if (!cJSON_IsString(text) || !text->valuestring) {
                    continue;
                }
                SpanRun run;
                run.text = text->valuestring;
                run.bold = ParseBold(sp);
                item.spans.push_back(std::move(run));
            }
        }
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "Header") == 0) {
        item.kind = FlowKind::Header;
        const cJSON* title = cJSON_GetObjectItemCaseSensitive(comp, "title");
        const cJSON* subtitle = cJSON_GetObjectItemCaseSensitive(comp, "subtitle");
        item.text = cJSON_IsString(title) ? title->valuestring : "";
        item.text2 = cJSON_IsString(subtitle) ? subtitle->valuestring : "";
        item.bold = ParseBold(comp);
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "ListItem") == 0) {
        item.kind = FlowKind::ListItem;
        const cJSON* text = cJSON_GetObjectItemCaseSensitive(comp, "text");
        const cJSON* hint = cJSON_GetObjectItemCaseSensitive(comp, "hint");
        item.text = cJSON_IsString(text) ? text->valuestring : "";
        item.text2 = cJSON_IsString(hint) ? hint->valuestring : "";
        item.done = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(comp, "done"));
        item.bold = ParseBold(comp);
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "Badge") == 0) {
        item.kind = FlowKind::Badge;
        const cJSON* text = cJSON_GetObjectItemCaseSensitive(comp, "text");
        item.text = cJSON_IsString(text) ? text->valuestring : "";
        item.bold = ParseBold(comp);
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "Button") == 0) {
        item.kind = FlowKind::Button;
        const cJSON* text = cJSON_GetObjectItemCaseSensitive(comp, "text");
        item.text = cJSON_IsString(text) ? text->valuestring : "OK";
        item.bold = ParseBold(comp);
        const cJSON* action = cJSON_GetObjectItemCaseSensitive(comp, "action");
        const cJSON* aname = action ? cJSON_GetObjectItemCaseSensitive(action, "name") : nullptr;
        if (cJSON_IsString(aname) && aname->valuestring) {
            item.action_name = aname->valuestring;
        }
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "Status") == 0) {
        item.kind = FlowKind::Status;
        const cJSON* label = cJSON_GetObjectItemCaseSensitive(comp, "label");
        const cJSON* value = cJSON_GetObjectItemCaseSensitive(comp, "value");
        item.text = cJSON_IsString(label) ? label->valuestring : "";
        item.text2 = cJSON_IsString(value) ? value->valuestring : "";
        item.bold = ParseBold(comp);
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "Progress") == 0) {
        item.kind = FlowKind::Progress;
        const cJSON* label = cJSON_GetObjectItemCaseSensitive(comp, "label");
        const cJSON* value = cJSON_GetObjectItemCaseSensitive(comp, "value");
        item.text = cJSON_IsString(label) ? label->valuestring : "";
        item.pct = cJSON_IsNumber(value) ? value->valueint : 0;
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "Math") == 0 || std::strcmp(type, "Formula") == 0) {
        item.kind = FlowKind::Math;
        const cJSON* latex = cJSON_GetObjectItemCaseSensitive(comp, "latex");
        if (!cJSON_IsString(latex) || !latex->valuestring) {
            latex = cJSON_GetObjectItemCaseSensitive(comp, "text");
        }
        item.text = cJSON_IsString(latex) ? latex->valuestring : "";
        const cJSON* disp = cJSON_GetObjectItemCaseSensitive(comp, "display");
        item.math_display = cJSON_IsTrue(disp) ||
                            (cJSON_IsString(disp) && disp->valuestring &&
                             std::strcmp(disp->valuestring, "true") == 0);
        item.center = parent_center || ParseAlignCenter(comp);
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "Image") == 0) {
        item.kind = FlowKind::Image;
        const cJSON* url = cJSON_GetObjectItemCaseSensitive(comp, "url");
        const cJSON* w = cJSON_GetObjectItemCaseSensitive(comp, "width");
        const cJSON* h = cJSON_GetObjectItemCaseSensitive(comp, "height");
        const cJSON* border = cJSON_GetObjectItemCaseSensitive(comp, "border");
        item.image_url = cJSON_IsString(url) ? url->valuestring : "";
        item.img_w = cJSON_IsNumber(w) ? w->valueint : 440;
        item.img_h = cJSON_IsNumber(h) ? h->valueint : 280;
        item.img_border = !cJSON_IsFalse(border);
        item.center = parent_center || ParseAlignCenter(comp);
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "Divider") == 0) {
        item.kind = FlowKind::Divider;
        out.push_back(std::move(item));
    } else if (std::strcmp(type, "Spacer") == 0) {
        item.kind = FlowKind::Spacer;
        const cJSON* h = cJSON_GetObjectItemCaseSensitive(comp, "height");
        item.spacer_h = cJSON_IsNumber(h) ? h->valueint : 16;
        out.push_back(std::move(item));
    } else {
        ESP_LOGW(TAG, "skip unknown component: %s", type);
    }
}

enum class IngestResult { kNone, kAppended, kClear };

IngestResult IngestA2uiMessage(cJSON* msg, FlowList& out) {
    if (!cJSON_IsObject(msg)) {
        return IngestResult::kNone;
    }
    if (cJSON_GetObjectItemCaseSensitive(msg, "deleteSurface")) {
        return IngestResult::kClear;
    }
    const cJSON* body = cJSON_GetObjectItemCaseSensitive(msg, "updateComponents");
    if (!cJSON_IsObject(body)) {
        return IngestResult::kNone;
    }
    const cJSON* comps = cJSON_GetObjectItemCaseSensitive(body, "components");
    if (!cJSON_IsArray(comps)) {
        return IngestResult::kNone;
    }
    const size_t before = out.size();
    const cJSON* root = cJSON_GetObjectItemCaseSensitive(body, "root");
    if (cJSON_IsString(root) && root->valuestring) {
        WalkToFlow(const_cast<cJSON*>(comps),
                   FindComponentById(const_cast<cJSON*>(comps), root->valuestring), out);
    } else if (cJSON_GetArraySize(comps) > 0) {
        WalkToFlow(const_cast<cJSON*>(comps), cJSON_GetArrayItem(comps, 0), out);
    }
    return out.size() > before ? IngestResult::kAppended : IngestResult::kNone;
}

void PersistTurnJson(cJSON* msg)
{
    if (s_replaying_history || msg == nullptr) {
        return;
    }
    if (!assistant_chat_store_is_ready()) {
        return;
    }
    char* raw = cJSON_PrintUnformatted(msg);
    if (raw == nullptr) {
        ESP_LOGW(TAG, "persist: PrintUnformatted failed");
        return;
    }
    const esp_err_t err = assistant_chat_store_append(raw, std::strlen(raw));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "persist failed: %s", esp_err_to_name(err));
    }
    cJSON_free(raw);
}

void PresentAppendUi(bool jump_to_new, int reflow_from_page, bool show_content) {
    bool want_paint = false;
    {
        std::lock_guard<std::mutex> g(s_session_mu);
        want_paint = jump_to_new || s_page_index >= reflow_from_page;
        if (want_paint && !jump_to_new) {
            const int64_t now = esp_timer_get_time();
            if (now - s_last_stream_paint_us < kStreamPaintMinUs) {
                want_paint = false;
            } else {
                s_last_stream_paint_us = now;
            }
        } else if (want_paint) {
            s_last_stream_paint_us = esp_timer_get_time();
        }
    }

    Display* display = Board::GetInstance().GetDisplay();
    if (display == nullptr) {
        return;
    }
    DisplayLockGuard lock(display);
    if (!AssistantScreen::IsActive() || s_content == nullptr || !lv_obj_is_valid(s_content)) {
        return;
    }
    if (!s_a2ui_ready) {
        EnsureA2ui(s_scr);
        if (!s_a2ui_ready) {
            return;
        }
    }
    a2ui_set_host(s_content);
    if (show_content) {
        HideIdleHint();
    }
    if (want_paint) {
        RequestRenderCurrentPage();
    } else {
        UpdatePageIndicator();
    }
}

void AppendFlowAndShow(FlowList&& chunk, bool jump_to_new) {
    if (chunk.empty()) {
        return;
    }
    int reflow_from_page = 0;
    {
        std::lock_guard<std::mutex> g(s_session_mu);
        size_t chunk_at = s_flow.size();
        if (!s_flow.empty()) {
            FlowItem gap;
            gap.kind = FlowKind::MsgGap;
            s_flow.push_back(std::move(gap));
            chunk_at = s_flow.size();
        }
        for (auto& it : chunk) {
            s_flow.push_back(std::move(it));
        }
        if (jump_to_new) {
            if (s_replaying_history) {
                s_pin_flow_idx = static_cast<int>(s_flow.size()) - 1;
                while (s_pin_flow_idx >= 0 &&
                       s_flow[static_cast<size_t>(s_pin_flow_idx)].kind == FlowKind::MsgGap) {
                    --s_pin_flow_idx;
                }
            } else {
                s_pin_flow_idx = static_cast<int>(chunk_at);
            }
        }
        s_has_a2ui_content = true;
        if (!s_pages.empty() && s_page_flow_begin.size() == s_pages.size()) {
            while (s_flow_first_page.size() < s_flow.size()) {
                s_flow_first_page.push_back(-1);
            }
            reflow_from_page = RebuildPagesTail();
        } else {
            RebuildPages();
        }
        if (TrimFlowToMaxPages()) {
            reflow_from_page = 0;
        }
        if (jump_to_new && s_pin_flow_idx >= 0 &&
            static_cast<size_t>(s_pin_flow_idx) < s_flow_first_page.size() &&
            s_flow_first_page[static_cast<size_t>(s_pin_flow_idx)] >= 0) {
            s_page_index = s_flow_first_page[static_cast<size_t>(s_pin_flow_idx)];
        } else if (jump_to_new) {
            const int last_page = static_cast<int>(s_pages.size()) - 1;
            if (last_page >= 0) {
                s_page_index = last_page;
            }
        }
        ClampPageIndex();
    }
    PresentAppendUi(jump_to_new, reflow_from_page, true);
}

void CoalesceTimerCb(void* /*arg*/) {
    Application::GetInstance().Schedule([]() {
        FlowList chunk;
        bool jump = false;
        {
            if (!s_coalesce_pending) {
                return;
            }
            chunk = std::move(s_coalesce_buf);
            jump = s_coalesce_jump;
            s_coalesce_jump = false;
            s_coalesce_pending = false;
            s_coalesce_buf.clear();
        }
        if (chunk.empty() || !AssistantScreen::IsActive()) {
            return;
        }
        AppendFlowAndShow(std::move(chunk), jump);
    });
}

bool EnsureCoalesceTimer() {
    if (s_coalesce_timer != nullptr) {
        return true;
    }
    const esp_timer_create_args_t args = {
        .callback = &CoalesceTimerCb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "as_coalesce",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&args, &s_coalesce_timer) != ESP_OK) {
        s_coalesce_timer = nullptr;
        return false;
    }
    return true;
}

void QueueOrShowChunk(FlowList&& chunk, bool jump_to_new) {
    if (chunk.empty()) {
        return;
    }
    // 用户开口 / 回放：立刻排版；assistant 流式合并，避免主循环被每包堵住
    if (jump_to_new || s_replaying_history) {
        if (s_coalesce_pending) {
            FlowList merged = std::move(s_coalesce_buf);
            const bool pending_jump = s_coalesce_jump;
            s_coalesce_buf.clear();
            s_coalesce_jump = false;
            s_coalesce_pending = false;
            if (s_coalesce_timer != nullptr) {
                esp_timer_stop(s_coalesce_timer);
            }
            if (!merged.empty()) {
                AppendFlowAndShow(std::move(merged), pending_jump);
            }
        }
        AppendFlowAndShow(std::move(chunk), jump_to_new);
        return;
    }
    if (!s_coalesce_buf.empty()) {
        FlowItem gap;
        gap.kind = FlowKind::MsgGap;
        s_coalesce_buf.push_back(std::move(gap));
    }
    for (auto& it : chunk) {
        s_coalesce_buf.push_back(std::move(it));
    }
    s_coalesce_pending = true;
    if (!EnsureCoalesceTimer()) {
        AppendFlowAndShow(std::move(s_coalesce_buf), false);
        s_coalesce_pending = false;
        return;
    }
    esp_timer_stop(s_coalesce_timer);
    if (esp_timer_start_once(s_coalesce_timer, kStreamCoalesceUs) != ESP_OK) {
        AppendFlowAndShow(std::move(s_coalesce_buf), false);
        s_coalesce_pending = false;
    }
}

void ReplayHistoryFromStore()
{
    if (!assistant_chat_store_is_ready()) {
        ESP_LOGI(TAG, "history: no store (no SD or init skipped)");
        return;
    }

    /* Stream: store frees each file buf after the visitor returns. Parse here so
     * we never hold 40 JSON blobs (~192KB) plus a cJSON tree at once. Extra RAM
     * is one file (≤24KB) + one parse tree, then only FlowItem text (UI state). */
    struct Acc {
        FlowList chunk;
        unsigned ok = 0;
        unsigned skip = 0;
    } acc;

    s_replaying_history = true;
    const esp_err_t err = assistant_chat_store_for_each(
        [](const char* json, size_t len, void* ctx) -> bool {
            auto* a = static_cast<Acc*>(ctx);
            cJSON* root = cJSON_Parse(json);
            if (root == nullptr) {
                ESP_LOGW(TAG, "history skip: parse fail len=%u", static_cast<unsigned>(len));
                a->skip++;
                return true;
            }
            FlowList part;
            const IngestResult r = IngestA2uiMessage(root, part);
            cJSON_Delete(root);
            if (r != IngestResult::kAppended || part.empty()) {
                a->skip++;
                return true;
            }
            if (!a->chunk.empty()) {
                FlowItem gap;
                gap.kind = FlowKind::MsgGap;
                a->chunk.push_back(std::move(gap));
            }
            for (auto& it : part) {
                a->chunk.push_back(std::move(it));
            }
            a->ok++;
            return true;
        },
        &acc);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "history load: %s", esp_err_to_name(err));
        s_replaying_history = false;
        return;
    }
    if (acc.ok == 0) {
        ESP_LOGI(TAG, "history: empty (skip=%u)", acc.skip);
        s_replaying_history = false;
        return;
    }

    ESP_LOGI(TAG, "history replay %u record(s) skip=%u", acc.ok, acc.skip);
    if (!acc.chunk.empty()) {
        // 进页回放落在末条所在页（最近一轮）
        AppendFlowAndShow(std::move(acc.chunk), true);
    }
    s_replaying_history = false;
}

bool OnVkKey(const char* key) {
    if (key == nullptr) {
        return false;
    }

    // vk_home：直接出栈（阅读进百问回阅读）；不翻页、不清栈回首页
    if (std::strcmp(key, "vk_home") == 0) {
        ESP_LOGI(TAG, "vk_home -> navigate back");
        // 先停收消息/合并绘制/连翻，避免卸页过程中再 Render 踩已毁控件
        s_active.store(false, std::memory_order_release);
        if (s_coalesce_timer != nullptr) {
            esp_timer_stop(s_coalesce_timer);
        }
        s_coalesce_pending = false;
        s_coalesce_buf.clear();
        VkPageRepeatStop();
        ScreenPaintCoalesceReset(&s_page_paint);
        ScreenNavigateBack();
        return true;
    }

    if (std::strcmp(key, "vk_prev") == 0) {
        if (AssistantTurnPage(-1)) {
            return true;
        }
        ESP_LOGI(TAG, "vk_prev on first page -> navigate back");
        return false; // 默认策略 ScreenNavigateBack
    }

    if (!s_has_a2ui_content || s_pages.empty()) {
        return false;
    }
    if (std::strcmp(key, "vk_next") == 0) {
        AssistantTurnPage(1);
        return true;
    }
    return false;
}

void OnAssistantLifecycle(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    auto& app = Application::GetInstance();
    if (code == LV_EVENT_SCREEN_LOADED) {
        s_scr = static_cast<lv_obj_t*>(lv_event_get_target(e));
        s_active.store(true, std::memory_order_release);
        const bool hold_through = BootKey_IsHeld();
        if (auto* disp = LVAdapterDisplay::Instance()) {
            disp->SetStatusTitlePrefix(nullptr);
            // 顶栏：联网提示走 SetStatus；聆听/说话文案过滤；Idle 后刷时钟
            disp->SetIdleStatusMode(IdleStatusMode::kClock);
            disp->UpdateStatusBar(true);
        }
        if (s_content != nullptr && lv_obj_is_valid(s_content)) {
            a2ui_set_host(s_content);
        }
        if (hold_through) {
            ESP_LOGI(TAG, "enter hold-through -> schedule voice + listen (skip standby)");
            app.Schedule([]() {
                TaskScreen::RequestCacheRefresh();
                Application::GetInstance().StartXiaozhiVoice(true);
            });
        } else {
            ESP_LOGI(TAG, "enter -> schedule voice session prepare (PTT, no auto listen)");
            app.Schedule([]() {
                TaskScreen::RequestCacheRefresh();
                Application::GetInstance().StartXiaozhiVoice(false);
            });
        }
        ApplyPttWave(IsPttWaveWanted());
    } else if (code == LV_EVENT_SCREEN_UNLOADED) {
        ESP_LOGI(TAG, "leave -> schedule voice session stop (keep session RAM)");
        s_active.store(false, std::memory_order_release);
        if (s_coalesce_timer != nullptr) {
            esp_timer_stop(s_coalesce_timer);
        }
        s_coalesce_pending = false;
        s_coalesce_buf.clear();
        VkPageRepeatStop();
        StopPttWaveAnim();
        ScreenPaintCoalesceReset(&s_page_paint);
        a2ui_set_host(nullptr);
        // 保留 s_flow/s_pages；须先毁引用像素的控件，再 a2ui_image_clear（与 ClearSession 同序）
        a2ui_image_abort_loads();
        if (s_content != nullptr && lv_obj_is_valid(s_content)) {
            lv_obj_clean(s_content);
        }
        a2ui_image_clear();
        ResetTouchPttState();
        s_scr = nullptr;
        s_content = nullptr;
        s_page_label = nullptr;
        s_hint_img = nullptr;
        s_hint_raster = nullptr;  // 像素由 hint_img DELETE 回调释放
        s_status_label = nullptr;
        s_notification_label = nullptr;
        s_ptt_wave = nullptr;
        s_touch_ptt_hit = nullptr;
        s_ptt_wave_visible = false;
        for (int i = 0; i < kPttWaveBars; ++i) {
            s_ptt_wave_bars[i] = nullptr;
        }
        s_ui_font = nullptr;
        s_ui_font_bold = nullptr;
        s_viewport_w = 0;
        s_viewport_h = 0;
        if (auto* disp = LVAdapterDisplay::Instance()) {
            disp->SetStatusTitlePrefix(nullptr);
            disp->SetIdleStatusMode(IdleStatusMode::kClock);
        }
        app.Schedule([]() {
            Application::GetInstance().StopXiaozhiVoice();
        });
    }
}

void OpenAssistantAsync(void* /*arg*/) {
    if (AssistantScreen::IsActive()) {
        ESP_LOGI(TAG, "RequestOpen no-op: already active");
        return;
    }
    HapticPulseIfEnabled();
    ESP_LOGI(TAG, "RequestOpen -> ScreenNavigateTo");
    ScreenNavigateTo(AssistantScreen::Create);
}

bool OnBootPressDown() {
    // 开听只走 LongPress / hold-through；按下不听，避免短按先闪「聆听中」
    ESP_LOGI(TAG, "boot press-down ignored for listen (click/long own the semantics)");
    return true;
}

bool OnBootPressUp() {
    // BootKey 已清 IsHeld；屏触仍按住时不能停听，否则多源 PTT 会互相打断
    StopListeningIfNoPttHeld("boot press-up");
    return true;
}

bool OnBootLongPress() {
    // 仅开听；打断说话由 StartListening(Speaking) 内部处理，不走 AbortSpeakingToIdle
    ESP_LOGI(TAG, "boot long-press (500ms) -> StartListening (PTT arm, not click-abort)");
    RequestSyncPttOverlay();
    Application::GetInstance().StartListening();
    return true;
}

bool OnBootClick() {
    // 仅短按打断 → 待命；进待机改走电源键。长按回合已在 BootKey 层屏蔽。
    if (BootKey_DidLongPress()) {
        ESP_LOGI(TAG, "boot short ignored: long-press already armed listen");
        return true;
    }
    ESP_LOGI(TAG, "boot short -> AbortSpeakingToIdle (no listen)");
    Application::GetInstance().AbortSpeakingToIdle();
    return true;
}

bool OnBootDoubleClick() {
    return AssistantBootPhoto_OnDoubleClick();
}

bool OnVkKeyLongPress(const char* key) {
    if (key == nullptr) {
        return false;
    }
    // vk_home 长按不消费 → VkKey 默认一键回系统首页（不再作 PTT）
    if (!AssistantScreen::IsActive()) {
        return false;
    }
    if (VkPageRepeatTryStart(key, AssistantPageRepeatStep)) {
        return true;
    }
    return false;
}

bool OnVkKeyPressUp(const char* key) {
    if (key == nullptr) {
        return false;
    }
    return VkPageRepeatOnPressUp(key);
}

}  // namespace

bool AssistantScreen::IsActive() {
    // 主事件循环 / SetStatus 会跨线程调用；勿 lv_obj_is_valid / lv_screen_active
    return s_active.load(std::memory_order_acquire);
}

bool AssistantScreen::IsPttHeld() {
    return IsAnyPttHeld();
}

bool AssistantScreen::IsPttWaveVisible() {
    return s_ptt_wave_visible;
}

void AssistantScreen::SyncPttOverlay() {
    RequestSyncPttOverlay();
}

void AssistantScreen::RequestOpen() {
    if (!esp_lv_adapter_is_initialized()) {
        return;
    }
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGW(TAG, "RequestOpen: adapter lock failed");
        return;
    }
    if (lv_async_call(OpenAssistantAsync, nullptr) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "RequestOpen: lv_async_call failed");
    }
    esp_lv_adapter_unlock();
}

void AssistantScreen::SetEmotion(const char* emotion) {
    if (s_hint_img == nullptr || !lv_obj_is_valid(s_hint_img)) {
        return;
    }
    if (s_has_a2ui_content || !s_flow.empty()) {
        HideIdleHint();
        return;
    }
    (void)emotion;
    ShowIdleHint();
}

void AssistantScreen::AddMessage(const char* role, const char* content) {
    ESP_LOGI(TAG, "AddMessage role=%s content=%s",
             role != nullptr ? role : "(null)",
             content != nullptr ? content : "(null)");
    if (!IsActive()) {
        return;
    }
    if (content == nullptr || content[0] == '\0') {
        return;
    }

    const char* p = content;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        ++p;
    }
    if (*p != '{' && *p != '[') {
        ESP_LOGW(TAG, "AddMessage: content is not A2UI JSON, skip (role=%s len=%u)",
                 role != nullptr ? role : "?", static_cast<unsigned>(std::strlen(content)));
        return;
    }

    cJSON* root = cJSON_Parse(content);
    if (root == nullptr) {
        ESP_LOGW(TAG, "AddMessage: cJSON_Parse failed");
        return;
    }

    FlowList chunk;
    bool cleared = false;
    auto feed_one = [&](cJSON* msg) {
        FlowList part;
        const IngestResult r = IngestA2uiMessage(msg, part);
        if (r == IngestResult::kClear) {
            assistant_chat_store_wipe();
            cleared = true;
            chunk.clear();
            return;
        }
        if (r == IngestResult::kAppended) {
            PersistTurnJson(msg);
            for (auto& it : part) {
                chunk.push_back(std::move(it));
            }
        }
    };

    if (cJSON_IsArray(root)) {
        const cJSON* item = nullptr;
        cJSON_ArrayForEach(item, root) {
            feed_one(const_cast<cJSON*>(item));
        }
    } else if (cJSON_IsObject(root)) {
        feed_one(root);
    } else {
        ESP_LOGW(TAG, "AddMessage: unexpected JSON type");
    }
    cJSON_Delete(root);

    if (cleared) {
        Display* display = Board::GetInstance().GetDisplay();
        if (display == nullptr) {
            return;
        }
        DisplayLockGuard lock(display);
        if (!IsActive()) {
            return;
        }
        ClearSession();
        ShowIdleHint();
        return;
    }
    if (chunk.empty()) {
        return;
    }
    const bool jump_user = role != nullptr && std::strcmp(role, "user") == 0;
    QueueOrShowChunk(std::move(chunk), jump_user);
}

lv_obj_t* AssistantScreen::Create() {
    const lv_font_t* ui_font = UiFont();
    (void)UiFontBold();

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, ui_font, 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    DisableScroll(scr);

    // 顶栏与其它 App 共用 ScreenCreateStatusBar（25@2），勿用会话 30px 字
    EpdStatusBar status = ScreenCreateStatusBar(scr);
    const lv_coord_t header_h = status.height;
    const lv_coord_t body_h = LV_VER_RES - header_h - kFooterH;
    s_viewport_w = LV_HOR_RES - kContentPad * 2;
    s_viewport_h = body_h - kContentPad * 2;
    if (s_viewport_h < FontLineHeight(ui_font)) {
        s_viewport_h = FontLineHeight(ui_font);
    }

    lv_obj_t* container = lv_obj_create(scr);
    lv_obj_set_size(container, LV_HOR_RES, LV_VER_RES - header_h);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, header_h);
    lv_obj_set_style_radius(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_bg_color(container, lv_color_white(), 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(container);

    lv_obj_t* content = lv_obj_create(container);
    lv_obj_set_width(content, LV_HOR_RES);
    lv_obj_set_height(content, body_h);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_set_style_pad_hor(content, kContentPad, 0);
    lv_obj_set_style_pad_ver(content, kContentPad, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(content);

    lv_obj_t* footer = lv_label_create(container);
    lv_obj_set_size(footer, LV_HOR_RES, kFooterH);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer, ui_font, 0);
    lv_obj_set_style_text_color(footer, lv_color_black(), 0);
    lv_label_set_text(footer, "");
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_CLICKABLE);
    DisableScroll(footer);
    lv_obj_add_flag(footer, LV_OBJ_FLAG_HIDDEN);

    // 无会话全屏提示图：盖住内容区；顶栏透明，需再抬到图之上
    lv_obj_t* hint_img = lv_image_create(scr);
    lv_obj_add_flag(hint_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(hint_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        hint_img,
        [](lv_event_t* ev) {
            auto* p = static_cast<reader::RasterImage*>(
                lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(ev))));
            delete p;
            if (s_hint_raster == p) {
                s_hint_raster = nullptr;
            }
        },
        LV_EVENT_DELETE, nullptr);
    LoadIdleHintImage(hint_img);
    lv_obj_move_foreground(status.bar);
    lv_obj_move_foreground(status.overlay);

    // 录音波形叠在状态栏条带居中：高度约半栏偏矮、竖条更密；不改状态栏尺寸；按住说话时盖住时钟
    s_ptt_wave_max_h = header_h / 2;
    if (s_ptt_wave_max_h < 12) {
        s_ptt_wave_max_h = 12;
    }
    lv_obj_t* ptt_wave = CreatePttWaveHost(scr, s_ptt_wave_max_h);
    lv_obj_align(ptt_wave, LV_ALIGN_TOP_MID, 0, (header_h - s_ptt_wave_max_h) / 2);
    lv_obj_add_flag(ptt_wave, LV_OBJ_FLAG_HIDDEN);

    // 最后创建：保证 hit 在内容之上吃屏触；非 clickable 子对象不抢命中
    lv_obj_t* touch_ptt_hit = CreateTouchPttHitLayer(scr);

    s_scr = scr;
    s_content = content;
    s_page_label = footer;
    s_hint_img = hint_img;
    s_status_label = status.status_label;
    s_notification_label = status.notification_label;
    s_ptt_wave = ptt_wave;
    s_touch_ptt_hit = touch_ptt_hit;
    s_ptt_wave_visible = false;
    ResetTouchPttState();
    ScreenPaintCoalesceReset(&s_page_paint);
    const bool restore_session = !s_flow.empty();
    if (!restore_session) {
        s_pages.clear();
        s_flow_first_page.clear();
        s_page_flow_begin.clear();
        s_open_page_flow_begin = static_cast<size_t>(-1);
        s_page_index = 0;
        s_pin_flow_idx = -1;
        s_has_a2ui_content = false;
    }

    EnsureA2ui(scr);
    a2ui_set_host(content);
    if (restore_session) {
        ESP_LOGI(TAG, "restore session flow=%u pages=%u idx=%d",
                 static_cast<unsigned>(s_flow.size()), static_cast<unsigned>(s_pages.size()),
                 s_page_index);
        RebuildPages();
        TrimFlowToMaxPages();
        if (s_page_index >= static_cast<int>(s_pages.size())) {
            s_page_index = static_cast<int>(s_pages.size()) - 1;
        }
        if (s_page_index < 0) {
            s_page_index = 0;
        }
        if (!s_pages.empty()) {
            s_has_a2ui_content = true;
            HideIdleHint();
            RenderCurrentPage();
        } else {
            ShowIdleHint();
        }
    } else {
        ReplayHistoryFromStore();
        if (s_has_a2ui_content || !s_flow.empty()) {
            HideIdleHint();
        } else {
            ShowIdleHint();
        }
    }
    if (auto* disp = LVAdapterDisplay::Instance()) {
        disp->SetStatusTitlePrefix(nullptr);
        disp->SetIdleStatusMode(IdleStatusMode::kClock);
        disp->UpdateStatusBar(true);
    }

    ScreenSetIsHome(false);
    {
        VkKeyScreenDesc desc{};
        desc.factory = AssistantScreen::Create;
        desc.on_key = OnVkKey;
        desc.on_boot_click = OnBootClick;
        desc.on_boot_long_press = OnBootLongPress;
        desc.on_boot_press_down = OnBootPressDown;
        desc.on_boot_press_up = OnBootPressUp;
        desc.on_key_long_press = OnVkKeyLongPress;
        desc.on_key_press_up = OnVkKeyPressUp;
        desc.on_boot_double_click = OnBootDoubleClick;
        VkKey_AttachScreen(scr, "assistant", desc);
    }
    lv_obj_add_event_cb(scr, OnAssistantLifecycle, LV_EVENT_SCREEN_LOADED, nullptr);
    lv_obj_add_event_cb(scr, OnAssistantLifecycle, LV_EVENT_SCREEN_UNLOADED, nullptr);
    return scr;
}
