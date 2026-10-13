#include "battery_uv_lockout.h"

#include "bq27220_gauge.h"
#include "display/lv_adapter_display.h"
#include "ota_confirm_dialog/ota_confirm_dialog.h"
#include "ota_upgrade_screen/ota_upgrade_screen.h"
#include "standby_screen/standby_screen.h"
#include "power_policy.h"
#include "power_policy_priv.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iot_button.h>

#include <atomic>

namespace {

constexpr const char* TAG = "BatteryUv";
constexpr uint16_t kShutdownThresholdMv = 3200;
constexpr uint16_t kValidVbatMinMv = 2500;
constexpr int16_t kChargingCurrentMa = 5;
constexpr uint32_t kWarningHoldMs = 2000;
constexpr uint32_t kTaskStack = 4096;

std::atomic<bool> s_lockout{false};
std::atomic<bool> s_task_started{false};

void BatteryUvShutdownTask(void* /*arg*/)
{
    if (pwr::mu != nullptr) {
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        pwr::EnsureTouchSleepLocked();
        xSemaphoreGive(pwr::mu);
    }
    const esp_err_t key_err = iot_button_stop();
    if (key_err != ESP_OK) {
        ESP_LOGW(TAG, "iot_button_stop: %s", esp_err_to_name(key_err));
    }

    if (auto* disp = LVAdapterDisplay::Instance()) {
        disp->ShowLowBatteryShutdownScreen();
    }
    vTaskDelay(pdMS_TO_TICKS(kWarningHoldMs));
    PowerPolicy::GetInstance().RequestPowerOff();
    vTaskDelete(nullptr);
}

bool IsCharging()
{
    int16_t ma = 0;
    if (!Bq27220Gauge::GetInstance().ReadCurrentMa(ma)) {
        return false;
    }
    return ma > kChargingCurrentMa;
}

}  // namespace

bool BatteryUvLockout_IsActive()
{
    return s_lockout.load(std::memory_order_acquire);
}

void BatteryUvShutdownTick()
{
    if (s_task_started.load(std::memory_order_acquire)) {
        return;
    }
    if (StandbyScreen::IsActive()) {
        return;
    }
    if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
        return;
    }
    if (pwr::mu != nullptr) {
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        const bool shutting = pwr::shutdown_requested;
        xSemaphoreGive(pwr::mu);
        if (shutting) {
            return;
        }
    }

    uint16_t mv = 0;
    if (!Bq27220Gauge::GetInstance().ReadVoltageMv(mv)) {
        return;
    }
    if (mv < kValidVbatMinMv || mv >= kShutdownThresholdMv) {
        return;
    }
    if (IsCharging()) {
        return;
    }

    bool expected = false;
    if (!s_task_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }
    s_lockout.store(true, std::memory_order_release);
    ESP_LOGW(TAG, "vbat=%u mV below %u, show warning then power off", mv, kShutdownThresholdMv);
    if (xTaskCreate(BatteryUvShutdownTask, "batt_uv", kTaskStack, nullptr, tskIDLE_PRIORITY + 6,
                    nullptr) != pdPASS) {
        s_task_started.store(false, std::memory_order_release);
        s_lockout.store(false, std::memory_order_release);
        ESP_LOGE(TAG, "batt_uv task create failed");
    }
}
