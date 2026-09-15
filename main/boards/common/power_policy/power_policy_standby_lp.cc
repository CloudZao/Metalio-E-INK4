#include "power_policy_priv.h"

#include "power_hw.h"
#include "standby_screen/standby_screen.h"

#include "config.h"
#include <esp_lv_adapter.h>

#include <driver/gpio.h>
#include <iot_button.h>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_sleep.h>

#include "lvgl.h"

namespace {

constexpr const char* TAG = "PowerPolicy";

bool iot_keys_suspended = false;

/** lv_async_call 须在 LVGL 线程或持 esp_lv_adapter_lock；standby_lp 走后者 */
void LvAsyncCallLocked(lv_async_cb_t cb, void* user_data)
{
    if (cb == nullptr) {
        return;
    }
    if (!esp_lv_adapter_is_initialized()) {
        ESP_LOGW(TAG, "lv_async skipped: adapter not init");
        return;
    }
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGW(TAG, "lv_async skipped: adapter lock failed");
        return;
    }
    if (lv_async_call(cb, user_data) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "lv_async_call failed");
    }
    esp_lv_adapter_unlock();
}

int MeasureGpioHoldMs(gpio_num_t gpio)
{
    int held_ms = 0;
    while (gpio_get_level(gpio) == 0 && held_ms < 10000) {
        vTaskDelay(pdMS_TO_TICKS(20));
        held_ms += 20;
        // 满长按阈值立刻动作，勿等松手（BOOT→百问 hold-through；POWER→关机刷屏）
        if (gpio == BOOT_BUTTON_GPIO && held_ms >= kBootLongPressMs) {
            break;
        }
        if (gpio == POWER_BUTTON_GPIO && held_ms >= kPowerLongPressMs) {
            break;
        }
    }
    return held_ms;
}

/** @return true 结束本轮 LP（出待机）；false 继续浅睡 */
bool RunStandbyBootKeyGesture(int held_ms)
{
    if (held_ms >= kBootLongPressMs) {
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        pwr::WakeTouchLocked();
        pwr::ResetStandbyElapsedLocked();
        pwr::last_user_activity_us = esp_timer_get_time();
        pwr::PrepareStandbyWakeExitLocked();
        xSemaphoreGive(pwr::mu);
        ESP_LOGI(TAG, "BOOT standby long %d ms -> assistant", held_ms);
        StandbyScreen::HandleBootLongPress();
        return true;
    }
    // 短按不退出待机；浅睡会被 GPIO 短暂唤醒，量完后继续睡
    ESP_LOGI(TAG, "BOOT standby short %d ms -> stay LP", held_ms);
    return false;
}

void RunStandbyPowerKeyGesture(int held_ms)
{
    const bool is_long = held_ms >= kPowerLongPressMs;
    if (is_long) {
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        pwr::PrepareStandbyWakeExitLocked();
        ESP_LOGW(TAG, "POWER standby long %d ms -> power off", held_ms);
        pwr::BeginPowerOffLocked();
        xSemaphoreGive(pwr::mu);
        // 阈值处已开刷，键可能仍按住；松开后再结束 LP，避免 resume iot 误触
        while (gpio_get_level(POWER_BUTTON_GPIO) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    } else {
        ESP_LOGI(TAG, "POWER standby short %d ms -> dismiss", held_ms);
        pwr::RequestDismissStandbyOverlay();
    }
}

bool PollStandbyPhysicalKeys()
{
    const bool boot_held = (gpio_get_level(BOOT_BUTTON_GPIO) == 0);
    const bool power_held = (gpio_get_level(POWER_BUTTON_GPIO) == 0);
    if (!boot_held && !power_held) {
        return false;
    }
    ESP_LOGI(TAG, "standby key boot=%d power=%d", boot_held ? 1 : 0, power_held ? 1 : 0);
    if (boot_held) {
        return RunStandbyBootKeyGesture(MeasureGpioHoldMs(BOOT_BUTTON_GPIO));
    }
    xSemaphoreTake(pwr::mu, portMAX_DELAY);
    pwr::WakeTouchLocked();
    pwr::ResetStandbyElapsedLocked();
    pwr::last_user_activity_us = esp_timer_get_time();
    xSemaphoreGive(pwr::mu);
    RunStandbyPowerKeyGesture(MeasureGpioHoldMs(POWER_BUTTON_GPIO));
    return true;
}

bool DelayMsWithStandbyKeyPoll(uint32_t delay_ms)
{
    uint32_t elapsed_ms = 0;
    while (elapsed_ms < delay_ms) {
        if (PollStandbyPhysicalKeys()) {
            return true;
        }
        const uint32_t step_ms = (delay_ms - elapsed_ms > 50) ? 50 : (delay_ms - elapsed_ms);
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        elapsed_ms += step_ms;
    }
    return false;
}

/** 攒帧 defer：壁纸解码完即 Park，勿盲等 kStandbyUiSettleMs；经典页最短 80ms 布局。 */
bool WaitStandbyPaintReady()
{
    constexpr uint32_t kMinMs = 80;
    constexpr uint32_t kStepMs = 50;
    uint32_t elapsed_ms = 0;
    while (elapsed_ms < static_cast<uint32_t>(pwr::kStandbyUiSettleMs)) {
        if (PollStandbyPhysicalKeys()) {
            return true;
        }
        if (elapsed_ms >= kMinMs && StandbyScreen::IsPaintReady()) {
            return false;
        }
        const uint32_t remain = static_cast<uint32_t>(pwr::kStandbyUiSettleMs) - elapsed_ms;
        const uint32_t step_ms = remain > kStepMs ? kStepMs : remain;
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        elapsed_ms += step_ms;
    }
    return false;
}

void ResumeIotKeys()
{
    if (!iot_keys_suspended) {
        return;
    }
    const esp_err_t err = iot_button_resume();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "iot keys resume failed: %s", esp_err_to_name(err));
        return;
    }
    iot_keys_suspended = false;
    ESP_LOGI(TAG, "iot keys resumed");
}

void ResumeIotKeysAsync(void* /*arg*/)
{
    ResumeIotKeys();
}

void SuspendIotKeys()
{
    if (iot_keys_suspended) {
        return;
    }
    const esp_err_t err = iot_button_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "iot keys suspend failed: %s", esp_err_to_name(err));
        return;
    }
    iot_keys_suspended = true;
    ESP_LOGI(TAG, "iot keys suspended for standby LP");
}

int64_t StandbyUiRemainUsLocked()
{
    if (pwr::standby_to_shutdown_sec <= 0) {
        return static_cast<int64_t>(pwr::kLightSleepSliceUs);
    }
    const int64_t elapsed = pwr::standby_elapsed_us;
    int64_t total = elapsed;
    if (pwr::standby_awake_mark_us != 0) {
        total += esp_timer_get_time() - pwr::standby_awake_mark_us;
    }
    if (total >= pwr::StandbyToShutdownUs()) {
        return 0;
    }
    return pwr::StandbyToShutdownUs() - total;
}

void CloseStandbyAwakeSegmentLocked()
{
    if (pwr::standby_awake_mark_us == 0) {
        return;
    }
    pwr::standby_elapsed_us += esp_timer_get_time() - pwr::standby_awake_mark_us;
    pwr::standby_awake_mark_us = 0;
}

void OpenStandbyAwakeSegmentLocked()
{
    pwr::standby_awake_mark_us = esp_timer_get_time();
}

bool RunLightSleepSlice()
{
    xSemaphoreTake(pwr::mu, portMAX_DELAY);
    const int64_t remain_us = StandbyUiRemainUsLocked();
    xSemaphoreGive(pwr::mu);

    uint64_t slice = static_cast<uint64_t>(remain_us);
    if (slice > pwr::kLightSleepSliceUs) {
        slice = pwr::kLightSleepSliceUs;
    }
    if (slice < 1000) {
        slice = 1000;
    }

    xSemaphoreTake(pwr::mu, portMAX_DELAY);
    CloseStandbyAwakeSegmentLocked();
    xSemaphoreGive(pwr::mu);

    power_hw_light_sleep_once(slice);

    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        ESP_LOGI(TAG, "GPIO wake");
        if (PollStandbyPhysicalKeys()) {
            return true;
        }
        // BOOT 短按等「醒一下又继续睡」：补回 awake 段，避免漏计待机时长
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        OpenStandbyAwakeSegmentLocked();
        xSemaphoreGive(pwr::mu);
        return false;
    }
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        pwr::standby_elapsed_us += static_cast<int64_t>(slice);
        OpenStandbyAwakeSegmentLocked();
        xSemaphoreGive(pwr::mu);
        return false;
    }
    ESP_LOGW(TAG, "light sleep wake cause=%d, skip slice", static_cast<int>(cause));
    xSemaphoreTake(pwr::mu, portMAX_DELAY);
    OpenStandbyAwakeSegmentLocked();
    xSemaphoreGive(pwr::mu);
    vTaskDelay(pdMS_TO_TICKS(50));
    return false;
}

bool RunDelaySlice()
{
    xSemaphoreTake(pwr::mu, portMAX_DELAY);
    const int64_t remain_us = StandbyUiRemainUsLocked();
    const int64_t elapsed_s =
        (pwr::standby_elapsed_us +
         (pwr::standby_awake_mark_us != 0 ? esp_timer_get_time() - pwr::standby_awake_mark_us : 0)) /
        1000000;
    xSemaphoreGive(pwr::mu);

    uint64_t slice = static_cast<uint64_t>(remain_us);
    if (slice > pwr::kLightSleepSliceUs) {
        slice = pwr::kLightSleepSliceUs;
    }
    if (slice < 1000) {
        slice = 1000;
    }
    const uint32_t delay_ms = static_cast<uint32_t>(slice / 1000 > 0 ? slice / 1000 : 1);
    ESP_LOGI(TAG, "LP slice delay %lu ms (light sleep off, elapsed %ld s)",
             static_cast<unsigned long>(delay_ms), static_cast<long>(elapsed_s));
    return DelayMsWithStandbyKeyPoll(delay_ms);
}

bool StandbyLpShouldContinue()
{
    xSemaphoreTake(pwr::mu, portMAX_DELAY);
    if (pwr::shutdown_requested) {
        xSemaphoreGive(pwr::mu);
        return false;
    }
    const bool ok =
        pwr::mode == pwr::Mode::StandbyUi && pwr::board != nullptr && StandbyScreen::IsActive() &&
        !pwr::standby_wake_exit;
    if (!ok) {
        pwr::WakeTouchLocked();
        pwr::standby_ui_requested = false;
        pwr::ClearStandbyElapsedLocked();
    }
    xSemaphoreGive(pwr::mu);
    return ok;
}

bool RunStandbyLpMainLoop()
{
    for (;;) {
        if (!StandbyLpShouldContinue()) {
            return false;
        }
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        if (pwr::StandbyUiLongEnoughLocked()) {
            pwr::RequestShutdownLocked();
            xSemaphoreGive(pwr::mu);
            return false;
        }
        xSemaphoreGive(pwr::mu);

        if (PollStandbyPhysicalKeys()) {
            return true;
        }

        const bool key_done =
            pwr::kStandbyLpUseLightSleep ? RunLightSleepSlice() : RunDelaySlice();
        if (key_done) {
            return true;
        }
    }
}

void StandbyLpTask(void* /*arg*/)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "standby LP session start");

        int waited_ms = 0;
        while (!StandbyScreen::IsActive() && waited_ms < 8000) {
            vTaskDelay(pdMS_TO_TICKS(50));
            waited_ms += 50;
        }
        if (!StandbyScreen::IsActive()) {
            ESP_LOGW(TAG, "standby UI not active, LP session abort");
            xSemaphoreTake(pwr::mu, portMAX_DELAY);
            pwr::standby_ui_requested = false;
            pwr::ClearStandbyElapsedLocked();
            pwr::lp_session_active = false;
            pwr::ReevaluateLocked();
            xSemaphoreGive(pwr::mu);
            pwr::FlushCpuFreqOutsideLock();
            continue;
        }

        // 进待机第一件事：触摸深睡（待机靠 BOOT/POWER；勿等全刷后再睡）
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        pwr::EnsureTouchSleepLocked();
        xSemaphoreGive(pwr::mu);

        if (pwr::board != nullptr) {
            SuspendIotKeys();
        }

        const bool key_in_settle = WaitStandbyPaintReady();
        if (!key_in_settle) {
            // settle 攒帧完毕：全刷待机画面后冻结 flush
            StandbyScreen::EnterEpdSleep();
            RunStandbyLpMainLoop();
        }

        // dismiss 路径：OnStandbyOverlayDismissed 已 resume；勿再 lv_async（未持锁会崩）
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        const bool dismiss_exit = pwr::standby_wake_exit;
        xSemaphoreGive(pwr::mu);
        if (!dismiss_exit) {
            LvAsyncCallLocked(ResumeIotKeysAsync, nullptr);
        }
        xSemaphoreTake(pwr::mu, portMAX_DELAY);
        pwr::lp_session_active = false;
        xSemaphoreGive(pwr::mu);
        ESP_LOGI(TAG, "standby LP session end");
    }
}

}  // namespace

namespace pwr {

void EnsureTouchSleepLocked()
{
    if (board == nullptr || touch_asleep) {
        return;
    }
    if (board->TouchEnterSleep() == ESP_OK) {
        touch_asleep = true;
        ESP_LOGI(TAG, "touch sleep for standby LP");
    }
}

void ClearStandbyElapsedLocked()
{
    standby_elapsed_us = 0;
    standby_awake_mark_us = 0;
}

void ResetStandbyElapsedLocked()
{
    standby_elapsed_us = 0;
    standby_awake_mark_us = esp_timer_get_time();
}

bool StandbyUiLongEnoughLocked()
{
    if (standby_to_shutdown_sec <= 0) {
        return false;
    }
    int64_t elapsed = standby_elapsed_us;
    if (standby_awake_mark_us != 0) {
        elapsed += esp_timer_get_time() - standby_awake_mark_us;
    }
    return elapsed >= StandbyToShutdownUs();
}

void WakeTouchLocked()
{
    if (board == nullptr || !touch_asleep) {
        return;
    }
    if (board->TouchWakeByReset() == ESP_OK) {
        touch_asleep = false;
    }
}

void PrepareStandbyWakeExitLocked()
{
    standby_ui_requested = false;
    standby_wake_exit = true;
    ClearStandbyElapsedLocked();
}

void DismissStandbyOverlayAsync(void* /*arg*/)
{
    if (StandbyScreen::IsActive()) {
        StandbyScreen::Dismiss();
    }
}

void RequestDismissStandbyOverlay()
{
    xSemaphoreTake(mu, portMAX_DELAY);
    PrepareStandbyWakeExitLocked();
    last_user_activity_us = esp_timer_get_time();
    xSemaphoreGive(mu);
    LvAsyncCallLocked(DismissStandbyOverlayAsync, nullptr);
}

void RequestStandbyUiAsync()
{
    if (standby_ui_requested) {
        return;
    }
    // 仅置位；真正 lv_async_call(Show) 在 FlushCpuFreqOutsideLock（持锁外）
    standby_ui_requested = true;
    standby_show_pending = true;
}

void RequestShutdownLocked()
{
    if (shutdown_requested) {
        return;
    }
    const int64_t elapsed_s =
        (standby_elapsed_us +
         (standby_awake_mark_us != 0 ? esp_timer_get_time() - standby_awake_mark_us : 0)) /
        1000000;
    ESP_LOGW(TAG, "standby LP idle elapsed=%ld s (limit %d s) -> power off",
             static_cast<long>(elapsed_s), standby_to_shutdown_sec);
    BeginPowerOffLocked();
}

void ResumeIotKeysIfNeeded()
{
    ResumeIotKeys();
}

void ArmStandbyLpTaskLocked()
{
    if (lp_task == nullptr || lp_session_active || shutdown_requested || standby_wake_exit) {
        return;
    }
    lp_session_active = true;
    xTaskNotifyGive(lp_task);
}

void StartStandbyLpTask()
{
    if (lp_task != nullptr) {
        return;
    }
    BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(
        StandbyLpTask, "standby_lp", 4096, nullptr, tskIDLE_PRIORITY + 3, &lp_task, 0,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (ok != pdPASS) {
        lp_task = nullptr;
        ESP_LOGE(TAG, "create standby LP task failed");
    }
}

}  // namespace pwr
