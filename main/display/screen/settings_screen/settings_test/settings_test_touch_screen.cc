#include "settings_test_touch_screen.h"

#include "assets/lang_config.h"
#include "haptic_feedback.h"
#include "lv_adapter_display.h"
#include "screen_common.h"
#include "vk_key_handler.h"

#include <cstdio>

#include <esp_log.h>
#include <esp_random.h>
#include "fontpack_lvgl.h"

namespace {

constexpr const char* TAG = "SettingsTestTouch";
constexpr const char* kScreenId = "settings_test_touch";

// 与 debug_tp TouchTestScreen 一致：准星几何 + 固定 10 点后进入随机
constexpr lv_coord_t kTargetHit = 64;    // 点击热区边长
constexpr lv_coord_t kRingSize = 48;     // 外环直径
constexpr lv_coord_t kCrossLen = 56;     // 十字臂长
constexpr lv_coord_t kCrossThick = 2;    // 十字线宽
constexpr lv_coord_t kCoreSize = 10;     // 中心实心点
constexpr lv_coord_t kCoreClear = 16;    // 中心留白
constexpr lv_coord_t kEdgeInset = 48;    // 左右/上边距
constexpr lv_coord_t kBottomInset = 96;  // 底排上移，序号不被挡住
constexpr lv_coord_t kCenterGap = 70;    // 中心两点左右间距的一半
constexpr int kFixedCount = 10;

lv_obj_t* s_scr = nullptr;
lv_obj_t* s_field = nullptr;
lv_obj_t* s_start_lbl = nullptr;
int s_remaining = 0;
bool s_random_mode = false;
bool s_testing = false;

void ClearStatusBindings() {
    if (auto* disp = LVAdapterDisplay::Instance()) {
        disp->BindStatusWidgets(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
}

lv_obj_t* MakeBar(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, w, h);
    lv_obj_set_style_bg_color(bar, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    return bar;
}

void SpawnTarget(lv_coord_t cx, lv_coord_t cy, int index);

void SpawnRandomTarget() {
    if (s_field == nullptr || !lv_obj_is_valid(s_field)) {
        return;
    }
    const lv_coord_t fw = lv_obj_get_width(s_field);
    const lv_coord_t fh = lv_obj_get_height(s_field);
    const lv_coord_t half = kTargetHit / 2;
    const lv_coord_t min_x = half;
    const lv_coord_t max_x = fw > half ? (fw - half) : half;
    const lv_coord_t min_y = half;
    const lv_coord_t max_y = fh > kBottomInset ? (fh - kBottomInset) : half;
    const lv_coord_t span_x = (max_x > min_x) ? (max_x - min_x) : 0;
    const lv_coord_t span_y = (max_y > min_y) ? (max_y - min_y) : 0;
    const lv_coord_t rx = min_x + static_cast<lv_coord_t>(esp_random() % (span_x + 1));
    const lv_coord_t ry = min_y + static_cast<lv_coord_t>(esp_random() % (span_y + 1));
    SpawnTarget(rx, ry, 0);
}

void OnTargetClicked(lv_event_t* e) {
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    while (target != nullptr && target != s_field && lv_obj_get_parent(target) != s_field) {
        target = lv_obj_get_parent(target);
    }
    if (target == nullptr || target == s_field) {
        return;
    }

    lv_obj_t* lbl = static_cast<lv_obj_t*>(lv_obj_get_user_data(target));
    if (lbl != nullptr && lv_obj_is_valid(lbl)) {
        lv_obj_delete(lbl);
    }
    lv_obj_delete(target);

    if (!s_random_mode) {
        if (s_remaining > 0) {
            --s_remaining;
        }
        ESP_LOGI(TAG, "fixed remaining=%d", s_remaining);
        if (s_remaining == 0) {
            s_random_mode = true;
            ESP_LOGI(TAG, "enter random mode");
            SpawnRandomTarget();
        }
        return;
    }

    SpawnRandomTarget();
}

void SpawnTarget(lv_coord_t cx, lv_coord_t cy, int index) {
    if (s_field == nullptr || !lv_obj_is_valid(s_field)) {
        return;
    }

    // 透明热区；内部画「外环 + 十字 + 中心点」准星（同 debug_tp）
    lv_obj_t* t = lv_obj_create(s_field);
    lv_obj_remove_style_all(t);
    lv_obj_set_size(t, kTargetHit, kTargetHit);
    lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(t, 0, 0);
    lv_obj_set_style_pad_all(t, 0, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(t, cx - kTargetHit / 2, cy - kTargetHit / 2);

    lv_obj_t* hbar = MakeBar(t, kCrossLen, kCrossThick);
    lv_obj_center(hbar);
    lv_obj_t* vbar = MakeBar(t, kCrossThick, kCrossLen);
    lv_obj_center(vbar);

    lv_obj_t* ring = lv_obj_create(t);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, kRingSize, kRingSize);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 3, 0);
    lv_obj_set_style_border_color(ring, lv_color_black(), 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
    lv_obj_center(ring);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* clear = lv_obj_create(t);
    lv_obj_remove_style_all(clear);
    lv_obj_set_size(clear, kCoreClear, kCoreClear);
    lv_obj_set_style_radius(clear, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(clear, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(clear, LV_OPA_COVER, 0);
    lv_obj_center(clear);
    lv_obj_clear_flag(clear, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(clear, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* core = lv_obj_create(t);
    lv_obj_remove_style_all(core);
    lv_obj_set_size(core, kCoreSize, kCoreSize);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(core, LV_OPA_COVER, 0);
    lv_obj_center(core);
    lv_obj_clear_flag(core, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(core, LV_OBJ_FLAG_SCROLLABLE);

    if (index > 0) {
        lv_obj_t* lbl = lv_label_create(s_field);
        char buf[4];
        std::snprintf(buf, sizeof(buf), "%d", index);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(lbl, cx - 6, cy + kRingSize / 2 + 2);
        lv_obj_set_user_data(t, lbl);
    } else {
        lv_obj_set_user_data(t, nullptr);
    }

    HapticAttachClick(t);
    lv_obj_add_event_cb(t, OnTargetClicked, LV_EVENT_CLICKED, nullptr);
}

void SpawnFixedSymmetric() {
    if (s_field == nullptr) {
        return;
    }
    const lv_coord_t w = LV_HOR_RES;
    const lv_coord_t h = LV_VER_RES;
    lv_obj_set_size(s_field, w, h);

    const lv_coord_t L = kEdgeInset;
    const lv_coord_t R = w - kEdgeInset;
    const lv_coord_t T = kEdgeInset;
    const lv_coord_t B = h - kBottomInset;
    const lv_coord_t cx = w / 2;
    const lv_coord_t cy = h / 2;

    const lv_coord_t pts[kFixedCount][2] = {
        {L, T},  {cx, T}, {R, T},
        {L, cy},          {R, cy},
        {L, B},  {cx, B}, {R, B},
        {cx - kCenterGap, cy},
        {cx + kCenterGap, cy},
    };

    for (int i = 0; i < kFixedCount; ++i) {
        SpawnTarget(pts[i][0], pts[i][1], i + 1);
    }
    s_remaining = kFixedCount;
    s_random_mode = false;
    ESP_LOGI(TAG, "fullscreen %dx%d fixed=%d", static_cast<int>(w), static_cast<int>(h), kFixedCount);
}

void StartTesting() {
    if (s_testing || s_scr == nullptr || !lv_obj_is_valid(s_scr)) {
        return;
    }
    s_testing = true;

    if (s_start_lbl != nullptr && lv_obj_is_valid(s_start_lbl)) {
        lv_obj_delete(s_start_lbl);
        s_start_lbl = nullptr;
    }
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_CLICKABLE);

    s_field = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_field);
    lv_obj_set_size(s_field, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(s_field, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_field, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_field, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_field, 0, 0);
    lv_obj_set_style_pad_all(s_field, 0, 0);
    lv_obj_clear_flag(s_field, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_field, LV_OBJ_FLAG_CLICKABLE);

    SpawnFixedSymmetric();
}

void OnStartClicked(lv_event_t* /*e*/) {
    if (s_testing) {
        return;
    }
    HapticPulseIfEnabled();
    StartTesting();
}

void OnDeleted(lv_event_t* e) {
    if (lv_event_get_target(e) != s_scr) {
        return;
    }
    s_scr = nullptr;
    s_field = nullptr;
    s_start_lbl = nullptr;
    s_remaining = 0;
    s_random_mode = false;
    s_testing = false;
}

}  // namespace

lv_obj_t* SettingsTestTouchScreen::Create() {
    ESP_LOGI(TAG, "create touch test screen");
    ScreenSetIsHome(false);
    ClearStatusBindings();

    s_testing = false;
    s_field = nullptr;
    s_remaining = 0;
    s_random_mode = false;

    lv_obj_t* scr = lv_obj_create(nullptr);
    s_scr = scr;
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);

    s_start_lbl = lv_label_create(scr);
    lv_label_set_text(s_start_lbl, Lang::Strings::SETTINGS_TEST_TOUCH_START);
    lv_obj_set_style_text_font(s_start_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_start_lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(s_start_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_start_lbl);
    lv_obj_clear_flag(s_start_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(scr, OnStartClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(scr, OnDeleted, LV_EVENT_DELETE, nullptr);
    VkKey_AttachScreen(scr, kScreenId, VkKeyScreenDesc{SettingsTestTouchScreen::Create});
    return scr;
}
