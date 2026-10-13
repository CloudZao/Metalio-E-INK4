#include "metalio_buttons.h"

#include "assets/lang_config.h"
#include "audio_codec.h"
#include "battery_uv_lockout.h"
#include "board.h"
#include "config.h"
#include "display.h"
#include "display/screen/book_screen/book_screen.h"
#include "display/screen/boot_key_handler.h"
#include "display/screen/ota_confirm_dialog/ota_confirm_dialog.h"
#include "display/screen/ota_upgrade_screen/ota_upgrade_screen.h"
#include "display/screen/settings_screen/settings_log_upload.h"
#include "display/screen/settings_screen/settings_test/settings_test_aging_screen.h"
#include "display/screen/standby_screen/standby_screen.h"
#include "display/screen/vk_key_handler.h"
#include "IOExpander.hpp"
#include "power_policy.h"

#include <button_types.h>
#include <driver/gpio.h>
#include <esp_log.h>

#define TAG "MetalioBtn"

// TCA9555 按键：iot_button 自定义驱动轮询 IOExpander（默认低有效）
struct MetalioIoExpanderButtonDriver {
    button_driver_t base{};                           // iot_button 驱动基座
    IOExpander::Pin pin = IOExpander::Pin::kPinCount; // 扩展器引脚
    uint8_t active_level = 0;                         // 按下有效电平
};

static uint8_t MetalioIoExpanderButton_GetKeyLevel(button_driver_t* button_driver) {
    auto* self = reinterpret_cast<MetalioIoExpanderButtonDriver*>(button_driver);
    uint8_t level = 1;
    if (IOExpander::getInstance().getLevel(self->pin, &level) != ESP_OK) {
        return 0;
    }
    return level == self->active_level ? 1 : 0;
}

static esp_err_t MetalioIoExpanderButton_Delete(button_driver_t* button_driver) {
    delete reinterpret_cast<MetalioIoExpanderButtonDriver*>(button_driver);
    return ESP_OK;
}

static button_handle_t MetalioIoExpanderButton_Create(IOExpander::Pin pin,
                                                      bool active_high = false) {
    auto* drv = new MetalioIoExpanderButtonDriver();
    drv->pin = pin;
    drv->active_level = active_high ? 1 : 0;
    drv->base.enable_power_save = false;
    drv->base.get_key_level = MetalioIoExpanderButton_GetKeyLevel;
    drv->base.enter_power_save = nullptr;
    drv->base.del = MetalioIoExpanderButton_Delete;

    button_config_t cfg = {};
    button_handle_t handle = nullptr;
    ESP_ERROR_CHECK(iot_button_create(&cfg, &drv->base, &handle));
    return handle;
}

void MetalioButtons_Init(Button& boot, Button& power, std::unique_ptr<Button>& volume_up,
                         std::unique_ptr<Button>& volume_down, MetalioPowerKeyState& power_key,
                         Board* board) {
    // 注册回调时若仍按住，屏蔽至首次松开，避免上电残留长按关机 / 松手误进待机
    power_key.suppress = (gpio_get_level(POWER_BUTTON_GPIO) == 0);
    power_key.swallow_click = false;
    if (power_key.suppress) {
        ESP_LOGW(TAG, "POWER held at button init (GPIO%d), suppress until release",
                 static_cast<int>(POWER_BUTTON_GPIO));
    }

    boot.OnPressDown([]() {
        ESP_LOGI(TAG, "按键按下: BOOT (GPIO%d)", static_cast<int>(BOOT_BUTTON_GPIO));
        BootKey_OnPressDown();
    });
    boot.OnPressUp([]() {
        ESP_LOGI(TAG, "按键松开: BOOT (GPIO%d)", static_cast<int>(BOOT_BUTTON_GPIO));
        BootKey_OnPressUp();
    });
    boot.OnClick([]() {
        ESP_LOGI(TAG, "按键点击: BOOT (GPIO%d)", static_cast<int>(BOOT_BUTTON_GPIO));
        BootKey_OnClick();
    });
    boot.OnDoubleClick([]() {
        ESP_LOGI(TAG, "按键双击: BOOT (GPIO%d)", static_cast<int>(BOOT_BUTTON_GPIO));
        BootKey_OnDoubleClick();
    });
    boot.OnLongPress([]() {
        ESP_LOGI(TAG, "按键长按 %ums: BOOT (GPIO%d)", static_cast<unsigned>(kBootLongPressMs),
                 static_cast<int>(BOOT_BUTTON_GPIO));
        BootKey_OnLongPress();
    });

    power.OnPressDown([&power_key]() {
        if (power_key.suppress) {
            return;
        }
        if (BatteryUvLockout_IsActive()) {
            return;
        }
        if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
            ESP_LOGI(TAG, "POWER press-down blocked (ota upgrade)");
            return;
        }
        if (SettingsTestAgingScreen::IsActive()) {
            ESP_LOGI(TAG, "POWER press-down (aging test, activity only)");
        }
        if (!StandbyScreen::IsActive()) {
            PowerPolicy::GetInstance().NotifyUserActivity();
        }
        ESP_LOGI(TAG, "按键按下: POWER (GPIO%d)", static_cast<int>(POWER_BUTTON_GPIO));
    });
    power.OnPressUp([&power_key]() {
        if (power_key.suppress) {
            power_key.suppress = false;
            power_key.swallow_click = true;
            ESP_LOGI(TAG, "POWER released after boot hold, gestures armed");
        }
    });
    power.OnClick([&power_key]() {
        if (power_key.suppress) {
            return;
        }
        if (power_key.swallow_click) {
            power_key.swallow_click = false;
            ESP_LOGI(TAG, "POWER click swallowed (boot-hold release)");
            return;
        }
        ESP_LOGI(TAG, "按键点击: POWER 进/出待机 (GPIO%d)",
                 static_cast<int>(POWER_BUTTON_GPIO));
        PowerKey_OnClick();
    });
    power.OnLongPress([&power_key]() {
        if (power_key.suppress) {
            ESP_LOGI(TAG, "POWER long-press suppressed (boot hold)");
            return;
        }
        ESP_LOGI(TAG, "按键长按 %ums: POWER (GPIO%d)",
                 static_cast<unsigned>(kPowerLongPressMs),
                 static_cast<int>(POWER_BUTTON_GPIO));
        PowerKey_OnLongPress();
    });

    if (!IOExpander::getInstance().isInitialized()) {
        ESP_LOGW(TAG, "skip volume buttons: IOExpander not ready");
    } else {
        volume_down = std::make_unique<Button>(
            MetalioIoExpanderButton_Create(IOExpander::Pin::VOLUME_DOWN));
        volume_up =
            std::make_unique<Button>(MetalioIoExpanderButton_Create(IOExpander::Pin::VOLUME_UP));

        // 正文阅读：音量+ = 上一页，音量- = 下一页（含长按连翻）；其它页仍调音量
        volume_up->OnClick([board]() {
            if (BatteryUvLockout_IsActive()) {
                return;
            }
            if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
                ESP_LOGI(TAG, "VOLUME_UP blocked (ota upgrade)");
                return;
            }
            if (SettingsLogUpload_IsActive()) {
                ESP_LOGI(TAG, "VOLUME_UP blocked (log upload)");
                return;
            }
            if (SettingsTestAgingScreen::IsActive()) {
                ESP_LOGI(TAG, "VOLUME_UP blocked (aging test)");
                return;
            }
            PowerPolicy::GetInstance().NotifyUserActivity();
            if (BookScreen::IsReadingActive()) {
                VkKey_Dispatch("vk_prev");
                return;
            }
            auto* codec = board->GetAudioCodec();
            int volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            ESP_LOGI(TAG, "VOLUME_UP (P1.0): volume=%d", volume);
            board->GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_up->OnLongPress([board]() {
            if (BatteryUvLockout_IsActive()) {
                return;
            }
            if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
                return;
            }
            if (SettingsLogUpload_IsActive()) {
                return;
            }
            if (SettingsTestAgingScreen::IsActive()) {
                return;
            }
            PowerPolicy::GetInstance().NotifyUserActivity();
            if (BookScreen::IsReadingActive()) {
                VkKey_OnLongPress("vk_prev");
                return;
            }
            board->GetAudioCodec()->SetOutputVolume(100);
            ESP_LOGI(TAG, "VOLUME_UP long: volume=100");
            board->GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });
        volume_up->OnPressUp([]() {
            if (BatteryUvLockout_IsActive()) {
                return;
            }
            if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
                return;
            }
            if (SettingsLogUpload_IsActive()) {
                return;
            }
            if (SettingsTestAgingScreen::IsActive()) {
                return;
            }
            if (BookScreen::IsReadingActive()) {
                VkKey_OnPressUp("vk_prev");
            }
        });

        volume_down->OnClick([board]() {
            if (BatteryUvLockout_IsActive()) {
                return;
            }
            if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
                ESP_LOGI(TAG, "VOLUME_DOWN blocked (ota upgrade)");
                return;
            }
            if (SettingsLogUpload_IsActive()) {
                ESP_LOGI(TAG, "VOLUME_DOWN blocked (log upload)");
                return;
            }
            if (SettingsTestAgingScreen::IsActive()) {
                ESP_LOGI(TAG, "VOLUME_DOWN blocked (aging test)");
                return;
            }
            PowerPolicy::GetInstance().NotifyUserActivity();
            if (BookScreen::IsReadingActive()) {
                VkKey_Dispatch("vk_next");
                return;
            }
            auto* codec = board->GetAudioCodec();
            int volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            ESP_LOGI(TAG, "VOLUME_DOWN (P0.7): volume=%d", volume);
            board->GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_down->OnLongPress([board]() {
            if (BatteryUvLockout_IsActive()) {
                return;
            }
            if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
                return;
            }
            if (SettingsLogUpload_IsActive()) {
                return;
            }
            if (SettingsTestAgingScreen::IsActive()) {
                return;
            }
            PowerPolicy::GetInstance().NotifyUserActivity();
            if (BookScreen::IsReadingActive()) {
                VkKey_OnLongPress("vk_next");
                return;
            }
            board->GetAudioCodec()->SetOutputVolume(0);
            ESP_LOGI(TAG, "VOLUME_DOWN long: volume=0 (muted)");
            board->GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
        volume_down->OnPressUp([]() {
            if (BatteryUvLockout_IsActive()) {
                return;
            }
            if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
                return;
            }
            if (SettingsLogUpload_IsActive()) {
                return;
            }
            if (SettingsTestAgingScreen::IsActive()) {
                return;
            }
            if (BookScreen::IsReadingActive()) {
                VkKey_OnPressUp("vk_next");
            }
        });
    }

    ESP_LOGI(TAG, "Buttons ready: BOOT=GPIO%d POWER=GPIO%d VOL-=P0.7 VOL+=P1.0",
             static_cast<int>(BOOT_BUTTON_GPIO), static_cast<int>(POWER_BUTTON_GPIO));
}
