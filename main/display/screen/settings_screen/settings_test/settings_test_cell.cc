#include "settings_test_cell_priv.h"
#include "settings_test_cell_ui.h"
#include "settings_test_cell_at.h"
#include "settings_test_cell.h"

#include "assets/lang_config.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void SettingsTestCell_AsyncCellDone(void* p) {
    auto* msg = static_cast<SettingsTestCellDoneMsg*>(p);
    if (!SettingsTest_IsLive()) {
        delete msg;
        return;
    }
    SettingsTest_Ui().cell_state = SettingsTestCellState::Idle;
    SettingsTestRow* row = SettingsTestCell_RowForSlot(msg->slot);
    if (row != nullptr) {
        SettingsTest_SetRowValue(*row, msg->detail, !msg->pass);
        SettingsTest_SetRowStatus(*row, msg->pass);
    }
    if (SettingsTest_Ui().cell_auto && msg->slot == kSettingsTestSimInternal) {
        SettingsTestCell_StartCellTest(kSettingsTestSimExternal);
    } else if (SettingsTest_Ui().cell_auto && msg->slot == kSettingsTestSimExternal) {
        SettingsTest_Ui().cell_auto = false;
    }
    delete msg;
}

void SettingsTestCell_PostCellDone(SettingsTestCellDoneMsg* msg) {
    if (!SettingsTest_PostLvAsync(SettingsTestCell_AsyncCellDone, msg)) {
        delete msg;
    }
}

void SettingsTestCell_CellTestTask(void* arg) {
    SettingsTest_Ui().cell_busy = true;
    const int target_slot = static_cast<int>(reinterpret_cast<intptr_t>(arg));
    auto* msg = new SettingsTestCellDoneMsg{};
    msg->slot = target_slot;
    msg->pass = false;
    std::snprintf(msg->detail, sizeof(msg->detail), "%s", Lang::Strings::SETTINGS_TEST_CELL_TEST_FAIL);

    Nt26Board* nt26 = SettingsTest_GetNt26Board();
    if (nt26 == nullptr) {
        std::snprintf(msg->detail, sizeof(msg->detail), "%s", Lang::Strings::SETTINGS_TEST_CELL_NEED_4G);
        SettingsTestCell_PostCellDone(msg);
        SettingsTest_Ui().cell_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    char alive_detail[64];
    if (!SettingsTestCell_EnsureModemAlive(nt26, alive_detail, sizeof(alive_detail))) {
        std::snprintf(msg->detail, sizeof(msg->detail), "%s", alive_detail);
        SettingsTestCell_PostCellDone(msg);
        SettingsTest_Ui().cell_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    std::string resp;
    (void)resp;

    const int original_slot = SettingsTestCell_QueryCurrentSimSlot(nt26);
    bool switched = false;
    const int current_slot =
        (original_slot == kSettingsTestSimExternal || original_slot == kSettingsTestSimInternal) ? original_slot
                                                                                 : -1;

    if (current_slot != target_slot) {
        if (!SettingsTestCell_ApplySimSlot(nt26, target_slot, msg->detail, sizeof(msg->detail))) {
            SettingsTestCell_PostCellDone(msg);
            SettingsTest_Ui().cell_busy = false;
            vTaskDelete(nullptr);
            return;
        }
        switched = true;
    }

    if (SettingsTestCell_NeedsRegistrationWait(target_slot, current_slot) || switched) {
        char wait_detail[64];
        if (!SettingsTestCell_WaitSimRegisteredForPing(nt26, wait_detail, sizeof(wait_detail), kSwitchRegWaitMs)) {
            if (std::strcmp(wait_detail, Lang::Strings::SETTINGS_TEST_CELL_NO_SIM) == 0) {
                std::snprintf(msg->detail, sizeof(msg->detail), "%s", wait_detail);
                if (switched && current_slot >= 0) {
                    SettingsTestCell_RestoreSimSlotQuiet(nt26, current_slot);
                }
                SettingsTestCell_PostCellDone(msg);
                SettingsTest_Ui().cell_busy = false;
                vTaskDelete(nullptr);
                return;
            }
            ESP_LOGW(kTag, "reg wait: %s, try ping anyway", wait_detail);
        }
    }

    if (SettingsTestCell_RunEcpingTest(nt26, msg->detail, sizeof(msg->detail))) {
        msg->pass = true;
        std::snprintf(msg->detail, sizeof(msg->detail), "%s", Lang::Strings::SETTINGS_TEST_CELL_PING_OK);
        SettingsTestCell_ClearModemRadioReset();
    }

    if (switched && current_slot >= 0 && current_slot != target_slot) {
        SettingsTestCell_RestoreSimSlotQuiet(nt26, current_slot);
    }

    if (!SettingsTest_IsLive()) {
        delete msg;
        SettingsTest_Ui().cell_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    SettingsTestCell_PostCellDone(msg);
    SettingsTest_Ui().cell_busy = false;
    vTaskDelete(nullptr);
}

void SettingsTestCell_StopCellRetryTimerInternal() {
    if (SettingsTest_Ui().cell_retry_timer != nullptr) {
        lv_timer_delete(SettingsTest_Ui().cell_retry_timer);
        SettingsTest_Ui().cell_retry_timer = nullptr;
    }
    SettingsTestCell_pending_slot = -1;
    SettingsTestCell_create_retry = 0;
}

void SettingsTestCell_OnCellRetryTimer(lv_timer_t* /*t*/) {
    SettingsTest_Ui().cell_retry_timer = nullptr;
    if (!SettingsTest_IsLive() || SettingsTestCell_pending_slot < 0) {
        SettingsTestCell_pending_slot = -1;
        SettingsTestCell_create_retry = 0;
        return;
    }
    const int slot = SettingsTestCell_pending_slot;
    SettingsTestCell_pending_slot = -1;
    SettingsTestCell_StartCellTest(slot);
}

void SettingsTestCell_ScheduleCellRetry(int slot) {
    if (!SettingsTest_IsLive()) {
        return;
    }
    if (SettingsTestCell_create_retry >= kCellCreateMaxRetry) {
        SettingsTestCell_create_retry = 0;
        SettingsTestRow* row = SettingsTestCell_RowForSlot(slot);
        if (row != nullptr) {
            SettingsTest_SetRowValue(*row, Lang::Strings::SETTINGS_TEST_CELL_NO_RESOURCE, true);
            SettingsTest_SetRowStatus(*row, false);
        }
        if (SettingsTest_Ui().cell_auto && slot == kSettingsTestSimInternal) {
            SettingsTestCell_StartCellTest(kSettingsTestSimExternal);
        } else {
            SettingsTest_Ui().cell_auto = false;
        }
        return;
    }

    ++SettingsTestCell_create_retry;
    SettingsTestCell_pending_slot = slot;
    if (SettingsTest_Ui().cell_retry_timer != nullptr) {
        lv_timer_delete(SettingsTest_Ui().cell_retry_timer);
        SettingsTest_Ui().cell_retry_timer = nullptr;
    }
    SettingsTestRow* row = SettingsTestCell_RowForSlot(slot);
    if (row != nullptr) {
        SettingsTest_SetRowValue(*row, Lang::Strings::SETTINGS_TEST_CELL_WAIT_RESOURCE, false);
    }
    SettingsTest_Ui().cell_retry_timer =
        lv_timer_create(SettingsTestCell_OnCellRetryTimer, kCellCreateRetryMs, nullptr);
    lv_timer_set_repeat_count(SettingsTest_Ui().cell_retry_timer, 1);
}

void SettingsTestCell_StartCellTest(int slot) {
    if (SettingsTest_Ui().cell_state != SettingsTestCellState::Idle || SettingsTest_Ui().cell_busy) {
        return;
    }
    SettingsTest_Ui().cell_state =
        slot == kSettingsTestSimInternal ? SettingsTestCellState::TestingInternal
                                         : SettingsTestCellState::TestingExternal;
    SettingsTestRow* row = SettingsTestCell_RowForSlot(slot);
    if (row != nullptr) {
        SettingsTest_SetRowValue(*row, Lang::Strings::SETTINGS_TEST_CELL_TESTING, false);
    }
    if (xTaskCreate(SettingsTestCell_CellTestTask, "set_cell_ping", kCellTaskStack,
                    reinterpret_cast<void*>(static_cast<intptr_t>(slot)), 5, nullptr) != pdPASS) {
        ESP_LOGE(kTag, "create cell task failed slot=%d heap=%u largest=%u", slot,
                 static_cast<unsigned>(esp_get_free_heap_size()),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)));
        SettingsTest_Ui().cell_state = SettingsTestCellState::Idle;
        SettingsTestCell_ScheduleCellRetry(slot);
        return;
    }
    SettingsTestCell_create_retry = 0;
    SettingsTestCell_pending_slot = -1;
}

static uint32_t CellWaitElapsedMs(uint32_t start_ms) {
    const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return now - start_ms;
}

void SettingsTestCell_OnCellAutoStartPoll(lv_timer_t* t) {
    if (!SettingsTest_IsLive()) {
        SettingsTestCell_StopCellStartTimer();
        return;
    }

    const uint32_t start_ms = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_timer_get_user_data(t)));
    if (SettingsTest_Ui().wifi_scan_busy &&
        CellWaitElapsedMs(start_ms) < kSettingsTestCellWaitMaxMs) {
        return;
    }

    SettingsTestCell_StopCellStartTimer();
    if (!SettingsTest_IsCellNetwork()) {
        SettingsTest_SetRowValue(SettingsTest_Ui().sim_int, Lang::Strings::SETTINGS_TEST_CELL_NEED_4G, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().sim_int, false);
        SettingsTest_SetRowValue(SettingsTest_Ui().sim_ext, Lang::Strings::SETTINGS_TEST_CELL_NEED_4G, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().sim_ext, false);
        return;
    }
    SettingsTest_Ui().cell_auto = true;
    SettingsTestCell_StartCellTest(kSettingsTestSimInternal);
}

void SettingsTestCell_StopCellStartTimer() {
    if (SettingsTest_Ui().cell_start_timer != nullptr) {
        lv_timer_delete(SettingsTest_Ui().cell_start_timer);
        SettingsTest_Ui().cell_start_timer = nullptr;
    }
}

void SettingsTestCell_StopStartTimer() {
    SettingsTestCell_StopCellStartTimer();
}

void SettingsTestCell_StopRetryTimer() {
    SettingsTestCell_StopCellRetryTimerInternal();
}

void SettingsTestCell_StartAutoSequence() {
    SettingsTestCell_StopStartTimer();
    SettingsTestCell_StopRetryTimer();
    const uint32_t start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    SettingsTest_Ui().cell_start_timer = lv_timer_create(SettingsTestCell_OnCellAutoStartPoll,
                                                         kSettingsTestCellWaitPollMs, nullptr);
    lv_timer_set_repeat_count(SettingsTest_Ui().cell_start_timer, -1);
    lv_timer_set_user_data(SettingsTest_Ui().cell_start_timer,
                           reinterpret_cast<void*>(static_cast<uintptr_t>(start_ms)));
}
