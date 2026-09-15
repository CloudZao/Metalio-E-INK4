#include "settings_screen.h"

#include "assets/lang_config.h"
#include "haptic_feedback.h"
#include "home_screen/home_screen.h"
#include "lv_adapter_display.h"
#include "power_policy.h"
#include "screen_common.h"
#include "settings_about_tab.h"
#include "settings_bluetooth_tab.h"
#include "settings_common.h"
#include "settings_conversation_tab.h"
#include "settings_haptic_tab.h"
#include "settings_language_tab.h"
#include "settings_network_tab.h"
#include "settings_power_tab.h"
#include "settings_storage_tab.h"
#include "settings_test_tab.h"
#include "settings_theme_tab.h"
#include "vk_key_handler.h"

#include <cstring>

#include <esp_log.h>
#include "fontpack_lvgl.h"

namespace {

constexpr const char* TAG = "SettingsScreen";
constexpr const char* kScreenId = "settings";

enum class SettingsTab : int {
    kNetwork = 0,
    kTheme = 1,
    kLanguage = 2,
    kHaptic = 3,
    kPower = 4,
    kConversation = 5,
    kStorage = 6,
    kBluetooth = 7,
    kTest = 8,
    kAbout = 9,  // 关于始终放最后
};

struct TabEntry {
    SettingsTab id;
    const char* (*name)();
};

const char* TabNameNetwork() { return Lang::Strings::SETTINGS_TAB_NETWORK; }
const char* TabNameTheme() { return Lang::Strings::SETTINGS_TAB_THEME; }
const char* TabNameLanguage() { return Lang::Strings::SETTINGS_TAB_LANGUAGE; }
const char* TabNameHaptic() { return Lang::Strings::SETTINGS_TAB_HAPTIC; }
const char* TabNamePower() { return Lang::Strings::SETTINGS_TAB_POWER; }
const char* TabNameConversation() { return Lang::Strings::SETTINGS_TAB_CONVERSATION; }
const char* TabNameStorage() { return Lang::Strings::SETTINGS_TAB_STORAGE; }
const char* TabNameBluetooth() { return Lang::Strings::SETTINGS_TAB_BLUETOOTH; }
const char* TabNameTest() { return Lang::Strings::SETTINGS_TAB_TEST; }
const char* TabNameAbout() { return Lang::Strings::SETTINGS_TAB_ABOUT; }

// 关于 Tab 必须保持为数组最后一项
constexpr TabEntry kTabs[] = {
    {SettingsTab::kNetwork, TabNameNetwork},
    {SettingsTab::kTheme, TabNameTheme},
    {SettingsTab::kLanguage, TabNameLanguage},
    {SettingsTab::kHaptic, TabNameHaptic},
    {SettingsTab::kPower, TabNamePower},
    {SettingsTab::kConversation, TabNameConversation},
    {SettingsTab::kStorage, TabNameStorage},
    {SettingsTab::kBluetooth, TabNameBluetooth},
    {SettingsTab::kTest, TabNameTest},
    {SettingsTab::kAbout, TabNameAbout},
};
constexpr int kTabCount = sizeof(kTabs) / sizeof(kTabs[0]);
static_assert(kTabs[kTabCount - 1].id == SettingsTab::kAbout, "about tab must be last");

struct SettingsShellUi {
    lv_obj_t* tab_btns[kTabCount] = {};
    lv_obj_t* content_page = nullptr; // 右侧单页：进 Tab 再 Build，离 Tab 销毁
    int active_tab = -1;              // -1=尚未打开任一 Tab
    bool content_built = false;
};

SettingsShellUi s_ui;
SettingsTab s_pending_tab = SettingsTab::kNetwork;
lv_obj_t* s_settings_scr = nullptr; // 当前设置壳；async 删旧屏时勿清新屏指针

int TabIndexOf(SettingsTab id) {
    for (int i = 0; i < kTabCount; ++i) {
        if (kTabs[i].id == id) {
            return i;
        }
    }
    return 0;
}

// 返回栈重建时落到当前 Tab（测试专用入口）。
ScreenFactory FactoryForTab(SettingsTab id) {
    if (id == SettingsTab::kTest) {
        return SettingsScreen::CreateTest;
    }
    return SettingsScreen::Create;
}

void RefreshBackFactory(SettingsTab id) {
    VkKey_SetScreenFactory(kScreenId, FactoryForTab(id));
}

void ResetTab(SettingsTab id) {
    switch (id) {
        case SettingsTab::kNetwork:
            SettingsNetworkTab_Reset();
            break;
        case SettingsTab::kTheme:
            SettingsThemeTab_Reset();
            break;
        case SettingsTab::kLanguage:
            SettingsLanguageTab_Reset();
            break;
        case SettingsTab::kHaptic:
            SettingsHapticTab_Reset();
            break;
        case SettingsTab::kPower:
            SettingsPowerTab_Reset();
            break;
        case SettingsTab::kConversation:
            SettingsConversationTab_Reset();
            break;
        case SettingsTab::kStorage:
            SettingsStorageTab_Reset();
            break;
        case SettingsTab::kBluetooth:
            SettingsBluetoothTab_Reset();
            break;
        case SettingsTab::kTest:
            SettingsTestTab_Reset();
            break;
        case SettingsTab::kAbout:
            SettingsAboutTab_Reset();
            break;
    }
}

void BuildTab(SettingsTab id) {
    lv_obj_t* page = s_ui.content_page;
    if (page == nullptr) {
        return;
    }
    switch (id) {
        case SettingsTab::kNetwork:
            SettingsNetworkTab_Build(page);
            break;
        case SettingsTab::kTheme:
            SettingsThemeTab_Build(page);
            break;
        case SettingsTab::kLanguage:
            SettingsLanguageTab_Build(page);
            break;
        case SettingsTab::kHaptic:
            SettingsHapticTab_Build(page);
            break;
        case SettingsTab::kPower:
            SettingsPowerTab_Build(page);
            break;
        case SettingsTab::kConversation:
            SettingsConversationTab_Build(page);
            break;
        case SettingsTab::kStorage:
            SettingsStorageTab_Build(page);
            break;
        case SettingsTab::kBluetooth:
            SettingsBluetoothTab_Build(page);
            break;
        case SettingsTab::kTest:
            SettingsTestTab_Build(page);
            break;
        case SettingsTab::kAbout:
            SettingsAboutTab_Build(page);
            break;
    }
    s_ui.content_built = true;
}

void TearDownActiveTab() {
    if (s_ui.content_page == nullptr || !s_ui.content_built || s_ui.active_tab < 0 ||
        s_ui.active_tab >= kTabCount) {
        return;
    }
    const SettingsTab id = kTabs[s_ui.active_tab].id;
    if (id == SettingsTab::kBluetooth) {
        SettingsBluetoothTab_OnDeactivated();
    }
    if (id == SettingsTab::kTest) {
        SettingsTestTab_OnDeactivated();
    }
    // Reset 先清 alive/指针，异步回调不再碰已删控件
    ResetTab(id);
    lv_obj_clean(s_ui.content_page);
    s_ui.content_built = false;
}

void ShowTab(int index) {
    if (index < 0 || index >= kTabCount) {
        return;
    }
    if (s_ui.content_built && s_ui.active_tab == index) {
        return;
    }

    TearDownActiveTab();
    s_ui.active_tab = index;

    for (int i = 0; i < kTabCount; ++i) {
        if (s_ui.tab_btns[i] != nullptr) {
            SettingsStyleSelectable(s_ui.tab_btns[i], lv_obj_get_child(s_ui.tab_btns[i], 0),
                                    i == index);
        }
    }

    const SettingsTab id = kTabs[index].id;
    BuildTab(id);

    if (id == SettingsTab::kConversation) {
        SettingsConversationTab_OnActivated();
    }
    if (id == SettingsTab::kBluetooth) {
        SettingsBluetoothTab_OnActivated();
    }
    if (id == SettingsTab::kTest) {
        SettingsTestTab_OnActivated();
    }
    RefreshBackFactory(id);
}

void OnTabClicked(lv_event_t* e) {
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    ShowTab(index);
}

lv_obj_t* CreateTabButton(lv_obj_t* parent, const TabEntry& entry, int index) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, kSettingsTabW, kSettingsTabH);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(btn);
    lv_obj_add_event_cb(btn, OnTabClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, entry.name());
    lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

    SettingsStyleSelectable(btn, lbl, false);
    return btn;
}

bool OnSettingsVkKey(const char* key_name) {
    (void)key_name;
    return false;
}

bool OnSettingsVkKeyLongPress(const char* key_name) {
    (void)key_name;
    return false;
}

bool OnSettingsVkKeyPressUp(const char* key_name) {
    (void)key_name;
    return false;
}

void OnScreenDeleted(lv_event_t* e) {
    // ScreenLoadReplace 会 async 删旧屏；勿清掉新设置页的 s_ui（否则右侧内容变空白）
    if (lv_event_get_target(e) != s_settings_scr) {
        return;
    }
    TearDownActiveTab();
    s_ui = {};
    s_settings_scr = nullptr;
}

lv_obj_t* CreateInternal() {
    s_ui = {};

    lv_obj_t* scr = lv_obj_create(nullptr);
    s_settings_scr = scr;
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, OnScreenDeleted, LV_EVENT_DELETE, nullptr);

    EpdStatusBar status = ScreenCreateStatusBar(scr);

    const lv_coord_t body_h = LV_VER_RES - status.height;

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_HOR_RES, body_h);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, status.height);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(body, kSettingsBodyPad, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(body, kSettingsBodyPad, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_CLICKABLE);

    // 左侧 Tab
    lv_obj_t* tab_col = lv_obj_create(body);
    lv_obj_remove_style_all(tab_col);
    lv_obj_set_size(tab_col, kSettingsTabW, body_h - kSettingsBodyPad * 2);
    lv_obj_set_style_bg_opa(tab_col, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(tab_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tab_col, kSettingsTabGap, 0);
    lv_obj_clear_flag(tab_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_col, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < kTabCount; ++i) {
        s_ui.tab_btns[i] = CreateTabButton(tab_col, kTabs[i], i);
    }

    // 分隔线
    lv_obj_t* split = lv_obj_create(body);
    lv_obj_remove_style_all(split);
    lv_obj_set_size(split, kSettingsSplitLineW, body_h - kSettingsBodyPad * 2);
    lv_obj_set_style_bg_color(split, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(split, LV_OPA_COVER, 0);
    lv_obj_clear_flag(split, LV_OBJ_FLAG_CLICKABLE);

    // 右侧单内容页：仅当前 Tab 持有控件树
    lv_obj_t* content = lv_obj_create(body);
    lv_obj_remove_style_all(content);
    lv_obj_set_height(content, body_h - kSettingsBodyPad * 2);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content, kSettingsContentPad, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, kSettingsOptionGap, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_CLICKABLE);
    s_ui.content_page = content;

    const SettingsTab open_tab = s_pending_tab;
    s_pending_tab = SettingsTab::kNetwork;

    ScreenSetIsHome(false);
    VkKey_AttachScreen(scr, kScreenId,
                       VkKeyScreenDesc{FactoryForTab(open_tab), OnSettingsVkKey, nullptr, nullptr,
                                       nullptr, nullptr, OnSettingsVkKeyLongPress,
                                       OnSettingsVkKeyPressUp});
    ShowTab(TabIndexOf(open_tab));
    ESP_LOGI(TAG, "settings ready tabs=%d open=%d network=%s card_style=%d haptic=%d idle_mhz=%d",
             kTabCount, static_cast<int>(open_tab), SettingsNetworkTab_CurrentName(),
             HomeScreen::LoadCardStyle(), HapticIsEnabled() ? 1 : 0,
             PowerPolicy::GetInstance().GetIdleCpuMhz());
    return scr;
}

}  // namespace

lv_obj_t* SettingsScreen::Create() {
    s_pending_tab = SettingsTab::kNetwork;
    return CreateInternal();
}

lv_obj_t* SettingsScreen::CreateTest() {
    s_pending_tab = SettingsTab::kTest;
    return CreateInternal();
}

void SettingsScreen::ReloadAfterLanguageChange() {
    s_pending_tab = SettingsTab::kLanguage;
    ScreenLoadReplace(CreateInternal());
}
