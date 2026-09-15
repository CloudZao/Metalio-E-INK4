#include "screen_common.h"

#include "application.h"
#include "assets/lang_config.h"
#include "haptic_feedback.h"
#include "home_screen/home_screen.h"
#include "lv_adapter_display.h"
#include "network_screen/network_screen.h"
#include "settings_network_tab.h"
#include "vk_key_handler.h"

#include <cstring>

#include <esp_log.h>
#include <esp_lv_adapter.h>
#include <esp_timer.h>
#include <font_awesome.h>

#include "board.h"
#include "fontpack_lvgl.h"

LV_FONT_DECLARE(font_awesome_30_1);

namespace {

constexpr const char* TAG = "ScreenCommon";
constexpr const char* kHomeScreen = "home";
constexpr int kMaxBack = 8;
// 数字时分在 CJK 行盒里偏上，下移后与两侧图标视觉居中。
constexpr lv_coord_t kStatusTextNudgeY = 4;
/** 屏内边沿未交完时的兜底上屏（防极端卡住） */
constexpr uint64_t kPaintCoalescePendingFallbackUs = 120000;

bool s_on_home = true;
ScreenFactory s_back_stack[kMaxBack] = {};
int s_back_depth = 0;

LVAdapterDisplay* GetAdapterDisplay() {
    if (auto* self = LVAdapterDisplay::Instance()) {
        return self;
    }
    return static_cast<LVAdapterDisplay*>(Board::GetInstance().GetDisplay());
}

/** 首页左上角网络图标：WiFi 模式且未联网时可进扫网页；已连接则忽略。 */
void OnStatusNetworkIconClicked(lv_event_t* /*e*/) {
    if (!ScreenIsHome()) {
        return;
    }
    if (!SettingsNetworkTab_IsWifi()) {
        return;
    }
    if (Board::GetInstance().IsNetworkReady()) {
        return;
    }
    ESP_LOGI(TAG, "home wifi icon -> NetworkScreen (wifi mode, not connected)");
    ScreenNavigateTo(NetworkScreen::Create);
}

void ClearBackStack() {
    s_back_depth = 0;
    for (int i = 0; i < kMaxBack; ++i) {
        s_back_stack[i] = nullptr;
    }
}

void PushBackFactory(ScreenFactory factory) {
    if (factory == nullptr) {
        return;
    }
    if (s_back_depth < kMaxBack) {
        s_back_stack[s_back_depth++] = factory;
    } else {
        for (int i = 1; i < kMaxBack; ++i) {
            s_back_stack[i - 1] = s_back_stack[i];
        }
        s_back_stack[kMaxBack - 1] = factory;
        ESP_LOGW(TAG, "back stack full, drop oldest");
    }
}

ScreenFactory PopBackFactory() {
    if (s_back_depth <= 0) {
        return nullptr;
    }
    --s_back_depth;
    ScreenFactory f = s_back_stack[s_back_depth];
    s_back_stack[s_back_depth] = nullptr;
    return f;
}

void GoHomeAsync(void* /*user_data*/) {
    if (s_on_home) {
        ESP_LOGI(TAG, "home ignored (already home)");
        return;
    }
    ESP_LOGI(TAG, "-> home (clear back stack depth was %d)", s_back_depth);
    ScreenGoHome();
}

void NavigateBackAsync(void* /*user_data*/) {
    ScreenFactory prev = PopBackFactory();
    if (prev != nullptr) {
        ESP_LOGI(TAG, "navigate back -> recreate (stack left=%d)", s_back_depth);
        s_on_home = false;
        ScreenLoadReplace(prev());
        return;
    }
    ESP_LOGI(TAG, "navigate back -> home (empty stack)");
    ScreenGoHome();
}

}  // namespace

EpdStatusBar ScreenCreateStatusBar(lv_obj_t* scr) {
    EpdStatusBar out;

    const lv_font_t* ui_font = fontpack_lv_font_ui();
    const lv_coord_t status_line_h = ui_font != nullptr ? ui_font->line_height : 30;
    const lv_coord_t header_pad_v = 8;
    out.height = status_line_h + header_pad_v * 2;

    out.bar = lv_obj_create(scr);
    lv_obj_set_size(out.bar, LV_HOR_RES, out.height);
    lv_obj_set_style_radius(out.bar, 0, 0);
    lv_obj_set_style_bg_opa(out.bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(out.bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(out.bar, 1, 0);
    lv_obj_set_style_border_color(out.bar, lv_color_black(), 0);
    lv_obj_set_style_pad_all(out.bar, 0, 0);
    lv_obj_set_style_pad_top(out.bar, header_pad_v, 0);
    lv_obj_set_style_pad_bottom(out.bar, header_pad_v, 0);
    lv_obj_set_style_pad_left(out.bar, 8, 0);
    lv_obj_set_style_pad_right(out.bar, 8, 0);
    lv_obj_set_flex_flow(out.bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(out.bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(out.bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(out.bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(out.bar, LV_OBJ_FLAG_CLICKABLE);

    // 左上角网络图标：加大热区，便于点按；文案仍由 BindStatusWidgets 刷 network_label
    lv_obj_t* net_hit = lv_obj_create(out.bar);
    lv_obj_remove_style_all(net_hit);
    lv_obj_set_size(net_hit, 56, out.height - header_pad_v * 2);
    if (lv_obj_get_height(net_hit) < 44) {
        lv_obj_set_height(net_hit, 44);
    }
    lv_obj_set_style_bg_opa(net_hit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(net_hit, 0, 0);
    lv_obj_clear_flag(net_hit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(net_hit, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(net_hit);
    lv_obj_add_event_cb(net_hit, OnStatusNetworkIconClicked, LV_EVENT_CLICKED, nullptr);

    out.network_label = lv_label_create(net_hit);
    lv_label_set_text(out.network_label, "");
    lv_obj_set_style_text_font(out.network_label, &font_awesome_30_1, 0);
    lv_obj_set_style_text_color(out.network_label, lv_color_black(), 0);
    lv_obj_center(out.network_label);
    lv_obj_clear_flag(out.network_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* right_icons = lv_obj_create(out.bar);
    lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_icons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_icons, 0, 0);
    lv_obj_set_style_pad_all(right_icons, 0, 0);
    lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(right_icons, LV_OBJ_FLAG_CLICKABLE);

    out.mute_label = lv_label_create(right_icons);
    lv_label_set_text(out.mute_label, "");
    lv_obj_set_style_text_font(out.mute_label, &font_awesome_30_1, 0);
    lv_obj_set_style_text_color(out.mute_label, lv_color_black(), 0);
    lv_obj_clear_flag(out.mute_label, LV_OBJ_FLAG_CLICKABLE);

    out.battery_pct_label = lv_label_create(right_icons);
    lv_label_set_text(out.battery_pct_label, "");
    lv_obj_set_style_text_font(out.battery_pct_label, ui_font, 0);
    lv_obj_set_style_text_color(out.battery_pct_label, lv_color_black(), 0);
    lv_obj_set_style_margin_left(out.battery_pct_label, 6, 0);
    lv_obj_set_style_translate_y(out.battery_pct_label, 2, 0);
    lv_obj_clear_flag(out.battery_pct_label, LV_OBJ_FLAG_CLICKABLE);

    out.battery_label = lv_label_create(right_icons);
    lv_label_set_text(out.battery_label, "");
    lv_obj_set_style_text_font(out.battery_label, &font_awesome_30_1, 0);
    lv_obj_set_style_text_color(out.battery_label, lv_color_black(), 0);
    lv_obj_set_style_margin_left(out.battery_label, -2, 0);
    lv_obj_clear_flag(out.battery_label, LV_OBJ_FLAG_CLICKABLE);

    out.overlay = lv_obj_create(scr);
    lv_obj_set_size(out.overlay, LV_HOR_RES, out.height);
    lv_obj_set_style_radius(out.overlay, 0, 0);
    lv_obj_set_style_bg_opa(out.overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(out.overlay, 0, 0);
    lv_obj_set_style_pad_all(out.overlay, 0, 0);
    lv_obj_set_style_pad_top(out.overlay, header_pad_v, 0);
    lv_obj_set_style_pad_bottom(out.overlay, header_pad_v, 0);
    lv_obj_set_scrollbar_mode(out.overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_layout(out.overlay, LV_LAYOUT_NONE, 0);
    lv_obj_align(out.overlay, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(out.overlay, LV_OBJ_FLAG_CLICKABLE);

    // 居中状态/通知：两侧留给网络图标与电量，避免英文长文盖住电量
    const lv_coord_t status_text_w = LV_HOR_RES - 200;

    out.notification_label = lv_label_create(out.overlay);
    lv_obj_set_width(out.notification_label, status_text_w);
    lv_label_set_long_mode(out.notification_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(out.notification_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(out.notification_label, ui_font, 0);
    lv_obj_set_style_text_color(out.notification_label, lv_color_black(), 0);
    lv_label_set_text(out.notification_label, "");
    lv_obj_align(out.notification_label, LV_ALIGN_CENTER, 0, kStatusTextNudgeY);
    lv_obj_add_flag(out.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(out.notification_label, LV_OBJ_FLAG_CLICKABLE);

    out.status_label = lv_label_create(out.overlay);
    lv_obj_set_width(out.status_label, status_text_w);
    lv_label_set_long_mode(out.status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(out.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(out.status_label, ui_font, 0);
    lv_obj_set_style_text_color(out.status_label, lv_color_black(), 0);
    lv_label_set_text(out.status_label, "");
    lv_obj_align(out.status_label, LV_ALIGN_CENTER, 0, kStatusTextNudgeY);
    lv_obj_clear_flag(out.status_label, LV_OBJ_FLAG_CLICKABLE);

    out.low_battery_popup = lv_obj_create(scr);
    lv_obj_set_scrollbar_mode(out.low_battery_popup, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(out.low_battery_popup, LV_HOR_RES * 9 / 10, status_line_h * 2);
    lv_obj_align(out.low_battery_popup, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(out.low_battery_popup, lv_color_black(), 0);
    lv_obj_set_style_radius(out.low_battery_popup, 4, 0);
    lv_obj_t* low_battery_label = lv_label_create(out.low_battery_popup);
    lv_label_set_text(low_battery_label, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_font(low_battery_label, ui_font, 0);
    lv_obj_set_style_text_color(low_battery_label, lv_color_white(), 0);
    lv_obj_center(low_battery_label);
    lv_obj_add_flag(out.low_battery_popup, LV_OBJ_FLAG_HIDDEN);

    if (auto* disp = GetAdapterDisplay()) {
        disp->BindStatusWidgets(out.network_label, out.mute_label, out.battery_label, out.status_label,
                                out.notification_label, out.low_battery_popup, out.battery_pct_label);
    }
    return out;
}

void ScreenSetIsHome(bool is_home) {
    s_on_home = is_home;
}

bool ScreenIsHome() {
    return s_on_home;
}

void ScreenLoadReplace(lv_obj_t* new_scr) {
    lv_obj_t* old_scr = lv_screen_active();
    lv_screen_load(new_scr);
    if (old_scr != nullptr && old_scr != new_scr) {
        lv_obj_delete_async(old_scr);
    }
    // 清掉跨页残留的 press/gesture，避免新页首击被当成旧拖拽吞掉
    for (lv_indev_t* indev = lv_indev_get_next(nullptr); indev != nullptr;
         indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_reset(indev, nullptr);
        }
    }
}

void ScreenNavigateTo(lv_obj_t* (*create)()) {
    if (create == nullptr) {
        return;
    }
    const char* cur = VkKey_ActiveScreen();
    if (cur != nullptr && std::strcmp(cur, kHomeScreen) != 0 &&
        std::strcmp(cur, "none") != 0) {
        if (ScreenFactory factory = VkKey_GetScreenFactory(cur)) {
            PushBackFactory(factory);
            ESP_LOGI(TAG, "navigate to: push back factory for %s (depth=%d)", cur, s_back_depth);
        }
    }
    s_on_home = false;
    ScreenLoadReplace(create());
}

void ScreenNavigateBack() {
    if (!esp_lv_adapter_is_initialized()) {
        ESP_LOGW(TAG, "ScreenNavigateBack: adapter not ready");
        return;
    }
    const char* active = VkKey_ActiveScreen();
    if (active == nullptr || std::strcmp(active, kHomeScreen) == 0 ||
        std::strcmp(active, "none") == 0) {
        ESP_LOGI(TAG, "ScreenNavigateBack: skip stale back from screen=%s", active ? active : "null");
        return;
    }
    // 直接 lv_async_call，勿经主循环 Schedule；StartXiaozhiVoice 等可能长时间占住 MainEventLoop。
    if (lv_async_call(NavigateBackAsync, nullptr) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "ScreenNavigateBack: lv_async_call failed");
    }
}

void ScreenGoHome() {
    ClearBackStack();
    s_on_home = true;
    ScreenLoadReplace(HomeScreen::Create());
}

void ScreenRequestHome() {
    // 与 ScreenNavigateBack / 电源键同：勿经 MainEventLoop。
    // 长待机醒后 WiFi/蜂窝 EnsureNetworkReady、百问 StartXiaozhiVoice 等可占死主循环数秒～数十秒；
    // 若仍走 ScreenLvAsync，VkKey 有日志但 GoHomeAsync 迟迟不跑，表现为短按/长按都「没反应」。
    if (!ScreenLvAsyncUrgent(GoHomeAsync)) {
        ScreenLvAsync(GoHomeAsync);
    }
}

void ScreenRequestBack() {
    ScreenRequestHome();
}

bool ScreenLvAsync(void (*cb)(void*), void* user_data) {
    if (cb == nullptr) {
        return false;
    }
    if (!esp_lv_adapter_is_initialized()) {
        ESP_LOGW(TAG, "ScreenLvAsync: adapter not ready");
        return false;
    }
    // 页码等状态由调用方已改完；此处只排队。主循环再抢锁，不堵 touch_feed。
    Application::GetInstance().Schedule([cb, user_data]() {
        if (!esp_lv_adapter_is_initialized()) {
            return;
        }
        if (esp_lv_adapter_lock(-1) != ESP_OK) {
            ESP_LOGW(TAG, "ScreenLvAsync: adapter lock failed");
            return;
        }
        if (lv_async_call(cb, user_data) != LV_RESULT_OK) {
            ESP_LOGW(TAG, "ScreenLvAsync: lv_async_call failed");
        }
        esp_lv_adapter_unlock();
    });
    return true;
}

bool ScreenLvAsyncUrgent(void (*cb)(void*), void* user_data) {
    if (cb == nullptr) {
        return false;
    }
    if (!esp_lv_adapter_is_initialized()) {
        ESP_LOGW(TAG, "ScreenLvAsyncUrgent: adapter not ready");
        return false;
    }
    // 勿经 MainEventLoop：百问等网可占死主循环数十秒
    if (esp_lv_adapter_lock(50) != ESP_OK) {
        ESP_LOGW(TAG, "ScreenLvAsyncUrgent: adapter lock failed");
        return false;
    }
    const bool ok = lv_async_call(cb, user_data) == LV_RESULT_OK;
    if (!ok) {
        ESP_LOGW(TAG, "ScreenLvAsyncUrgent: lv_async_call failed");
    }
    esp_lv_adapter_unlock();
    return ok;
}

void ScreenPaintCoalesceEnqueue(ScreenPaintCoalesce* c);

void ScreenPaintCoalesceAsync(void* user_data) {
    auto* c = static_cast<ScreenPaintCoalesce*>(user_data);
    if (c == nullptr || c->paint == nullptr) {
        return;
    }
    c->queued.store(false, std::memory_order_release);
    const uint32_t want = c->seq.load(std::memory_order_acquire);
    if (want == c->done.load(std::memory_order_acquire)) {
        return;
    }
    // 每次只画一帧；落后则立刻再排队（不再防抖），让 LVGL/墨水有机会 flush
    c->paint();
    c->done.store(want, std::memory_order_release);
    if (c->seq.load(std::memory_order_acquire) != c->done.load(std::memory_order_acquire)) {
        ScreenPaintCoalesceEnqueue(c);
    }
}

void ScreenPaintCoalesceEnqueue(ScreenPaintCoalesce* c) {
    if (c == nullptr || c->paint == nullptr) {
        return;
    }
    bool expected = false;
    if (!c->queued.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }
    if (!ScreenLvAsync(ScreenPaintCoalesceAsync, c)) {
        c->queued.store(false, std::memory_order_release);
    }
}

void ScreenPaintCoalesceDeferCb(void* arg) {
    ScreenPaintCoalesceEnqueue(static_cast<ScreenPaintCoalesce*>(arg));
}

bool ScreenPaintCoalesceEnsureTimer(ScreenPaintCoalesce* c) {
    if (c->defer_timer != nullptr) {
        return true;
    }
    esp_timer_handle_t t = nullptr;
    const esp_timer_create_args_t args = {
        .callback = &ScreenPaintCoalesceDeferCb,
        .arg = c,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "paint_coal",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&args, &t) != ESP_OK || t == nullptr) {
        ESP_LOGW(TAG, "paint coalesce timer create failed");
        return false;
    }
    c->defer_timer = t;
    return true;
}

void ScreenPaintCoalesceArmDefer(ScreenPaintCoalesce* c, uint64_t delay_us) {
    if (!ScreenPaintCoalesceEnsureTimer(c)) {
        ScreenPaintCoalesceEnqueue(c);
        return;
    }
    auto* t = static_cast<esp_timer_handle_t>(c->defer_timer);
    esp_timer_stop(t);
    if (esp_timer_start_once(t, delay_us) != ESP_OK) {
        ScreenPaintCoalesceEnqueue(c);
    }
}

void ScreenPaintCoalesceRequest(ScreenPaintCoalesce* c) {
    if (c == nullptr || c->paint == nullptr) {
        return;
    }
    c->seq.fetch_add(1, std::memory_order_acq_rel);

    // 屏内连点：边沿还在队列 / 下一拍已按下 → 只记序号，让 LVGL 继续交 CLICKED
    // （固定短窗就开画会堵 indev，连点又变一页一刷）
    if (TouchUiHasPendingEdges() || TouchUiFingerIsDown()) {
        ScreenPaintCoalesceArmDefer(c, kPaintCoalescePendingFallbackUs);
        return;
    }

    // 触摸空闲（含刚交完最后一拍）：立刻排队画最终页；盖板键连点靠 seq 合并
    if (c->defer_timer != nullptr) {
        esp_timer_stop(static_cast<esp_timer_handle_t>(c->defer_timer));
    }
    ScreenPaintCoalesceEnqueue(c);
}

void ScreenPaintCoalesceRequestDebounced(ScreenPaintCoalesce* c, uint64_t delay_us) {
    if (c == nullptr || c->paint == nullptr) {
        return;
    }
    c->seq.fetch_add(1, std::memory_order_acq_rel);
    uint64_t wait = delay_us;
    if (wait == 0) {
        wait = kPaintCoalescePendingFallbackUs;
    }
    // 边沿未交完时至少撑过兜底窗，避免中途开画堵 indev
    if ((TouchUiHasPendingEdges() || TouchUiFingerIsDown()) &&
        wait < kPaintCoalescePendingFallbackUs) {
        wait = kPaintCoalescePendingFallbackUs;
    }
    ScreenPaintCoalesceArmDefer(c, wait);
}

void ScreenPaintCoalesceReset(ScreenPaintCoalesce* c) {
    if (c == nullptr) {
        return;
    }
    if (c->defer_timer != nullptr) {
        esp_timer_stop(static_cast<esp_timer_handle_t>(c->defer_timer));
    }
    c->queued.store(false, std::memory_order_release);
    const uint32_t seq = c->seq.load(std::memory_order_acquire);
    c->done.store(seq, std::memory_order_release);
}

lv_obj_t* ScreenCreatePlaceholder(const char* screen_id, const char* title) {
    s_on_home = false;

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);

    EpdStatusBar status = ScreenCreateStatusBar(scr);
    if (status.status_label != nullptr) {
        lv_label_set_text(status.status_label, "");
        lv_obj_add_flag(status.status_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (status.notification_label != nullptr) {
        lv_obj_add_flag(status.notification_label, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_HOR_RES, LV_VER_RES - status.height);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* label = lv_label_create(body);
    lv_label_set_text(label, title != nullptr ? title : "");
    lv_obj_set_style_text_font(label, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

    if (screen_id != nullptr) {
        VkKey_AttachScreen(scr, screen_id);
    }
    return scr;
}

namespace {

// I1 无真灰阶：弹窗外遮罩用大颗粒网点（16x16 平铺，3x3 黑块）。
constexpr int kDotBackdropW = 16;
constexpr int kDotBackdropH = 16;
constexpr int kDotSize = 3;
constexpr int kDotPitch = 8;
uint8_t s_dot_backdrop_l8[kDotBackdropW * kDotBackdropH];
lv_image_dsc_t s_dot_backdrop_img;
bool s_dot_backdrop_ready = false;

const lv_image_dsc_t* DotBackdropImg() {
    if (!s_dot_backdrop_ready) {
        for (int i = 0; i < kDotBackdropW * kDotBackdropH; ++i) {
            s_dot_backdrop_l8[i] = 0xFF;
        }
        for (int gy = 0; gy < kDotBackdropH; gy += kDotPitch) {
            const int x0 = ((gy / kDotPitch) & 1) ? (kDotPitch / 2) : 2;
            for (int gx = x0; gx < kDotBackdropW; gx += kDotPitch) {
                for (int dy = 0; dy < kDotSize; ++dy) {
                    for (int dx = 0; dx < kDotSize; ++dx) {
                        const int x = (gx + dx) % kDotBackdropW;
                        const int y = (gy + 2 + dy) % kDotBackdropH;
                        s_dot_backdrop_l8[y * kDotBackdropW + x] = 0x00;
                    }
                }
            }
        }
        s_dot_backdrop_img.header.magic = LV_IMAGE_HEADER_MAGIC;
        s_dot_backdrop_img.header.cf = LV_COLOR_FORMAT_L8;
        s_dot_backdrop_img.header.flags = 0;
        s_dot_backdrop_img.header.w = kDotBackdropW;
        s_dot_backdrop_img.header.h = kDotBackdropH;
        s_dot_backdrop_img.header.stride = kDotBackdropW;
        s_dot_backdrop_img.data_size = sizeof(s_dot_backdrop_l8);
        s_dot_backdrop_img.data = s_dot_backdrop_l8;
        s_dot_backdrop_ready = true;
    }
    return &s_dot_backdrop_img;
}

}  // namespace

void ScreenApplyDotBackdrop(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_image_src(obj, DotBackdropImg(), 0);
    lv_obj_set_style_bg_image_tiled(obj, true, 0);
}
