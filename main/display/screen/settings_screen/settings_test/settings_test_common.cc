#include "settings_test_common.h"

#include "board.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "settings_common.h"

#include <esp_log.h>
#include <esp_lv_adapter.h>

namespace {

constexpr const char* TAG = "SettingsTest";
SettingsTestUi s_ui;

}  // namespace

SettingsTestUi& SettingsTest_Ui() {
    return s_ui;
}

bool SettingsTest_IsLive() {
    return s_ui.alive && s_ui.active;
}

void SettingsTest_BeginShutdown() {
    s_ui.alive = false;
    s_ui.active = false;
}

bool SettingsTest_PostLvAsync(void (*cb)(void*), void* user_data) {
    if (cb == nullptr) {
        return false;
    }
    if (!esp_lv_adapter_is_initialized()) {
        ESP_LOGW(TAG, "PostLvAsync: adapter not ready");
        return false;
    }
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGW(TAG, "PostLvAsync: adapter lock failed");
        return false;
    }
    const bool ok = (lv_async_call(cb, user_data) == LV_RESULT_OK);
    esp_lv_adapter_unlock();
    if (!ok) {
        ESP_LOGW(TAG, "PostLvAsync: lv_async_call failed");
    }
    return ok;
}

void SettingsTest_SetRowStatus(SettingsTestRow& row, bool pass) {
    if (!SettingsTest_IsLive() || row.icon == nullptr) {
        return;
    }
    lv_image_set_src(row.icon, pass ? kSettingsTestIconPass : kSettingsTestIconFail);
    lv_obj_clear_flag(row.icon, LV_OBJ_FLAG_HIDDEN);
}

void SettingsTest_SetRowValue(SettingsTestRow& row, const char* text, bool error) {
    if (!SettingsTest_IsLive() || row.value == nullptr) {
        return;
    }
    lv_label_set_text(row.value, text != nullptr ? text : "");
    lv_obj_set_style_text_color(row.value, lv_color_black(), 0);
    (void)error;
}

SettingsTestRow SettingsTest_CreateRow(lv_obj_t* parent, const char* title,
                                       lv_event_cb_t row_click_cb) {
    SettingsTestRow out;

    lv_obj_t* row = lv_obj_create(parent);
    out.row = row;
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, kSettingsTestRowH);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_black(), 0);
    lv_obj_set_style_pad_hor(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    if (row_click_cb != nullptr) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(row);
        lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, nullptr);
    } else {
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    }

    out.icon = lv_image_create(row);
    lv_obj_set_size(out.icon, kSettingsTestIconSz, kSettingsTestIconSz);
    lv_obj_clear_flag(out.icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(out.icon, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title_lbl = lv_label_create(row);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_width(title_lbl, kSettingsTestTitleW);
    lv_obj_set_height(title_lbl, lv_font_get_line_height(fontpack_lv_font_ui()));
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(title_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(title_lbl, LV_OBJ_FLAG_CLICKABLE);

    out.value = lv_label_create(row);
    lv_label_set_text(out.value, "…");
    lv_obj_set_flex_grow(out.value, 1);
    lv_obj_set_height(out.value, lv_font_get_line_height(fontpack_lv_font_ui()));
    lv_label_set_long_mode(out.value, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(out.value, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(out.value, lv_color_black(), 0);
    lv_obj_set_style_text_align(out.value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_clear_flag(out.value, LV_OBJ_FLAG_CLICKABLE);

    return out;
}

DualNetworkBoard* SettingsTest_GetDualBoard() {
    return dynamic_cast<DualNetworkBoard*>(&Board::GetInstance());
}

Nt26Board* SettingsTest_GetNt26Board() {
    auto& board = Board::GetInstance();
    if (auto* dual = dynamic_cast<DualNetworkBoard*>(&board)) {
        return dynamic_cast<Nt26Board*>(&dual->GetCurrentBoard());
    }
    return dynamic_cast<Nt26Board*>(&board);
}

bool SettingsTest_IsCellNetwork() {
    if (auto* dual = SettingsTest_GetDualBoard()) {
        return dual->GetNetworkType() == NetworkType::ML307;
    }
    return DualNetworkBoard::LoadNetworkTypeFromSettings(1) == NetworkType::ML307;
}
