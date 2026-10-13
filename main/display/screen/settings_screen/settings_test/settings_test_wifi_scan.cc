#include "settings_test_wifi_priv.h"
#include "settings_test_wifi_scan.h"
#include "settings_test_wifi_ui.h"
#include "settings_test_wifi.h"

#include "assets/lang_config.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "screen_common.h"
#include "settings_common.h"

#include <wifi_scan_lease.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

void SettingsTestWifi_AsyncWifiScanDone(void* p) {
    auto* msg = static_cast<SettingsTestScanDoneMsg*>(p);
    // 无论是否仍 live，都清 busy，避免离页/投递失败后卡死「扫描中」
    SettingsTest_Ui().wifi_scan_busy = false;
    if (!SettingsTest_IsLive()) {
        delete msg;
        return;
    }
    if (msg->success && msg->count > 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_TEST_WIFI_FOUND_FMT, msg->count);
        SettingsTest_SetRowValue(SettingsTest_Ui().wifi, buf, false);
        SettingsTest_SetRowStatus(SettingsTest_Ui().wifi, true);
    } else if (msg->success) {
        SettingsTest_SetRowValue(SettingsTest_Ui().wifi, Lang::Strings::SETTINGS_TEST_WIFI_NONE, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().wifi, false);
    } else {
        SettingsTest_SetRowValue(SettingsTest_Ui().wifi, Lang::Strings::SETTINGS_TEST_WIFI_SCAN_FAIL, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().wifi, false);
    }
    SettingsTestWifi_SetWifiRescanEnabled(true);
    SettingsTestWifi_RebuildWifiListPage();
    delete msg;
}

void SettingsTestWifi_PostWifiScanDone(bool success, int count) {
    auto* msg = new (std::nothrow) SettingsTestScanDoneMsg{success, count};
    if (msg == nullptr) {
        SettingsTest_Ui().wifi_scan_busy = false;
        return;
    }
    if (!SettingsTest_PostLvAsync(SettingsTestWifi_AsyncWifiScanDone, msg)) {
        SettingsTest_Ui().wifi_scan_busy = false;
        delete msg;
    }
}

void SettingsTestWifi_DestroyScanSession(ScanSession* session) {
    if (session == nullptr) {
        return;
    }
    if (session->handler != nullptr) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, session->handler);
        session->handler = nullptr;
    }
    if (session->evt != nullptr) {
        vEventGroupDelete(session->evt);
        session->evt = nullptr;
    }
    if (session->lease_held) {
        WifiScanLease::Release();
        session->lease_held = false;
    }
    session->records.clear();
    // StaticSemaphore 随 session 释放；勿 vSemaphoreDelete
}

void SettingsTestWifi_WifiScanTask(void* /*arg*/) {
    const uint32_t my_gen = SettingsTestWifi_scan_generation;
    SettingsTest_Ui().wifi_scan_busy = true;

    // 任务末尾 vTaskDelete 不跑 C++ 析构，会话必须堆分配并在删任务前 delete
    ScanSession* session = new (std::nothrow) ScanSession();
    if (session == nullptr) {
        SettingsTestWifi_PostWifiScanDone(false, 0);
        vTaskDelete(nullptr);
        return;
    }
    session->generation = my_gen;
    session->mu = xSemaphoreCreateMutexStatic(&session->mu_buf);
    session->evt = xEventGroupCreate();

    auto finish_fail = [&]() {
        SettingsTestWifi_DestroyScanSession(session);
        delete session;
        session = nullptr;
        if (my_gen == SettingsTestWifi_scan_generation) {
            SettingsTestWifi_PostWifiScanDone(false, 0);
        } else {
            SettingsTest_Ui().wifi_scan_busy = false;
        }
        vTaskDelete(nullptr);
    };

    if (session->mu == nullptr || session->evt == nullptr) {
        ESP_LOGE(kTag, "session alloc failed");
        finish_fail();
        return;
    }

    if (!WifiScanLease::TryAcquire(kLeaseOwner)) {
        ESP_LOGW(kTag, "lease busy");
        finish_fail();
        return;
    }
    session->lease_held = true;

    if (esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &SettingsTestWifi_OnScanDoneCapture,
                                            session, &session->handler) != ESP_OK) {
        ESP_LOGE(kTag, "register SCAN_DONE failed");
        finish_fail();
        return;
    }

    if (!SettingsTest_IsLive() || my_gen != SettingsTestWifi_scan_generation) {
        SettingsTestWifi_DestroyScanSession(session);
        delete session;
        SettingsTest_Ui().wifi_scan_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    if (!SettingsTestWifi_WifiEnsureForScan()) {
        finish_fail();
        return;
    }

    esp_wifi_scan_stop();
    xEventGroupClearBits(session->evt, static_cast<EventBits_t>(kSettingsTestBitScanDone));
    if (xSemaphoreTake(session->mu, pdMS_TO_TICKS(200)) == pdTRUE) {
        session->captured = false;
        session->records.clear();
        xSemaphoreGive(session->mu);
    }

    wifi_scan_config_t cfg = {};
    cfg.show_hidden = false;
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    const esp_err_t start_err = esp_wifi_scan_start(&cfg, false);
    if (start_err != ESP_OK) {
        ESP_LOGW(kTag, "scan_start: %s", esp_err_to_name(start_err));
        finish_fail();
        return;
    }

    const EventBits_t bits = xEventGroupWaitBits(
        session->evt, static_cast<EventBits_t>(kSettingsTestBitScanDone), pdTRUE, pdTRUE,
        pdMS_TO_TICKS(kScanTimeoutMs));

    std::vector<wifi_ap_record_t> local_records;
    bool ok = false;
    if ((bits & kSettingsTestBitScanDone) &&
        xSemaphoreTake(session->mu, pdMS_TO_TICKS(200)) == pdTRUE) {
        ok = session->captured;
        local_records = std::move(session->records);
        xSemaphoreGive(session->mu);
    } else {
        esp_wifi_scan_stop();
        ESP_LOGW(kTag, "scan timeout or capture missed");
    }

    SettingsTestWifi_DestroyScanSession(session);
    delete session;
    session = nullptr;

    if (my_gen != SettingsTestWifi_scan_generation || !SettingsTest_IsLive()) {
        local_records.clear();
        SettingsTest_Ui().wifi_scan_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    if (!ok) {
        local_records.clear();
        SettingsTestWifi_PostWifiScanDone(false, 0);
        vTaskDelete(nullptr);
        return;
    }

    SettingsTestWifi_FillUiApsFromRecords(local_records);
    local_records.clear();
    SettingsTestWifi_PostWifiScanDone(true, static_cast<int>(SettingsTest_Ui().wifi_aps.size()));
    vTaskDelete(nullptr);
}

void SettingsTestWifi_ScheduleWifiScan() {
    if (!SettingsTest_IsLive() || SettingsTest_Ui().wifi_scan_busy) {
        return;
    }
    ++SettingsTestWifi_scan_generation;
    SettingsTest_SetRowValue(SettingsTest_Ui().wifi, Lang::Strings::SETTINGS_TEST_SCANNING, false);
    SettingsTestWifi_SetWifiRescanEnabled(false);
    SettingsTestWifi_RebuildWifiListPage();
    if (xTaskCreate(SettingsTestWifi_WifiScanTask, "set_wifi_scan", 6144, nullptr, 5, nullptr) != pdPASS) {
        SettingsTest_Ui().wifi_scan_busy = false;
        SettingsTest_SetRowValue(SettingsTest_Ui().wifi, Lang::Strings::CLOUD_TASK_FAIL, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().wifi, false);
        SettingsTestWifi_SetWifiRescanEnabled(true);
    }
}

void SettingsTestWifi_InvalidateInFlightScan() {
    ++SettingsTestWifi_scan_generation;
    esp_wifi_scan_stop();
    // 进行中的任务在 WaitBits 返回后见 generation 变化会自行 Release/清 busy
}

void SettingsTestWifi_ScheduleScan() {
    SettingsTestWifi_ScheduleWifiScan();
}

void SettingsTestWifi_ClosePopup() {
    SettingsTestWifi_CloseWifiPopup();
}

void SettingsTestWifi_Teardown() {
    SettingsTestWifi_InvalidateInFlightScan();
    SettingsTestWifi_WifiTeardownIfOwned();
}

void SettingsTestWifi_OnRowClicked(lv_event_t* e) {
    SettingsTestWifi_OnWifiRowClicked(e);
}
