#include "settings_log_tab.h"

#include "assets/lang_config.h"
#include "fontpack_lvgl.h"
#include "sd_log_writer.h"
#include "settings_common.h"
#include "settings_log_upload.h"

struct SettingsLogOption {
    bool enabled; // 是否写 SD 日志
    const char* (*title)(); // 选项标题
    const char* (*current_text)(); // 当前选中文案
};

static const char* LogTitleOn() { return Lang::Strings::SETTINGS_LOG_ON; }
static const char* LogTitleOff() { return Lang::Strings::SETTINGS_LOG_OFF; }
static const char* LogCurOn() { return Lang::Strings::SETTINGS_LOG_CURRENT_ON; }
static const char* LogCurOff() { return Lang::Strings::SETTINGS_LOG_CURRENT_OFF; }

static constexpr SettingsLogOption kOptions[] = {
    {true, LogTitleOn, LogCurOn},
    {false, LogTitleOff, LogCurOff},
};
static constexpr int kOptionCount = sizeof(kOptions) / sizeof(kOptions[0]);

struct SettingsLogUi {
    lv_obj_t* btns[kOptionCount] = {}; // 选项按钮
    lv_obj_t* current_lbl = nullptr; // 当前状态文案
    lv_obj_t* upload_btn = nullptr; // 上传按钮
};

static SettingsLogUi s_ui;

static const char* CurrentText(bool enabled) {
    for (int i = 0; i < kOptionCount; ++i) {
        if (kOptions[i].enabled == enabled) {
            return kOptions[i].current_text();
        }
    }
    return kOptions[0].current_text();
}

static void RefreshOptions() {
    const bool enabled = SdLogWriter_IsEnabled();
    for (int i = 0; i < kOptionCount; ++i) {
        if (s_ui.btns[i] != nullptr) {
            SettingsStyleSelectable(s_ui.btns[i], lv_obj_get_child(s_ui.btns[i], 0),
                                    kOptions[i].enabled == enabled);
        }
    }
    if (s_ui.current_lbl != nullptr) {
        lv_label_set_text(s_ui.current_lbl, CurrentText(enabled));
    }
    if (s_ui.upload_btn != nullptr) {
        SettingsStyleSelectable(s_ui.upload_btn, lv_obj_get_child(s_ui.upload_btn, 0), false);
    }
}

static void OnOptionClicked(lv_event_t* e) {
    if (SettingsLogUpload_IsActive()) {
        return;
    }
    const bool enable = (reinterpret_cast<intptr_t>(lv_event_get_user_data(e)) != 0);
    if (SdLogWriter_IsEnabled() == enable) {
        return;
    }
    SdLogWriter_SetEnabled(enable);
    RefreshOptions();
}

static void OnUploadClicked(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    SettingsLogUpload_Start();
}

void SettingsLogTab_Reset() {
    SettingsLogUpload_Teardown();
    s_ui = {};
}

void SettingsLogTab_Build(lv_obj_t* page) {
    s_ui = {};

    lv_obj_t* title = lv_label_create(page);
    lv_label_set_text(title, Lang::Strings::SETTINGS_LOG_TITLE);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* hint = lv_label_create(page);
    lv_label_set_text(hint, Lang::Strings::SETTINGS_LOG_HINT);
    lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(hint, lv_color_black(), 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* hint_off = lv_label_create(page);
    lv_label_set_text(hint_off, Lang::Strings::SETTINGS_LOG_HINT_OFF);
    lv_obj_set_style_text_font(hint_off, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(hint_off, lv_color_black(), 0);
    lv_obj_clear_flag(hint_off, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < kOptionCount; ++i) {
        s_ui.btns[i] = SettingsCreateSelectableOption(
            page, kOptions[i].title(), OnOptionClicked,
            static_cast<intptr_t>(kOptions[i].enabled ? 1 : 0));
    }

    s_ui.current_lbl = lv_label_create(page);
    lv_label_set_text(s_ui.current_lbl, "");
    lv_obj_set_style_text_font(s_ui.current_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.current_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.current_lbl, LV_OBJ_FLAG_CLICKABLE);

    s_ui.upload_btn = SettingsCreateSelectableOption(page, Lang::Strings::SETTINGS_LOG_UPLOAD,
                                                     OnUploadClicked, 0);

    RefreshOptions();
}
