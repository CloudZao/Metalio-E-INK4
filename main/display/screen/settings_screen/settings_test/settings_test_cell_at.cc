#include "settings_test_cell_priv.h"
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

int SettingsTestCell_pending_slot = -1;
int SettingsTestCell_create_retry = 0;

bool SettingsTestCell_HasAtError(const std::string& resp) {
    if (resp.find("+CME ERROR") != std::string::npos ||
        resp.find("+CMS ERROR") != std::string::npos) {
        return true;
    }
    if (resp.find("ERROR") != std::string::npos && resp.find("OK") == std::string::npos) {
        return true;
    }
    return false;
}

bool SettingsTestCell_RunAtOk(Nt26Board* nt26, const std::string& cmd, std::string& resp, uint32_t timeout_ms) {
    resp.clear();
    const esp_err_t err = nt26->SendAtCommand(cmd, resp, timeout_ms, true);
    if (err != ESP_OK) {
        return false;
    }
    return resp.find("OK") != std::string::npos && !SettingsTestCell_HasAtError(resp);
}

bool SettingsTestCell_RunAtOkRetry(Nt26Board* nt26, const std::string& cmd, std::string& resp, uint32_t timeout_ms,
                  int max_retries) {
    for (int i = 0; i < max_retries; ++i) {
        if (!SettingsTest_IsLive()) {
            return false;
        }
        if (SettingsTestCell_RunAtOk(nt26, cmd, resp, timeout_ms)) {
            return true;
        }
        if (i + 1 < max_retries) {
            vTaskDelay(pdMS_TO_TICKS(150));
        }
    }
    return false;
}

bool SettingsTestCell_ProbeModemAt(Nt26Board* nt26, std::string& resp, uint32_t timeout_ms, esp_err_t* out_err) {
    resp.clear();
    const esp_err_t err = nt26->SendAtCommand("AT", resp, timeout_ms, true);
    if (out_err != nullptr) {
        *out_err = err;
    }
    return err == ESP_OK && resp.find("OK") != std::string::npos && !SettingsTestCell_HasAtError(resp);
}

bool SettingsTestCell_TryAtProbeBurst(Nt26Board* nt26, int attempts, uint32_t timeout_ms, uint32_t gap_ms,
                     esp_err_t* out_err) {
    std::string resp;
    for (int i = 0; i < attempts; ++i) {
        if (!SettingsTest_IsLive()) {
            return false;
        }
        if (SettingsTestCell_ProbeModemAt(nt26, resp, timeout_ms, out_err)) {
            return true;
        }
        if (i + 1 < attempts) {
            vTaskDelay(pdMS_TO_TICKS(gap_ms));
        }
    }
    return false;
}

void SettingsTestCell_RunAt(Nt26Board* nt26, const std::string& cmd, std::string& resp, uint32_t timeout_ms) {
    resp.clear();
    nt26->SendAtCommand(cmd, resp, timeout_ms, true);
}

void SettingsTestCell_SoftResetModem(Nt26Board* nt26) {
    std::string resp;
    SettingsTestCell_RunAt(nt26, "AT+ECRST", resp, 500);
    vTaskDelay(pdMS_TO_TICKS(kModemResetSettleMs));
}

bool SettingsTestCell_EnsureModemAlive(Nt26Board* nt26, char* detail, size_t detail_len) {
    esp_err_t last_err = ESP_OK;

    if (SettingsTestCell_TryAtProbeBurst(nt26, kAtProbeRetries, kAtProbeMs, 200, &last_err)) {
        return true;
    }

    ESP_LOGW(kTag, "AT probe failed err=%s, try ECRST",
             esp_err_to_name(last_err));
    SettingsTestCell_SoftResetModem(nt26);
    if (SettingsTestCell_TryAtProbeBurst(nt26, kAtProbeAfterResetRetries, 500, 150, &last_err)) {
        ESP_LOGI(kTag, "modem responsive after ECRST");
        return true;
    }

    std::string resp;
    SettingsTestCell_RunAtOkRetry(nt26, "AT+CFUN=1", resp, kAtLongMs, 2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (SettingsTestCell_TryAtProbeBurst(nt26, 4, kAtShortMs, 300, &last_err)) {
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

bool SettingsTestCell_IsSimAbsent(const std::string& resp) {
    return resp.find("+CME ERROR: 10") != std::string::npos ||
           resp.find("+CME ERROR: 13") != std::string::npos ||
           resp.find("+CME ERROR: 14") != std::string::npos;
}

int SettingsTestCell_ParseSimSlotFromEcsimcfg(const std::string& resp) {
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

int SettingsTestCell_QueryCurrentSimSlot(Nt26Board* nt26) {
    std::string resp;
    if (!SettingsTestCell_RunAtOkRetry(nt26, "AT+ECSIMCFG?", resp, kAtShortMs, kAtDefaultRetries)) {
        return -1;
    }
    return SettingsTestCell_ParseSimSlotFromEcsimcfg(resp);
}

int SettingsTestCell_ParseCeregStat(const std::string& resp) {
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

bool SettingsTestCell_IsSimReady(const std::string& resp) {
    return resp.find("+CPIN: READY") != std::string::npos;
}

bool SettingsTestCell_HasIpv4Address(const std::string& resp) {
    if (resp.find("+CGPADDR:") == std::string::npos) {
        return false;
    }
    int a = 0, b = 0, c = 0, d = 0;
    return std::sscanf(resp.c_str(), "+CGPADDR: %*d,\"%d.%d.%d.%d", &a, &b, &c, &d) == 4 ||
           std::sscanf(resp.c_str(), "\r\n+CGPADDR: %*d,\"%d.%d.%d.%d", &a, &b, &c, &d) == 4;
}

void SettingsTestCell_MarkModemRadioReset() {
    SettingsTest_Ui().last_cfun_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void SettingsTestCell_ClearModemRadioReset() {
    SettingsTest_Ui().last_cfun_ms = 0;
}

bool SettingsTestCell_NeedsRegistrationWait(int target_slot, int current_slot) {
    if (current_slot != target_slot) {
        return true;
    }
    if (SettingsTest_Ui().last_cfun_ms == 0) {
        return false;
    }
    const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return (now - SettingsTest_Ui().last_cfun_ms) < kPostCfunSettleMs;
}

bool SettingsTestCell_WaitSimRegisteredForPing(Nt26Board* nt26, char* detail, size_t detail_len,
                              uint32_t max_wait_ms) {
    const uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    std::string resp;
    SettingsTestCell_RunAtOk(nt26, "AT+CEREG=2", resp, kAtShortMs);

    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start) < max_wait_ms) {
        if (!SettingsTest_IsLive()) {
            std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_CANCELLED);
            return false;
        }
        SettingsTestCell_RunAt(nt26, "AT+CPIN?", resp, kAtShortMs);
        if (SettingsTestCell_IsSimAbsent(resp)) {
            std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_NO_SIM);
            return false;
        }
        if (!SettingsTestCell_IsSimReady(resp)) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (SettingsTestCell_RunAtOk(nt26, "AT+CEREG?", resp, kAtShortMs)) {
            const int stat = SettingsTestCell_ParseCeregStat(resp);
            if (stat == 3) {
                std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_REG_REJECT);
                return false;
            }
            if (stat == 1 || stat == 5) {
                return true;
            }
        }
        if (SettingsTestCell_RunAtOk(nt26, "AT+CGPADDR=1", resp, kAtShortMs) && SettingsTestCell_HasIpv4Address(resp)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_SEARCH_TIMEOUT);
    return false;
}

bool SettingsTestCell_ApplySimSlot(Nt26Board* nt26, int target_slot, char* detail, size_t detail_len) {
    std::string resp;
    if (!SettingsTestCell_RunAtOkRetry(nt26, "AT+CFUN=0", resp, kAtMediumMs, kAtDefaultRetries)) {
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_CFUN0_FAIL);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    char buf[64];
    std::snprintf(buf, sizeof(buf), "AT+ECSIMCFG=SimSlot,%d", target_slot);
    if (!SettingsTestCell_RunAtOkRetry(nt26, buf, resp, kAtShortMs, kAtDefaultRetries)) {
        SettingsTestCell_RunAtOkRetry(nt26, "AT+CFUN=1", resp, kAtLongMs, 2);
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_SIM_SWITCH_FAIL);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    if (!SettingsTestCell_RunAtOkRetry(nt26, "AT+CFUN=1", resp, kAtLongMs, kAtDefaultRetries)) {
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_CFUN1_FAIL);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (SettingsTestCell_QueryCurrentSimSlot(nt26) != target_slot) {
        std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_SLOT_FAIL);
        return false;
    }
    SettingsTestCell_MarkModemRadioReset();
    return true;
}

void SettingsTestCell_RestoreSimSlotQuiet(Nt26Board* nt26, int target_slot) {
    char detail[64];
    SettingsTestCell_ApplySimSlot(nt26, target_slot, detail, sizeof(detail));
}

bool SettingsTestCell_ParseEcpingSummary(const std::string& resp, int* tx, int* rx) {
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

bool SettingsTestCell_IsEcpingSuccess(const std::string& resp) {
    if (SettingsTestCell_HasAtError(resp)) {
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
    if (SettingsTestCell_ParseEcpingSummary(resp, &tx, &rx)) {
        return rx > 0 && (tx <= 0 || rx >= tx);
    }
    return false;
}

bool SettingsTestCell_RunEcpingTest(Nt26Board* nt26, char* detail, size_t detail_len) {
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
        if (SettingsTestCell_IsEcpingSuccess(resp)) {
            return true;
        }
        if (SettingsTestCell_HasAtError(resp)) {
            std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_PING_CMD_FAIL);
        } else if (err == ESP_ERR_TIMEOUT) {
            std::snprintf(detail, detail_len, "%s", Lang::Strings::SETTINGS_TEST_CELL_PING_WAIT_TIMEOUT);
        } else {
            int tx = 0;
            int rx = 0;
            if (SettingsTestCell_ParseEcpingSummary(resp, &tx, &rx)) {
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
    if (!SettingsTestCell_EnsureModemAlive(nt26, alive_detail, sizeof(alive_detail))) {
        return false;
    }
    ESP_LOGW(kTag, "ECPING retry after modem recovery");
    return run_once();
}

SettingsTestRow* SettingsTestCell_RowForSlot(int slot) {
    return slot == kSettingsTestSimInternal ? &SettingsTest_Ui().sim_int : &SettingsTest_Ui().sim_ext;
}

