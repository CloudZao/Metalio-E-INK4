#include "settings_language_tab.h"

#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "device_state.h"
#include "display.h"
#include "fontpack_lvgl.h"
#include "screen_common.h"
#include "settings.h"
#include "settings_common.h"
#include "settings_screen.h"

#include <cstring>
#include <string>

namespace {

struct LangOption {
    const char* code;
    const char* (*title)();
};

const char* TitleZh() {
    return Lang::Strings::SETTINGS_LANG_ZH_CN;
}

const char* TitleEn() {
    return Lang::Strings::SETTINGS_LANG_EN_US;
}

constexpr LangOption kLangOptions[] = {
    {"zh-CN", TitleZh},
    {"en-US", TitleEn},
};
constexpr int kLangOptionCount = sizeof(kLangOptions) / sizeof(kLangOptions[0]);

struct LangUi {
    lv_obj_t* btns[kLangOptionCount] = {};
    lv_obj_t* current_lbl = nullptr;
};

LangUi s_ui;

const char* CurrentLabelText() {
    if (std::strcmp(Lang::CODE, "zh-CN") == 0) {
        return Lang::Strings::SETTINGS_LANG_CURRENT_ZH;
    }
    return Lang::Strings::SETTINGS_LANG_CURRENT_EN;
}

void RefreshLangOptions() {
    for (int i = 0; i < kLangOptionCount; ++i) {
        if (s_ui.btns[i] != nullptr) {
            SettingsStyleSelectable(s_ui.btns[i], lv_obj_get_child(s_ui.btns[i], 0),
                                    std::strcmp(Lang::CODE, kLangOptions[i].code) == 0);
        }
    }
    if (s_ui.current_lbl != nullptr) {
        lv_label_set_text(s_ui.current_lbl, CurrentLabelText());
    }
}

void RefreshStatusAfterLanguageChange() {
    auto* display = Board::GetInstance().GetDisplay();
    if (display == nullptr) {
        return;
    }
    const DeviceState state = Application::GetInstance().GetDeviceState();
    switch (state) {
        case kDeviceStateListening:
            display->SetStatus(Lang::Strings::LISTENING);
            break;
        case kDeviceStateSpeaking:
            display->SetStatus(Lang::Strings::SPEAKING);
            break;
        case kDeviceStateConnecting:
            display->SetStatus(Lang::Strings::CONNECTING);
            break;
        case kDeviceStateWifiConfiguring:
            display->SetStatus(Lang::Strings::WIFI_CONFIG_MODE);
            break;
        case kDeviceStateActivating:
            display->SetStatus(Lang::Strings::ACTIVATION);
            break;
        case kDeviceStateUpgrading:
            display->SetStatus(Lang::Strings::UPGRADING);
            break;
        case kDeviceStateStarting:
            display->SetStatus(Lang::Strings::INITIALIZING);
            break;
        case kDeviceStateIdle:
        default:
            display->SetStatus(Lang::Strings::STANDBY);
            break;
    }
    display->UpdateStatusBar(true);
}

void OnLangOptionClicked(lv_event_t* e) {
    const char* code = static_cast<const char*>(lv_event_get_user_data(e));
    if (code == nullptr || std::strcmp(Lang::CODE, code) == 0) {
        return;
    }
    // LVGL 栈在 PSRAM：勿在此路径 NVS/flash（关 cache 会 assert）
    Lang::SetLanguage(code, false);
    const std::string lang(code);
    Application::GetInstance().Schedule([lang]() {
        Settings settings("ui", true);
        settings.SetString("language", lang);
    });
    RefreshStatusAfterLanguageChange();
    SettingsScreen::ReloadAfterLanguageChange();
}

}  // namespace

void SettingsLanguageTab_Reset() {
    s_ui = {};
}

void SettingsLanguageTab_Build(lv_obj_t* page) {
    s_ui = {};

    lv_obj_t* title = lv_label_create(page);
    lv_label_set_text(title, Lang::Strings::SETTINGS_LANG_TITLE);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* hint = lv_label_create(page);
    lv_label_set_text(hint, Lang::Strings::SETTINGS_LANG_HINT);
    lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(hint, lv_color_black(), 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < kLangOptionCount; ++i) {
        s_ui.btns[i] = SettingsCreateSelectableOption(page, kLangOptions[i].title(),
                                                      OnLangOptionClicked,
                                                      reinterpret_cast<intptr_t>(kLangOptions[i].code));
    }

    s_ui.current_lbl = lv_label_create(page);
    lv_label_set_text(s_ui.current_lbl, "");
    lv_obj_set_style_text_font(s_ui.current_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.current_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.current_lbl, LV_OBJ_FLAG_CLICKABLE);

    RefreshLangOptions();
}
