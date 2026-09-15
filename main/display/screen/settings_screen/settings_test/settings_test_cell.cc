#include "settings_test_cell.h"

#include "settings_test_common.h"

#include "assets/lang_config.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace settings_test_cell_detail {

constexpr const char* kTag = "SettingsTestCell";

// 手册：AT+ECPING="url",number,size,delay(ms)
constexpr int kPingCount = 3;
constexpr int kPingSize = 32;
constexpr int kPingDelayMs = 15000;
constexpr uint32_t kEcpingTimeoutMs =
    static_cast<uint32_t>(kPingCount) * kPingDelayMs + 20000;
constexpr uint32_t kSwitchRegWaitMs = 30000;
constexpr uint32_t kAtShortMs = 3000;
constexpr uint32_t kAtMediumMs = 8000;
constexpr uint32_t kAtLongMs = 12000;
constexpr uint32_t kAtProbeMs = 800;
constexpr uint32_t kPostCfunSettleMs = 40000;
constexpr uint32_t kModemResetSettleMs = 1500;
constexpr int kAtProbeRetries = 5;
constexpr int kAtProbeAfterResetRetries = 12;
constexpr int kAtDefaultRetries = 3;

constexpr uint32_t kCellTaskStack = 8192;
constexpr int kCellCreateMaxRetry = 5;
constexpr uint32_t kCellCreateRetryMs = 1200;

int s_pending_cell_slot = -1;
int s_cell_create_retry = 0;

struct SettingsTestCellDoneMsg {
    int slot = 0;
    bool pass = false;
    char detail[64] = {};
};


void StartCellTest(int slot);
void StopCellStartTimer();

bool HasAtError(const std::string& resp) {
    if (resp.find("+CME ERROR") != std::string::npos ||
        resp.find("+CMS ERROR") != std::string::npos) {
        return true;
    }
    if (resp.find("ERROR") != std::string::npos && resp.find("OK") == std::string::npos) {
        return true;
    }
    return false;
}

bool RunAtOk(Nt26Board* nt26, const std::string& cmd, std::string& resp, uint32_t timeout_ms) {
    resp.clear();
    const esp_err_t err = nt26->SendAtCommand(cmd, resp, timeout_ms, true);
    if (err != ESP_OK) {
        return false;
    }
    return resp.find("OK") != std::string::npos && !HasAtError(resp);
}

bool RunAtOkRetry(Nt26Board* nt26, const std::string& cmd, std::string& resp, uint32_t timeout_ms,
                  int max_retries) {
    for (int i = 0; i < max_retries; ++i) {
        if (!SettingsTest_IsLive()) {
            return false;
        }
        if (RunAtOk(nt26, cmd, resp, timeout_ms)) {
            return true;
        }
        if (i + 1 < max_retries) {
            vTaskDelay(pdMS_TO_TICKS(150));
        }
    }
    return false;
}

bool ProbeModemAt(Nt26Board* nt26, std::string& resp, uint32_t timeout_ms, esp_err_t* out_err) {
    resp.clear();
    const esp_err_t err = nt26->SendAtCommand("AT", resp, timeout_ms, true);
    if (out_err != nullptr) {
        *out_err = err;
    }
    return err == ESP_OK && resp.find("OK") != std::string::npos && !HasAtError(resp);
}

bool TryAtProbeBurst(Nt26Board* nt26, int attempts, uint32_t timeout_ms, uint32_t gap_ms,
                     esp_err_t* out_err) {
    std::string resp;
    for (int i = 0; i < attempts; ++i) {
        if (!SettingsTest_IsLive()) {
            return false;
        }
        if (ProbeModemAt(nt26, resp, timeout_ms, out_err)) {
            return true;
        }
        if (i + 1 < attempts) {
            vTaskDelay(pdMS_TO_TICKS(gap_ms));
        }
    }
    return false;
}

void RunAt(Nt26Board* nt26, const std::string& cmd, std::string& resp, uint32_t timeout_ms) {
    resp.clear();
    nt26->SendAtCommand(cmd, resp, timeout_ms, true);
}

void SoftResetModem(Nt26Board* nt26) {
    std::string resp;
    RunAt(nt26, "AT+ECRST", resp, 500);
    vTaskDelay(pdMS_TO_TICKS(kModemResetSettleMs));
}

bool EnsureModemAlive(Nt26Board* nt26, char* detail, size_t detail_len) {
    esp_err_t last_err = ESP_OK;

    if (TryAtProbeBurst(nt26, kAtProbeRetries, kAtProbeMs, 200, &last_err)) {
        return true;
    }

    ESP_LOGW(kTag, "AT probe failed err=%s, try ECRST",
             esp_err_to_name(last_err));
    SoftResetModem(nt26);
    if (TryAtProbeBurst(nt26, kAtProbeAfterResetRetries, 500, 150, &last_err)) {
        ESP_LOGI(kTag, "modem responsive after ECRST");
        return true;
    }

    std::string resp;
    RunAtOkRetry(nt26, "AT+CFUN=1", resp, kAtLongMs, 2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (TryAtProbeBurst(nt26, 4, kAtShortMs, 300, &last_err)) {
        ESP_LOGI(kTag, "modem responsive after CFUN=1");
        return true;
    }

    if (last_err == ESP_ERR_INVALID_STATE) {
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_MODEM_NOT_READY);
    } else if (last_err == ESP_ERR_TIMEOUT) {
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_MODEM_NO_RESP);
    } else {
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_MODEM_NO_RESP);
    }
    ESP_LOGE(kTag, "modem not alive after recovery err=%s", esp_err_to_name(last_err));
    return false;
}

bool IsSimAbsent(const std::string& resp) {
    return resp.find("+CME ERROR: 10") != std::string::npos ||
           resp.find("+CME ERROR: 13") != std::string::npos ||
           resp.find("+CME ERROR: 14") != std::string::npos;
}

int ParseSimSlotFromEcsimcfg(const std::string& resp) {
    constexpr const char* kKey = "\"SimSlot\"";
    size_t pos = 0;
    while ((pos = resp.find(kKey, pos)) != std::string::npos) {
        const size_t comma = resp.find(',', pos);
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

int QueryCurrentSimSlot(Nt26Board* nt26) {
    std::string resp;
    if (!RunAtOkRetry(nt26, "AT+ECSIMCFG?", resp, kAtShortMs, kAtDefaultRetries)) {
        return -1;
    }
    return ParseSimSlotFromEcsimcfg(resp);
}

int ParseCeregStat(const std::string& resp) {
    const size_t pos = resp.find("+CEREG:");
    if (pos == std::string::npos) {
        return -1;
    }
    int n = 0;
    int stat = 0;
    const char* p = resp.c_str() + pos;
    if (std::sscanf(p, "+CEREG: %d,%d", &n, &stat) >= 2 && n <= 3) {
        return stat;
    }
    if (std::sscanf(p, "+CEREG: %d", &stat) == 1) {
        return stat;
    }
    return -1;
}

bool IsSimReady(const std::string& resp) {
    return resp.find("+CPIN: READY") != std::string::npos;
}

bool HasIpv4Address(const std::string& resp) {
    if (resp.find("+CGPADDR:") == std::string::npos) {
        return false;
    }
    int a = 0, b = 0, c = 0, d = 0;
    return std::sscanf(resp.c_str(), "+CGPADDR: %*d,\"%d.%d.%d.%d", &a, &b, &c, &d) == 4 ||
           std::sscanf(resp.c_str(), "\r\n+CGPADDR: %*d,\"%d.%d.%d.%d", &a, &b, &c, &d) == 4;
}

void MarkModemRadioReset() {
    SettingsTest_Ui().last_cfun_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void ClearModemRadioReset() {
    SettingsTest_Ui().last_cfun_ms = 0;
}

bool NeedsRegistrationWait(int target_slot, int current_slot) {
    if (current_slot != target_slot) {
        return true;
    }
    if (SettingsTest_Ui().last_cfun_ms == 0) {
        return false;
    }
    const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return (now - SettingsTest_Ui().last_cfun_ms) < kPostCfunSettleMs;
}

bool WaitSimRegisteredForPing(Nt26Board* nt26, char* detail, size_t detail_len,
                              uint32_t max_wait_ms) {
    const uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    std::string resp;
    RunAtOk(nt26, "AT+CEREG=2", resp, kAtShortMs);

    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start) < max_wait_ms) {
        if (!SettingsTest_IsLive()) {
            std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_CANCELLED);
            return false;
        }
        RunAt(nt26, "AT+CPIN?", resp, kAtShortMs);
        if (IsSimAbsent(resp)) {
            std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_NO_SIM);
            return false;
        }
        if (!IsSimReady(resp)) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (RunAtOk(nt26, "AT+CEREG?", resp, kAtShortMs)) {
            const int stat = ParseCeregStat(resp);
            if (stat == 3) {
                std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_REG_REJECT);
                return false;
            }
            if (stat == 1 || stat == 5) {
                return true;
            }
        }
        if (RunAtOk(nt26, "AT+CGPADDR=1", resp, kAtShortMs) && HasIpv4Address(resp)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_SEARCH_TIMEOUT);
    return false;
}

bool ApplySimSlot(Nt26Board* nt26, int target_slot, char* detail, size_t detail_len) {
    std::string resp;
    if (!RunAtOkRetry(nt26, "AT+CFUN=0", resp, kAtMediumMs, kAtDefaultRetries)) {
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_CFUN0_FAIL);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    char buf[64];
    std::snprintf(buf, sizeof(buf), "AT+ECSIMCFG=SimSlot,%d", target_slot);
    if (!RunAtOkRetry(nt26, buf, resp, kAtShortMs, kAtDefaultRetries)) {
        RunAtOkRetry(nt26, "AT+CFUN=1", resp, kAtLongMs, 2);
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_SIM_SWITCH_FAIL);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    if (!RunAtOkRetry(nt26, "AT+CFUN=1", resp, kAtLongMs, kAtDefaultRetries)) {
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_CFUN1_FAIL);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (QueryCurrentSimSlot(nt26) != target_slot) {
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_SLOT_FAIL);
        return false;
    }
    MarkModemRadioReset();
    return true;
}

void RestoreSimSlotQuiet(Nt26Board* nt26, int target_slot) {
    char detail[64];
    ApplySimSlot(nt26, target_slot, detail, sizeof(detail));
}

bool ParseEcpingSummary(const std::string& resp, int* tx, int* rx) {
    size_t pos = 0;
    while ((pos = resp.find("+ECPING:", pos)) != std::string::npos) {
        const char* p = resp.c_str() + pos;
        int local_tx = 0;
        int local_rx = 0;
        if (std::sscanf(p, "+ECPING: dest: %*[^,], %d packets transmitted, %d received",
                        &local_tx, &local_rx) == 2 ||
            std::sscanf(p, "+ECPING: dest: %*[^,], %d packets transmittted, %d received",
                        &local_tx, &local_rx) == 2) {
            *tx = local_tx;
            *rx = local_rx;
            return true;
        }
        pos += 8;
    }
    return false;
}

bool IsEcpingSuccess(const std::string& resp) {
    if (HasAtError(resp)) {
        return false;
    }
    if (resp.find("+ECPING: FAIL") != std::string::npos ||
        resp.find("+ECPING: TIMEOUT") != std::string::npos) {
        return false;
    }
    if (resp.find("+ECPING: SUCC") != std::string::npos ||
        resp.find("+ECPING: DONE") != std::string::npos) {
        return true;
    }
    if (resp.find("0% packet loss") != std::string::npos ||
        resp.find("0 % packet loss") != std::string::npos) {
        return true;
    }
    int tx = 0;
    int rx = 0;
    if (ParseEcpingSummary(resp, &tx, &rx)) {
        return rx > 0 && (tx <= 0 || rx >= tx);
    }
    return false;
}

bool RunEcpingTest(Nt26Board* nt26, char* detail, size_t detail_len) {
    char cmd[96];
    std::snprintf(cmd, sizeof(cmd), "AT+ECPING=\"www.baidu.com\",%d,%d,%d", kPingCount, kPingSize,
                  kPingDelayMs);
    std::string resp;

    auto run_once = [&]() -> bool {
        resp.clear();
        const esp_err_t err =
            nt26->SendAtCommandCollectUntil(cmd, resp, kEcpingTimeoutMs, "+ECPING: DONE", true);
        ESP_LOGI(kTag, "ECPING err=%d len=%u", static_cast<int>(err),
                 static_cast<unsigned>(resp.size()));
        if (IsEcpingSuccess(resp)) {
            return true;
        }
        if (HasAtError(resp)) {
            std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_PING_CMD_FAIL);
        } else if (err == ESP_ERR_TIMEOUT) {
            std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_PING_WAIT_TIMEOUT);
        } else {
            int tx = 0;
            int rx = 0;
            if (ParseEcpingSummary(resp, &tx, &rx)) {
                std::snprintf(detail, detail_len, Lang::Strings::SETTINGS_TEST_CELL_LOSS_FMT, rx, tx);
            } else {
                std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_PING_FAIL);
            }
        }
        return false;
    };

    if (run_once()) {
        return true;
    }

    // PING 超时/失败后再探活并重试一次（避免偶发 AT 通道未唤醒）
    char alive_detail[32];
    if (!EnsureModemAlive(nt26, alive_detail, sizeof(alive_detail))) {
        return false;
    }
    ESP_LOGW(kTag, "ECPING retry after modem recovery");
    return run_once();
}

SettingsTestRow* RowForSlot(int slot) {
    return slot == kSettingsTestSimInternal ? &SettingsTest_Ui().sim_int : &SettingsTest_Ui().sim_ext;
}

void AsyncCellDone(void* p) {
    auto* msg = static_cast<SettingsTestCellDoneMsg*>(p);
    if (!SettingsTest_IsLive()) {
        delete msg;
        return;
    }
    SettingsTest_Ui().cell_state = SettingsTestCellState::Idle;
    SettingsTestRow* row = RowForSlot(msg->slot);
    if (row != nullptr) {
        SettingsTest_SetRowValue(*row, msg->detail, !msg->pass);
        SettingsTest_SetRowStatus(*row, msg->pass);
    }
    if (SettingsTest_Ui().cell_auto && msg->slot == kSettingsTestSimInternal) {
        StartCellTest(kSettingsTestSimExternal);
    } else if (SettingsTest_Ui().cell_auto && msg->slot == kSettingsTestSimExternal) {
        SettingsTest_Ui().cell_auto = false;
    }
    delete msg;
}

void PostCellDone(SettingsTestCellDoneMsg* msg) {
    if (!SettingsTest_PostLvAsync(AsyncCellDone, msg)) {
        delete msg;
    }
}

void CellTestTask(void* arg) {
    SettingsTest_Ui().cell_busy = true;
    const int target_slot = static_cast<int>(reinterpret_cast<intptr_t>(arg));
    auto* msg = new SettingsTestCellDoneMsg{};
    msg->slot = target_slot;
    msg->pass = false;
    std::snprintf(msg->detail, sizeof(msg->detail), "%s", Lang::Strings::SETTINGS_TEST_CELL_TEST_FAIL);

    Nt26Board* nt26 = SettingsTest_GetNt26Board();
    if (nt26 == nullptr) {
        std::snprintf(msg->detail, sizeof(msg->detail), "%s", Lang::Strings::SETTINGS_TEST_CELL_NEED_4G);
        PostCellDone(msg);
        SettingsTest_Ui().cell_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    char alive_detail[64];
    if (!EnsureModemAlive(nt26, alive_detail, sizeof(alive_detail))) {
        std::snprintf(msg->detail, sizeof(msg->detail), "%s", alive_detail);
        PostCellDone(msg);
        SettingsTest_Ui().cell_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    std::string resp;
    (void)resp;

    const int original_slot = QueryCurrentSimSlot(nt26);
    bool switched = false;
    const int current_slot =
        (original_slot == kSettingsTestSimExternal || original_slot == kSettingsTestSimInternal) ? original_slot
                                                                                 : -1;

    if (current_slot != target_slot) {
        if (!ApplySimSlot(nt26, target_slot, msg->detail, sizeof(msg->detail))) {
            PostCellDone(msg);
            SettingsTest_Ui().cell_busy = false;
            vTaskDelete(nullptr);
            return;
        }
        switched = true;
    }

    if (NeedsRegistrationWait(target_slot, current_slot) || switched) {
        char wait_detail[64];
        if (!WaitSimRegisteredForPing(nt26, wait_detail, sizeof(wait_detail), kSwitchRegWaitMs)) {
            if (std::strcmp(wait_detail, Lang::Strings::SETTINGS_TEST_CELL_NO_SIM) == 0) {
                std::snprintf(msg->detail, sizeof(msg->detail), "%s", wait_detail);
                if (switched && current_slot >= 0) {
                    RestoreSimSlotQuiet(nt26, current_slot);
                }
                PostCellDone(msg);
                SettingsTest_Ui().cell_busy = false;
                vTaskDelete(nullptr);
                return;
            }
            ESP_LOGW(kTag, "reg wait: %s, try ping anyway", wait_detail);
        }
    }

    if (RunEcpingTest(nt26, msg->detail, sizeof(msg->detail))) {
        msg->pass = true;
        std::snprintf(msg->detail, sizeof(msg->detail), "%s", Lang::Strings::SETTINGS_TEST_CELL_PING_OK);
        ClearModemRadioReset();
    }

    if (switched && current_slot >= 0 && current_slot != target_slot) {
        RestoreSimSlotQuiet(nt26, current_slot);
    }

    if (!SettingsTest_IsLive()) {
        delete msg;
        SettingsTest_Ui().cell_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    PostCellDone(msg);
    SettingsTest_Ui().cell_busy = false;
    vTaskDelete(nullptr);
}

void StopCellRetryTimerInternal() {
    if (SettingsTest_Ui().cell_retry_timer != nullptr) {
        lv_timer_delete(SettingsTest_Ui().cell_retry_timer);
        SettingsTest_Ui().cell_retry_timer = nullptr;
    }
    s_pending_cell_slot = -1;
    s_cell_create_retry = 0;
}

void OnCellRetryTimer(lv_timer_t* /*t*/) {
    SettingsTest_Ui().cell_retry_timer = nullptr;
    if (!SettingsTest_IsLive() || s_pending_cell_slot < 0) {
        s_pending_cell_slot = -1;
        s_cell_create_retry = 0;
        return;
    }
    const int slot = s_pending_cell_slot;
    s_pending_cell_slot = -1;
    StartCellTest(slot);
}

void ScheduleCellRetry(int slot) {
    if (!SettingsTest_IsLive()) {
        return;
    }
    if (s_cell_create_retry >= kCellCreateMaxRetry) {
        s_cell_create_retry = 0;
        SettingsTestRow* row = RowForSlot(slot);
        if (row != nullptr) {
            SettingsTest_SetRowValue(*row, Lang::Strings::SETTINGS_TEST_CELL_NO_RESOURCE, true);
            SettingsTest_SetRowStatus(*row, false);
        }
        if (SettingsTest_Ui().cell_auto && slot == kSettingsTestSimInternal) {
            StartCellTest(kSettingsTestSimExternal);
        } else {
            SettingsTest_Ui().cell_auto = false;
        }
        return;
    }

    ++s_cell_create_retry;
    s_pending_cell_slot = slot;
    if (SettingsTest_Ui().cell_retry_timer != nullptr) {
        lv_timer_delete(SettingsTest_Ui().cell_retry_timer);
        SettingsTest_Ui().cell_retry_timer = nullptr;
    }
    SettingsTestRow* row = RowForSlot(slot);
    if (row != nullptr) {
        SettingsTest_SetRowValue(*row, Lang::Strings::SETTINGS_TEST_CELL_WAIT_RESOURCE, false);
    }
    SettingsTest_Ui().cell_retry_timer =
        lv_timer_create(OnCellRetryTimer, kCellCreateRetryMs, nullptr);
    lv_timer_set_repeat_count(SettingsTest_Ui().cell_retry_timer, 1);
}

void StartCellTest(int slot) {
    if (SettingsTest_Ui().cell_state != SettingsTestCellState::Idle || SettingsTest_Ui().cell_busy) {
        return;
    }
    SettingsTest_Ui().cell_state =
        slot == kSettingsTestSimInternal ? SettingsTestCellState::TestingInternal
                                         : SettingsTestCellState::TestingExternal;
    SettingsTestRow* row = RowForSlot(slot);
    if (row != nullptr) {
        SettingsTest_SetRowValue(*row, Lang::Strings::SETTINGS_TEST_CELL_TESTING, false);
    }
    if (xTaskCreate(CellTestTask, "set_cell_ping", kCellTaskStack,
                    reinterpret_cast<void*>(static_cast<intptr_t>(slot)), 5, nullptr) != pdPASS) {
        ESP_LOGE(kTag, "create cell task failed slot=%d heap=%u largest=%u", slot,
                 static_cast<unsigned>(esp_get_free_heap_size()),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)));
        SettingsTest_Ui().cell_state = SettingsTestCellState::Idle;
        ScheduleCellRetry(slot);
        return;
    }
    s_cell_create_retry = 0;
    s_pending_cell_slot = -1;
}

uint32_t CellWaitElapsedMs(uint32_t start_ms) {
    const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return now - start_ms;
}

void OnCellAutoStartPoll(lv_timer_t* t) {
    if (!SettingsTest_IsLive()) {
        StopCellStartTimer();
        return;
    }

    const uint32_t start_ms = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_timer_get_user_data(t)));
    if (SettingsTest_Ui().wifi_scan_busy &&
        CellWaitElapsedMs(start_ms) < kSettingsTestCellWaitMaxMs) {
        return;
    }

    StopCellStartTimer();
    if (!SettingsTest_IsCellNetwork()) {
        SettingsTest_SetRowValue(SettingsTest_Ui().sim_int, Lang::Strings::SETTINGS_TEST_CELL_NEED_4G, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().sim_int, false);
        SettingsTest_SetRowValue(SettingsTest_Ui().sim_ext, Lang::Strings::SETTINGS_TEST_CELL_NEED_4G, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().sim_ext, false);
        return;
    }
    SettingsTest_Ui().cell_auto = true;
    StartCellTest(kSettingsTestSimInternal);
}

void StopCellStartTimer() {
    if (SettingsTest_Ui().cell_start_timer != nullptr) {
        lv_timer_delete(SettingsTest_Ui().cell_start_timer);
        SettingsTest_Ui().cell_start_timer = nullptr;
    }
}

}  // namespace settings_test_cell_detail

void SettingsTestCell_StopStartTimer() {
    settings_test_cell_detail::StopCellStartTimer();
}

void SettingsTestCell_StopRetryTimer() {
    settings_test_cell_detail::StopCellRetryTimerInternal();
}

void SettingsTestCell_StartAutoSequence() {
    SettingsTestCell_StopStartTimer();
    SettingsTestCell_StopRetryTimer();
    const uint32_t start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    SettingsTest_Ui().cell_start_timer = lv_timer_create(settings_test_cell_detail::OnCellAutoStartPoll,
                                                         kSettingsTestCellWaitPollMs, nullptr);
    lv_timer_set_repeat_count(SettingsTest_Ui().cell_start_timer, -1);
    lv_timer_set_user_data(SettingsTest_Ui().cell_start_timer,
                           reinterpret_cast<void*>(static_cast<uintptr_t>(start_ms)));
}
