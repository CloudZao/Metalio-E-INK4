#include "settings_network_tab_priv.h"

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

int SettingsNetwork_ParseSimSlotFromEcsimcfg(const std::string& resp) {
    constexpr const char* kKey = "\"SimSlot\"";
    size_t pos = 0;
    while ((pos = resp.find(kKey, pos)) != std::string::npos) {
        size_t comma = resp.find(',', pos);
        if (comma == std::string::npos) {
            return -1;
        }
        size_t i = comma + 1;
        while (i < resp.size() && (resp[i] == ' ' || resp[i] == '\t')) {
            ++i;
        }
        if (i >= resp.size() || !std::isdigit(static_cast<unsigned char>(resp[i]))) {
            pos = comma + 1;
            continue;
        }
        int slot = 0;
        while (i < resp.size() && std::isdigit(static_cast<unsigned char>(resp[i]))) {
            slot = slot * 10 + (resp[i] - '0');
            ++i;
        }
        return slot;
    }
    return -1;
}

void SettingsNetwork_AsyncSimSlotQueried(void* user_data) {
    auto* msg = static_cast<SimSlotQueryMsg*>(user_data);
    if (SettingsNetwork_tab_alive && msg->slot >= 0) {
        if (SettingsNetwork_GetCachedSimSlot() != msg->slot) {
            SettingsNetwork_SetCachedSimSlot(msg->slot);
            SettingsNetwork_SchedulePersistSimSlot(msg->slot);
            ESP_LOGI(TAG, "sim_slot synced from modem: %d", msg->slot);
        }
        SettingsNetwork_RefreshSimUi();
    }
    delete msg;
}

void SettingsNetwork_SimSlotQueryTask(void* /*arg*/) {
    auto* msg = new SimSlotQueryMsg{-1};
    Nt26Board* nt26 = SettingsNetwork_GetNt26Board();
    if (nt26 != nullptr) {
        std::string resp;
        esp_err_t err = nt26->SendAtCommand("AT+ECSIMCFG?", resp, 5000, true);
        ESP_LOGW(TAG, "AT 'AT+ECSIMCFG?' -> err=%d", (int)err);
        if (err == ESP_OK && resp.find("OK") != std::string::npos) {
            int slot = SettingsNetwork_ParseSimSlotFromEcsimcfg(resp);
            if (slot == kSimSlotExternal || slot == kSimSlotInternal) {
                msg->slot = slot;
            }
        }
    }
    if (!ScreenLvAsync(SettingsNetwork_AsyncSimSlotQueried, msg)) {
        delete msg;
    }
    vTaskDelete(nullptr);
}

void SettingsNetwork_ScheduleSimSlotQuery() {
    if (SettingsNetwork_GetNt26Board() == nullptr) {
        return;
    }
    if (xTaskCreate(SettingsNetwork_SimSlotQueryTask, "sim_query", 4096, nullptr, 5, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate(sim_query) failed");
    }
}

void SettingsNetwork_AsyncSimProgress(void* user_data) {
    auto* msg = static_cast<AsyncStringMsg*>(user_data);
    if (SettingsNetwork_tab_alive) {
        SettingsNetwork_SetOverlayText(msg->text.c_str());
    }
    delete msg;
}

void SettingsNetwork_PostSimProgress(const std::string& text) {
    if (!SettingsNetwork_tab_alive) {
        return;
    }
    auto* msg = new AsyncStringMsg{text};
    if (!ScreenLvAsync(SettingsNetwork_AsyncSimProgress, msg)) {
        delete msg;
    }
}

void SettingsNetwork_AsyncSimSwitchDone(void* user_data) {
    auto* msg = static_cast<SimSwitchResultMsg*>(user_data);
    SettingsNetwork_sim_switch_pending = false;
    if (SettingsNetwork_tab_alive) {
        if (msg->success) {
            char buf[64];
            snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_NET_SWITCHED_FMT, SettingsNetwork_SimSlotName(msg->target_slot));
            SettingsNetwork_OpenRestartCountdown(buf);
            SettingsNetwork_RefreshSimUi();
        } else {
            SettingsNetwork_CloseOverlay();
            SettingsNetwork_ScheduleSimSlotQuery();
            SettingsNetwork_RefreshSimUi();
            if (SettingsNetwork_ui.sim_cur_lbl != nullptr) {
                lv_label_set_text(SettingsNetwork_ui.sim_cur_lbl, msg->detail.c_str());
            }
        }
    }
    delete msg;
}

void SettingsNetwork_PostSimSwitchDone(SimSwitchResultMsg* result) {
    if (result == nullptr) {
        return;
    }
    if (!ScreenLvAsync(SettingsNetwork_AsyncSimSwitchDone, result)) {
        SettingsNetwork_sim_switch_pending = false;
        delete result;
    }
}

void SettingsNetwork_SimSwitchTask(void* arg) {
    auto* ctx = static_cast<SimSwitchCtx*>(arg);
    auto* result = new SimSwitchResultMsg{};
    result->target_slot = ctx->target_slot;
    result->success = false;

    Nt26Board* nt26 = SettingsNetwork_GetNt26Board();
    if (nt26 == nullptr) {
        result->detail = Lang::Strings::SETTINGS_NET_NO_4G;
        SettingsNetwork_PostSimSwitchDone(result);
        delete ctx;
        vTaskDelete(nullptr);
        return;
    }

    auto run_at = [&](const std::string& cmd, std::string& resp, uint32_t timeout_ms) {
        resp.clear();
        esp_err_t e = nt26->SendAtCommand(cmd, resp, timeout_ms, true);
        ESP_LOGW(TAG, "AT '%s' -> err=%d resp='%s'", cmd.c_str(), (int)e, resp.c_str());
        return e;
    };

    SettingsNetwork_PostSimProgress(Lang::Strings::SETTINGS_NET_CFUN0);
    std::string resp;
    if (run_at("AT+CFUN=0", resp, 8000) != ESP_OK || resp.find("OK") == std::string::npos) {
        result->detail = Lang::Strings::SETTINGS_NET_CFUN0_FAIL;
        SettingsNetwork_PostSimSwitchDone(result);
        delete ctx;
        vTaskDelete(nullptr);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+ECSIMCFG=SimSlot,%d", ctx->target_slot);
    char prog[96];
    snprintf(prog, sizeof(prog), Lang::Strings::SETTINGS_NET_SWITCH_SIM_FMT, SettingsNetwork_SimSlotName(ctx->target_slot), cmd);
    SettingsNetwork_PostSimProgress(prog);
    if (run_at(cmd, resp, 5000) != ESP_OK || resp.find("OK") == std::string::npos) {
        std::string tmp;
        run_at("AT+CFUN=1", tmp, 10000);
        result->detail = Lang::Strings::SETTINGS_NET_ECSIMCFG_FAIL;
        SettingsNetwork_PostSimSwitchDone(result);
        delete ctx;
        vTaskDelete(nullptr);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    SettingsNetwork_PostSimProgress(Lang::Strings::SETTINGS_NET_CFUN1);
    run_at("AT+CFUN=1", resp, 15000);

    SettingsNetwork_SetCachedSimSlot(ctx->target_slot);
    SettingsNetwork_PersistSimSlotToNvs(ctx->target_slot);
    result->success = true;
    SettingsNetwork_PostSimSwitchDone(result);
    delete ctx;
    vTaskDelete(nullptr);
}

void SettingsNetwork_ScheduleSimSwitch(int target_slot) {
    if (SettingsNetwork_sim_switch_pending) {
        return;
    }
    if (target_slot != kSimSlotExternal && target_slot != kSimSlotInternal) {
        return;
    }
    if (SettingsNetwork_GetCachedSimSlot() == target_slot) {
        return;
    }
    if (!SettingsNetwork_IsCellNetwork() || SettingsNetwork_GetNt26Board() == nullptr) {
        if (SettingsNetwork_ui.sim_cur_lbl != nullptr) {
            lv_label_set_text(SettingsNetwork_ui.sim_cur_lbl, Lang::Strings::SETTINGS_NET_NOT_4G);
        }
        return;
    }

    SettingsNetwork_sim_switch_pending = true;
    char buf[80];
    snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_NET_SWITCH_SIM_CFUN0_FMT, SettingsNetwork_SimSlotName(target_slot));
    SettingsNetwork_OpenOverlay(buf);
    auto* ctx = new SimSwitchCtx{target_slot};
    if (xTaskCreate(SettingsNetwork_SimSwitchTask, "sim_switch", 4096, ctx, 5, nullptr) != pdPASS) {
        delete ctx;
        SettingsNetwork_sim_switch_pending = false;
        SettingsNetwork_CloseOverlay();
        if (SettingsNetwork_ui.sim_cur_lbl != nullptr) {
            lv_label_set_text(SettingsNetwork_ui.sim_cur_lbl, Lang::Strings::SETTINGS_NET_SIM_TASK_FAIL);
        }
    }
}

void SettingsNetwork_OnSimExtClicked(lv_event_t* /*e*/) { SettingsNetwork_ScheduleSimSwitch(kSimSlotExternal); }
void SettingsNetwork_OnSimIntClicked(lv_event_t* /*e*/) { SettingsNetwork_ScheduleSimSwitch(kSimSlotInternal); }

void SettingsNetwork_OnEnterWifiConfigClicked(lv_event_t* /*e*/) {
    if (SettingsNetwork_IsCellNetwork()) {
        return;
    }
    ESP_LOGI(TAG, "open NetworkScreen for WiFi setup");
    ScreenNavigateTo(NetworkScreen::Create);
}

void SettingsNetwork_AsyncRefreshSimUi(void* /*user_data*/) {
    if (SettingsNetwork_tab_alive) {
        SettingsNetwork_RefreshSimUi();
    }
}

void SettingsNetwork_ScheduleLoadSimSlotFromNvs() {
    Application::GetInstance().Schedule([]() {
        SettingsNetworkTab_LoadSimSlotFromNvsSync();
        ESP_LOGD(TAG, "sim_slot loaded from NVS: %d", SettingsNetwork_GetCachedSimSlot());
        // Application 任务非 LVGL：须 ScreenLvAsync，否则与删屏竞态会搞坏 async 链表
        ScreenLvAsync(SettingsNetwork_AsyncRefreshSimUi);
    });
}

