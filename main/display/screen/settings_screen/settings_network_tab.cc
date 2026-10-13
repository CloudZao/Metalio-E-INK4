#include "settings_network_tab_priv.h"

#include "settings_network_tab_ui.h"
#include "settings_network_sim.h"
#include "application.h"
#include "board.h"
#include "dual_network_board.h"
#include "nt26_board.h"
#include "network_screen/network_screen.h"
#include "settings.h"
#include "settings_common.h"
#include "screen_common.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "fontpack_lvgl.h"
#include "assets/lang_config.h"

NetworkUi SettingsNetwork_ui;
bool SettingsNetwork_sim_switch_pending = false;
int SettingsNetwork_cached_sim_slot = kSimSlotExternal;
bool SettingsNetwork_sim_slot_ready = false;
bool SettingsNetwork_tab_alive = false;

DualNetworkBoard* SettingsNetwork_GetDualBoard() {
    return dynamic_cast<DualNetworkBoard*>(&Board::GetInstance());
}

Nt26Board* SettingsNetwork_GetNt26Board() {
    auto& board = Board::GetInstance();
    if (auto* dual = dynamic_cast<DualNetworkBoard*>(&board)) {
        return dynamic_cast<Nt26Board*>(&dual->GetCurrentBoard());
    }
    return dynamic_cast<Nt26Board*>(&board);
}

NetworkType SettingsNetwork_CurrentNetworkType() {
    if (auto* dual = SettingsNetwork_GetDualBoard()) {
        return dual->GetNetworkType();
    }
    return DualNetworkBoard::LoadNetworkTypeFromSettings(1);
}

bool SettingsNetwork_IsCellNetwork() { return SettingsNetwork_CurrentNetworkType() == NetworkType::ML307; }

int SettingsNetwork_GetCachedSimSlot() {
    return (SettingsNetwork_cached_sim_slot == kSimSlotInternal) ? kSimSlotInternal : kSimSlotExternal;
}

void SettingsNetwork_SetCachedSimSlot(int slot) {
    SettingsNetwork_cached_sim_slot = (slot == kSimSlotInternal) ? kSimSlotInternal : kSimSlotExternal;
    SettingsNetwork_sim_slot_ready = true;
}

void SettingsNetwork_PersistSimSlotToNvs(int slot) {
    const int normalized = (slot == kSimSlotInternal) ? kSimSlotInternal : kSimSlotExternal;
    Settings settings("network", true);
    settings.SetInt("sim_slot", normalized);
}

void SettingsNetwork_SchedulePersistSimSlot(int slot) {
    Application::GetInstance().Schedule([slot]() { SettingsNetwork_PersistSimSlotToNvs(slot); });
}

const char* SettingsNetwork_SimSlotName(int slot) {
    return (slot == kSimSlotInternal) ? Lang::Strings::SETTINGS_NET_SIM_INT : Lang::Strings::SETTINGS_NET_SIM_EXT;
}

void SettingsNetwork_CloseOverlay() {
    if (SettingsNetwork_ui.reboot_timer != nullptr) {
        lv_timer_delete(SettingsNetwork_ui.reboot_timer);
        SettingsNetwork_ui.reboot_timer = nullptr;
    }
    if (SettingsNetwork_ui.overlay != nullptr) {
        lv_obj_delete(SettingsNetwork_ui.overlay);
    }
    SettingsNetwork_ui.overlay = nullptr;
    SettingsNetwork_ui.overlay_msg = nullptr;
    SettingsNetwork_ui.reboot_remaining = 0;
    SettingsNetwork_ui.reboot_headline.clear();
}

void SettingsNetwork_OpenOverlay(const char* msg) {
    if (SettingsNetwork_ui.root_scr == nullptr) {
        return;
    }
    SettingsNetwork_CloseOverlay();

    lv_obj_t* mask = lv_obj_create(SettingsNetwork_ui.root_scr);
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(mask, 0, 0);
    ScreenApplyDotBackdrop(mask);
    lv_obj_set_style_border_width(mask, 0, 0);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    SettingsNetwork_ui.overlay = mask;

    lv_obj_t* card = lv_obj_create(mask);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, LV_HOR_RES - 40, 180);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(card);
    SettingsNetwork_ui.overlay_msg = lbl;
    lv_label_set_text(lbl, msg != nullptr ? msg : "");
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
}

void SettingsNetwork_SetOverlayText(const char* msg) {
    if (SettingsNetwork_ui.overlay_msg != nullptr) {
        lv_label_set_text(SettingsNetwork_ui.overlay_msg, msg != nullptr ? msg : "");
    }
}

void SettingsNetwork_RebootTask(void* /*arg*/) {
    vTaskDelay(pdMS_TO_TICKS(200));
    Application::GetInstance().Reboot();
    vTaskDelete(nullptr);
}

void SettingsNetwork_RebootTimerCb(lv_timer_t* /*timer*/) {
    SettingsNetwork_ui.reboot_remaining--;
    if (SettingsNetwork_ui.reboot_remaining > 0) {
        if (SettingsNetwork_ui.overlay_msg != nullptr) {
            char buf[128];
            snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_NET_REBOOT_COUNT_FMT, SettingsNetwork_ui.reboot_headline.c_str(),
                     SettingsNetwork_ui.reboot_remaining);
            lv_label_set_text(SettingsNetwork_ui.overlay_msg, buf);
        }
        return;
    }
    if (SettingsNetwork_ui.reboot_timer != nullptr) {
        lv_timer_delete(SettingsNetwork_ui.reboot_timer);
        SettingsNetwork_ui.reboot_timer = nullptr;
    }
    SettingsNetwork_SetOverlayText(Lang::Strings::SETTINGS_NET_REBOOTING);
    xTaskCreate(SettingsNetwork_RebootTask, "sim_reboot", 2048, nullptr, 5, nullptr);
}

void SettingsNetwork_OpenRestartCountdown(const char* headline) {
    SettingsNetwork_ui.reboot_headline = headline != nullptr ? headline : "";
    SettingsNetwork_ui.reboot_remaining = 3;
    char buf[128];
    snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_NET_REBOOT_COUNT_FMT, SettingsNetwork_ui.reboot_headline.c_str(),
             SettingsNetwork_ui.reboot_remaining);
    SettingsNetwork_OpenOverlay(buf);
    if (SettingsNetwork_ui.reboot_timer != nullptr) {
        lv_timer_delete(SettingsNetwork_ui.reboot_timer);
    }
    SettingsNetwork_ui.reboot_timer = lv_timer_create(SettingsNetwork_RebootTimerCb, 1000, nullptr);
}

void SettingsNetwork_RefreshSimUi() {
    if (SettingsNetwork_ui.sim_ext_btn == nullptr || SettingsNetwork_ui.sim_int_btn == nullptr) {
        return;
    }
    const int slot = SettingsNetwork_GetCachedSimSlot();
    const bool ext = (slot == kSimSlotExternal);
    SettingsStyleSelectable(SettingsNetwork_ui.sim_ext_btn, lv_obj_get_child(SettingsNetwork_ui.sim_ext_btn, 0), ext);
    SettingsStyleSelectable(SettingsNetwork_ui.sim_int_btn, lv_obj_get_child(SettingsNetwork_ui.sim_int_btn, 0), !ext);
    if (SettingsNetwork_ui.sim_cur_lbl != nullptr) {
        char buf[48];
        snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_NET_CURRENT_FMT, SettingsNetwork_SimSlotName(slot));
        lv_label_set_text(SettingsNetwork_ui.sim_cur_lbl, buf);
    }
}

void SettingsNetwork_SetSimSectionVisible(bool visible) {
    if (SettingsNetwork_ui.sim_box == nullptr) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(SettingsNetwork_ui.sim_box, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(SettingsNetwork_ui.sim_box, LV_OBJ_FLAG_HIDDEN);
    }
}

void SettingsNetwork_SetWifiConfigSectionVisible(bool visible) {
    if (SettingsNetwork_ui.wifi_cfg_box == nullptr) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(SettingsNetwork_ui.wifi_cfg_box, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(SettingsNetwork_ui.wifi_cfg_box, LV_OBJ_FLAG_HIDDEN);
    }
}

void SettingsNetwork_RefreshNetworkOptions() {
    const bool is_cell = SettingsNetwork_IsCellNetwork();
    if (SettingsNetwork_ui.wifi_btn != nullptr) {
        SettingsStyleSelectable(SettingsNetwork_ui.wifi_btn, lv_obj_get_child(SettingsNetwork_ui.wifi_btn, 0), !is_cell);
    }
    if (SettingsNetwork_ui.cell_btn != nullptr) {
        SettingsStyleSelectable(SettingsNetwork_ui.cell_btn, lv_obj_get_child(SettingsNetwork_ui.cell_btn, 0), is_cell);
    }
    if (SettingsNetwork_ui.current_lbl != nullptr) {
        lv_label_set_text(SettingsNetwork_ui.current_lbl, is_cell ? Lang::Strings::SETTINGS_NET_CUR_4G : Lang::Strings::SETTINGS_NET_CUR_WIFI);
    }
    // 仅当前为 WiFi 时展示「配置 WIFI」；仅 4G 时展示内外置卡切换
    SettingsNetwork_SetWifiConfigSectionVisible(!is_cell);
    SettingsNetwork_SetSimSectionVisible(is_cell);
    if (is_cell) {
        SettingsNetwork_RefreshSimUi();
    }
}

void SettingsNetwork_SwitchNetworkTask(void* arg) {
    auto* job = static_cast<SwitchNetworkJob*>(arg);
    auto* dual = SettingsNetwork_GetDualBoard();
    if (dual == nullptr) {
        ESP_LOGW(TAG, "not a DualNetworkBoard, ignore network switch");
    } else {
        ESP_LOGI(TAG, "switch network to %s", job->target == NetworkType::WIFI ? "WiFi" : "4G");
        dual->SwitchToNetworkType(job->target);
    }
    delete job;
    vTaskDelete(nullptr);
}

void SettingsNetwork_OnNetworkOptionClicked(lv_event_t* e) {
    const auto target = static_cast<NetworkType>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (SettingsNetwork_CurrentNetworkType() == target) {
        // 点已选中的 4G：确保 SIM 区可见；点已选中的 WiFi：确保配网区可见
        if (target == NetworkType::ML307) {
            SettingsNetwork_SetSimSectionVisible(true);
            SettingsNetwork_RefreshSimUi();
        } else {
            SettingsNetwork_SetWifiConfigSectionVisible(true);
        }
        return;
    }
    if (target == NetworkType::WIFI) {
        SettingsStyleSelectable(SettingsNetwork_ui.wifi_btn, lv_obj_get_child(SettingsNetwork_ui.wifi_btn, 0), true);
        SettingsStyleSelectable(SettingsNetwork_ui.cell_btn, lv_obj_get_child(SettingsNetwork_ui.cell_btn, 0), false);
        SettingsNetwork_SetSimSectionVisible(false);
        SettingsNetwork_SetWifiConfigSectionVisible(true);
        if (SettingsNetwork_ui.current_lbl != nullptr) {
            lv_label_set_text(SettingsNetwork_ui.current_lbl, Lang::Strings::SETTINGS_NET_SWITCH_WIFI);
        }
    } else {
        SettingsStyleSelectable(SettingsNetwork_ui.wifi_btn, lv_obj_get_child(SettingsNetwork_ui.wifi_btn, 0), false);
        SettingsStyleSelectable(SettingsNetwork_ui.cell_btn, lv_obj_get_child(SettingsNetwork_ui.cell_btn, 0), true);
        SettingsNetwork_SetWifiConfigSectionVisible(false);
        SettingsNetwork_SetSimSectionVisible(true);
        SettingsNetwork_RefreshSimUi();
        if (SettingsNetwork_ui.current_lbl != nullptr) {
            lv_label_set_text(SettingsNetwork_ui.current_lbl, Lang::Strings::SETTINGS_NET_SWITCH_4G);
        }
    }
    auto* job = new SwitchNetworkJob{target};
    if (xTaskCreate(SettingsNetwork_SwitchNetworkTask, "net_switch", 4096, job, 5, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate(net_switch) failed");
        delete job;
        SettingsNetwork_RefreshNetworkOptions();
    }
}

// SIM 查询/切换：AT+CFUN=0 -> AT+ECSIMCFG=SimSlot,X -> AT+CFUN=1。

void SettingsNetwork_BuildWifiConfigSection(lv_obj_t* page) {
    SettingsNetwork_ui.wifi_cfg_box = lv_obj_create(page);
    lv_obj_remove_style_all(SettingsNetwork_ui.wifi_cfg_box);
    lv_obj_set_width(SettingsNetwork_ui.wifi_cfg_box, lv_pct(100));
    lv_obj_set_height(SettingsNetwork_ui.wifi_cfg_box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(SettingsNetwork_ui.wifi_cfg_box, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(SettingsNetwork_ui.wifi_cfg_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(SettingsNetwork_ui.wifi_cfg_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(SettingsNetwork_ui.wifi_cfg_box, kSettingsOptionGap, 0);
    lv_obj_clear_flag(SettingsNetwork_ui.wifi_cfg_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(SettingsNetwork_ui.wifi_cfg_box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* split = lv_obj_create(SettingsNetwork_ui.wifi_cfg_box);
    lv_obj_remove_style_all(split);
    lv_obj_set_size(split, lv_pct(100), 2);
    lv_obj_set_style_bg_color(split, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(split, LV_OPA_COVER, 0);
    lv_obj_clear_flag(split, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(SettingsNetwork_ui.wifi_cfg_box);
    lv_label_set_text(title, Lang::Strings::SETTINGS_NET_WIFI_CFG_TITLE);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* hint = lv_label_create(SettingsNetwork_ui.wifi_cfg_box);
    lv_label_set_text(hint, Lang::Strings::SETTINGS_NET_WIFI_CFG_HINT);
    lv_obj_set_width(hint, lv_pct(100));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(hint, lv_color_black(), 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);

    SettingsNetwork_ui.wifi_cfg_btn =
        SettingsCreateSelectableOption(SettingsNetwork_ui.wifi_cfg_box, Lang::Strings::SETTINGS_NET_ENTER_CFG, SettingsNetwork_OnEnterWifiConfigClicked, 0);
    SettingsStyleSelectable(SettingsNetwork_ui.wifi_cfg_btn, lv_obj_get_child(SettingsNetwork_ui.wifi_cfg_btn, 0), false);

    SettingsNetwork_SetWifiConfigSectionVisible(false);
}

void SettingsNetwork_BuildSimSection(lv_obj_t* page) {
    SettingsNetwork_ui.sim_box = lv_obj_create(page);
    lv_obj_remove_style_all(SettingsNetwork_ui.sim_box);
    lv_obj_set_width(SettingsNetwork_ui.sim_box, lv_pct(100));
    lv_obj_set_height(SettingsNetwork_ui.sim_box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(SettingsNetwork_ui.sim_box, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(SettingsNetwork_ui.sim_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(SettingsNetwork_ui.sim_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(SettingsNetwork_ui.sim_box, kSettingsOptionGap, 0);
    lv_obj_clear_flag(SettingsNetwork_ui.sim_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(SettingsNetwork_ui.sim_box, LV_OBJ_FLAG_CLICKABLE);

    // SIM 区与上方上网方式之间的分割横线
    lv_obj_t* split = lv_obj_create(SettingsNetwork_ui.sim_box);
    lv_obj_remove_style_all(split);
    lv_obj_set_size(split, lv_pct(100), 2);
    lv_obj_set_style_bg_color(split, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(split, LV_OPA_COVER, 0);
    lv_obj_clear_flag(split, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(SettingsNetwork_ui.sim_box);
    lv_label_set_text(title, Lang::Strings::SETTINGS_NET_SIM_TITLE);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* hint = lv_label_create(SettingsNetwork_ui.sim_box);
    lv_label_set_text(hint, Lang::Strings::SETTINGS_NET_REBOOT_HINT);
    lv_obj_set_width(hint, lv_pct(100));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(hint, lv_color_black(), 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);

    SettingsNetwork_ui.sim_ext_btn =
        SettingsCreateSelectableOption(SettingsNetwork_ui.sim_box, Lang::Strings::SETTINGS_NET_SIM_EXT, SettingsNetwork_OnSimExtClicked, 0);
    SettingsNetwork_ui.sim_int_btn =
        SettingsCreateSelectableOption(SettingsNetwork_ui.sim_box, Lang::Strings::SETTINGS_NET_SIM_INT, SettingsNetwork_OnSimIntClicked, 0);

    SettingsNetwork_ui.sim_cur_lbl = lv_label_create(SettingsNetwork_ui.sim_box);
    lv_label_set_text(SettingsNetwork_ui.sim_cur_lbl, Lang::Strings::SETTINGS_NET_CUR_DASH);
    lv_obj_set_style_text_font(SettingsNetwork_ui.sim_cur_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(SettingsNetwork_ui.sim_cur_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(SettingsNetwork_ui.sim_cur_lbl, LV_OBJ_FLAG_CLICKABLE);

    SettingsNetwork_SetSimSectionVisible(false);
}

void SettingsNetworkTab_Reset() {
    SettingsNetwork_CloseOverlay();
    SettingsNetwork_tab_alive = false;
    SettingsNetwork_sim_switch_pending = false;
    SettingsNetwork_ui = {};
}

const char* SettingsNetworkTab_CurrentName() {
    return SettingsNetwork_IsCellNetwork() ? "4G" : "WiFi";
}

bool SettingsNetworkTab_IsWifi() {
    return !SettingsNetwork_IsCellNetwork();
}

void SettingsNetworkTab_LoadSimSlotFromNvsSync() {
    Settings settings("network", false);
    const int v = settings.GetInt("sim_slot", kSimSlotExternal);
    SettingsNetwork_SetCachedSimSlot(v);
}

void SettingsNetworkTab_Build(lv_obj_t* page) {
    SettingsNetwork_ui = {};
    SettingsNetwork_tab_alive = true;
    SettingsNetwork_sim_switch_pending = false;
    SettingsNetwork_ui.root_scr = lv_obj_get_screen(page);

    lv_obj_t* title = lv_label_create(page);
    lv_label_set_text(title, Lang::Strings::SETTINGS_NET_MODE_TITLE);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* hint = lv_label_create(page);
    lv_label_set_text(hint, Lang::Strings::SETTINGS_NET_REBOOT_HINT);
    lv_obj_set_width(hint, lv_pct(100));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(hint, lv_color_black(), 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);

    // 4G 在前：与出厂默认一致
    SettingsNetwork_ui.cell_btn = SettingsCreateSelectableOption(page, Lang::Strings::SETTINGS_NET_MODE_4G, SettingsNetwork_OnNetworkOptionClicked,
                                                   static_cast<intptr_t>(NetworkType::ML307));
    SettingsNetwork_ui.wifi_btn = SettingsCreateSelectableOption(page, Lang::Strings::SETTINGS_NET_MODE_WIFI, SettingsNetwork_OnNetworkOptionClicked,
                                                   static_cast<intptr_t>(NetworkType::WIFI));

    SettingsNetwork_ui.current_lbl = lv_label_create(page);
    lv_label_set_text(SettingsNetwork_ui.current_lbl, "");
    lv_obj_set_style_text_font(SettingsNetwork_ui.current_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(SettingsNetwork_ui.current_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(SettingsNetwork_ui.current_lbl, LV_OBJ_FLAG_CLICKABLE);

    SettingsNetwork_BuildWifiConfigSection(page);
    SettingsNetwork_BuildSimSection(page);
    SettingsNetwork_RefreshNetworkOptions();
    SettingsNetwork_ScheduleLoadSimSlotFromNvs();
    if (SettingsNetwork_IsCellNetwork()) {
        SettingsNetwork_ScheduleSimSlotQuery();
    }
}
