#include "boot_key_handler.h"

#include <atomic>

#include <esp_log.h>
#include <lvgl.h>

#include "cloud_screen/cloud_screen.h"
#include "ota_upgrade_screen/ota_upgrade_screen.h"
#include "ota_confirm_dialog/ota_confirm_dialog.h"
#include "power_policy.h"
#include "screen_common.h"
#include "settings_screen/settings_test/settings_test_aging_screen.h"
#include "standby_screen/standby_screen.h"
#include "vk_key_handler.h"

namespace {

constexpr const char* TAG = "BootKey";
std::atomic<bool> s_boot_held{false};
std::atomic<bool> s_long_press_fired{false};

bool DispatchBootAction(BootKeyAction action, const char* kind, const char* screen) {
    if (action == nullptr) {
        ESP_LOGI(TAG, "%s no-op (screen=%s has no handler)", kind, screen);
        return false;
    }
    if (!action()) {
        ESP_LOGI(TAG, "%s ignored by screen=%s", kind, screen);
        return false;
    }
    ESP_LOGI(TAG, "%s handled by screen=%s", kind, screen);
    return true;
}

void EnterStandbyAsync(void* /*arg*/) {
    CloudScreen::StopTransfer();
    StandbyScreen::Show();
}

}  // namespace

void BootKey_OnPressDown() {
    if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
        ESP_LOGI(TAG, "press-down blocked (ota upgrade)");
        return;
    }
    if (SettingsTestAgingScreen::IsActive()) {
        ESP_LOGI(TAG, "press-down blocked (aging test)");
        return;
    }
    s_boot_held.store(true, std::memory_order_release);
    s_long_press_fired.store(false, std::memory_order_release);
    const char* screen = VkKey_ActiveScreen();
    ESP_LOGI(TAG, "press-down on screen=%s", screen);
    DispatchBootAction(VkKey_GetBootPressDown(screen), "press-down", screen);
}

void BootKey_OnPressUp() {
    if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
        s_boot_held.store(false, std::memory_order_release);
        ESP_LOGI(TAG, "press-up blocked (ota upgrade)");
        return;
    }
    if (SettingsTestAgingScreen::IsActive()) {
        s_boot_held.store(false, std::memory_order_release);
        ESP_LOGI(TAG, "press-up blocked (aging test)");
        return;
    }
    s_boot_held.store(false, std::memory_order_release);
    const char* screen = VkKey_ActiveScreen();
    ESP_LOGI(TAG, "press-up on screen=%s", screen);
    DispatchBootAction(VkKey_GetBootPressUp(screen), "press-up", screen);
}

void BootKey_OnClick() {
    if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
        ESP_LOGI(TAG, "short-press blocked (ota upgrade)");
        return;
    }
    if (SettingsTestAgingScreen::IsActive()) {
        ESP_LOGI(TAG, "short-press blocked (aging test)");
        return;
    }
    if (s_long_press_fired.load(std::memory_order_acquire)) {
        ESP_LOGI(TAG, "short-press suppressed (long-press already fired this press)");
        return;
    }
    const char* screen = VkKey_ActiveScreen();
    ESP_LOGI(TAG, "short-press on screen=%s", screen);
    // 仅页内钩子（如百问打断）；进/出待机只走电源键短按
    DispatchBootAction(VkKey_GetBootClick(screen), "short-press", screen);
}

void BootKey_OnDoubleClick() {
    if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
        ESP_LOGI(TAG, "double-click blocked (ota upgrade)");
        return;
    }
    if (SettingsTestAgingScreen::IsActive()) {
        ESP_LOGI(TAG, "double-click blocked (aging test)");
        return;
    }
    if (s_long_press_fired.load(std::memory_order_acquire)) {
        ESP_LOGI(TAG, "double-click suppressed (long-press already fired)");
        return;
    }
    const char* screen = VkKey_ActiveScreen();
    ESP_LOGI(TAG, "double-click on screen=%s", screen);
    DispatchBootAction(VkKey_GetBootDoubleClick(screen), "double-click", screen);
}

void BootKey_OnLongPress() {
    if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
        s_long_press_fired.store(true, std::memory_order_release);
        ESP_LOGI(TAG, "long-press blocked (ota upgrade)");
        return;
    }
    if (SettingsTestAgingScreen::IsActive()) {
        s_long_press_fired.store(true, std::memory_order_release);
        ESP_LOGI(TAG, "long-press blocked (aging test)");
        return;
    }
    s_long_press_fired.store(true, std::memory_order_release);
    if (StandbyScreen::IsActive()) {
        if (StandbyScreen::HandleBootLongPress()) {
            ESP_LOGI(TAG, "long-press handled by standby overlay");
            return;
        }
    }
    const char* screen = VkKey_ActiveScreen();
    ESP_LOGI(TAG, "long-press on screen=%s", screen);
    DispatchBootAction(VkKey_GetBootLongPress(screen), "long-press", screen);
}

void PowerKey_OnClick() {
    if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
        ESP_LOGI(TAG, "power short blocked (ota upgrade)");
        return;
    }
    if (SettingsTestAgingScreen::IsActive()) {
        ESP_LOGI(TAG, "power short blocked (aging test)");
        return;
    }
    if (StandbyScreen::IsActive()) {
        // 待机 Overlay 期间 iot_button 已挂起，不应到达此处
        ESP_LOGW(TAG, "power short in standby (unexpected iot path)");
        return;
    }
    ESP_LOGI(TAG, "power short -> standby");
    PowerPolicy::GetInstance().PreparePowerKeyStandby();
    // 紧急投递：勿经 MainEventLoop（百问 EnsureAssistantNetworkReady 可占死主循环）
    if (!ScreenLvAsyncUrgent(EnterStandbyAsync)) {
        ESP_LOGW(TAG, "power short: ScreenLvAsyncUrgent failed, fallback Schedule");
        ScreenLvAsync(EnterStandbyAsync);
    }
}

void PowerKey_OnLongPress() {
    if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
        ESP_LOGI(TAG, "power long blocked (ota upgrade)");
        return;
    }
    // 老化测试保留长按关机，便于产测救砖
    if (StandbyScreen::IsActive()) {
        ESP_LOGW(TAG, "power long in standby (unexpected iot path)");
        return;
    }
    ESP_LOGI(TAG, "power long -> RequestPowerOff");
    PowerPolicy::GetInstance().RequestPowerOff();
}

bool BootKey_IsHeld() {
    return s_boot_held.load(std::memory_order_acquire);
}

bool BootKey_DidLongPress() {
    return s_long_press_fired.load(std::memory_order_acquire);
}
