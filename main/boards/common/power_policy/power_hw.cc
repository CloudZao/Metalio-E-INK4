#include "power_hw.h"

#include "IOExpander.hpp"
#include "application.h"
#include "assets/lang_config.h"
#include "bluetooth_screen/bluetooth_screen.h"
#include "config.h"
#include "device_state.h"
#include "display/lv_adapter_display.h"
#include "dual_network_board.h"
#include "nt26_board.h"
#include "board.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <stdio.h>
#include <wifi_configuration_ap.h>
#include <wifi_station.h>

static const char* TAG = "PowerHw";

// MAIN_PWR 掉电后外置 BT 模组冷启动，AT 前需短 settle（异步任务，勿堵 Flush/定时器）
static constexpr uint32_t kMainRailBtSettleMs = 400;
static constexpr uint32_t kMainRailBtTaskStack = 3072;

static bool s_main_rail_known = false;
static bool s_main_rail_on = false;
static std::atomic<bool> s_main_rail_bt_reinit_busy{false};

/** 仅 4G 模式返回 Nt26Board；WiFi 模式 / 非双网板为 nullptr */
static Nt26Board* GetCellularBoardOrNull()
{
    auto* dual = dynamic_cast<DualNetworkBoard*>(&Board::GetInstance());
    if (dual == nullptr || dual->GetNetworkType() != NetworkType::ML307) {
        return nullptr;
    }
    return dynamic_cast<Nt26Board*>(&dual->GetCurrentBoard());
}

static void NotifyNetworkIconChanged()
{
    Board::GetInstance().GetDisplay()->UpdateStatusBar(true);
}

static void MainRailBtReinitTask(void* /*arg*/)
{
    vTaskDelay(pdMS_TO_TICKS(kMainRailBtSettleMs));
    BluetoothScreen::ApplyDefaultMode();
    s_main_rail_bt_reinit_busy.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

static void ScheduleBtMode1AfterRailOn()
{
    bool expected = false;
    if (!s_main_rail_bt_reinit_busy.compare_exchange_strong(expected, true,
                                                            std::memory_order_acq_rel)) {
        ESP_LOGW(TAG, "MAIN_PWR BT reinit already pending");
        return;
    }
    if (xTaskCreate(MainRailBtReinitTask, "main_rail_bt", kMainRailBtTaskStack, nullptr, 5,
                    nullptr) != pdPASS) {
        s_main_rail_bt_reinit_busy.store(false, std::memory_order_release);
        ESP_LOGW(TAG, "MAIN_PWR BT reinit task create failed");
    }
}

/** 掉电前：4G 模组须在供电仍在时 Stop，否则软件态与硬件脱节 */
static void CellularPrepareRailOff()
{
    Nt26Board* cell = GetCellularBoardOrNull();
    if (cell == nullptr) {
        return;
    }
    cell->PrepareMainRailOff();
}

/** 上升沿：未初始化则异步重走 4G Start（与 BT 模式1 同形，不堵 Flush） */
static void ScheduleCellularRecoverAfterRailOn()
{
    Nt26Board* cell = GetCellularBoardOrNull();
    if (cell == nullptr) {
        return;
    }
    cell->RecoverAfterMainRailOnAsync();
}

// 关机图全刷后等墨水/视觉稳定再发 PWR_KEY
// static constexpr uint32_t kPostEpdParkSettleMs = 10000;
static constexpr uint32_t kPwrOffStack = 4 * 1024;     // pwr_off 任务栈

// 浅睡时 SPI 外设掉电，EPD_RST 易漂；钉住高电平，避免误复位导致墨水漂移
static void EpdRstHoldForLightSleep(void)
{
    gpio_set_level(EPD_PIN_RST, 1);
    esp_err_t err = gpio_hold_en(EPD_PIN_RST);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "gpio_hold_en(RST): %s", esp_err_to_name(err));
    }
}

static void EpdRstReleaseAfterLightSleep(void)
{
    gpio_set_level(EPD_PIN_RST, 1);
    gpio_hold_dis(EPD_PIN_RST);
}

// /** 关机脉冲前：MOSI/SCLK/CS/DC 置低，BUSY 输入高阻，RST 不动 */
static void EpdPinsParkBeforePowerOff(void)
{
    gpio_intr_disable(EPD_PIN_BUSY);

    const gpio_num_t low_pins[] = {
        EPD_PIN_MOSI,
        EPD_PIN_SCLK,
        EPD_PIN_CS,
        EPD_PIN_DC,
    };
    uint64_t low_mask = 0;
    for (size_t i = 0; i < sizeof(low_pins) / sizeof(low_pins[0]); ++i) {
        low_mask |= 1ULL << low_pins[i];
    }
    gpio_config_t low_cfg = {
        .pin_bit_mask = low_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&low_cfg);
    for (size_t i = 0; i < sizeof(low_pins) / sizeof(low_pins[0]); ++i) {
        gpio_set_level(low_pins[i], 0);
    }

    // BUSY：输入高阻（无上下拉、不开输出）
    gpio_config_t busy_cfg = {
        .pin_bit_mask = 1ULL << EPD_PIN_BUSY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&busy_cfg);

    ESP_LOGI(TAG, "EPD pins park: MOSI/SCLK/CS/DC=L BUSY=HiZ RST untouched");
}

esp_err_t power_hw_pa_set(bool on)
{
    static bool known = false;
    static bool last_on = false;
    if (known && last_on == on) {
        return ESP_OK;
    }

    auto& io = IOExpander::getInstance();
    if (!io.isInitialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = io.setLevel(IOExpander::Pin::PA, on);
    if (err == ESP_OK) {
        known = true;
        last_on = on;
        ESP_LOGI(TAG, "PA %s", on ? "on" : "off");
    }
    return err;
}

bool power_hw_main_rail_is_on(void)
{
    return s_main_rail_known && s_main_rail_on;
}

static esp_err_t MainRailSetImpl(bool on, bool reinit_peripherals_on_rise)
{
    if (s_main_rail_known && s_main_rail_on == on) {
        return ESP_OK;
    }

    auto& io = IOExpander::getInstance();
    if (!io.isInitialized()) {
        ESP_LOGW(TAG, "MAIN_PWR %s skipped (IO not ready)", on ? "on" : "off");
        return ESP_ERR_INVALID_STATE;
    }

    // 仅「已知曾掉电后再上电」才重初始化外设；开机首次对齐 known 不重复发 AT
    const bool rising = on && s_main_rail_known && !s_main_rail_on;
    const bool falling = !on && (!s_main_rail_known || s_main_rail_on);

    // 掉电前先停 4G（供电仍在），再拉低 MAIN_PWR；WiFi 模式 no-op
    if (falling) {
        CellularPrepareRailOff();
    }

    esp_err_t err = io.setLevel(IOExpander::Pin::MAIN_PWR, on);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MAIN_PWR %s failed: %s", on ? "on" : "off", esp_err_to_name(err));
        // 已 TearDown 但 GPIO 掉电失败：轨仍上电、软件无 modem → 必须重建，否则永久失联
        if (falling) {
            ESP_LOGW(TAG, "MAIN_PWR off failed after cellular teardown; re-init");
            ScheduleCellularRecoverAfterRailOn();
        }
        return err;
    }

    s_main_rail_known = true;
    s_main_rail_on = on;
    ESP_LOGI(TAG, "MAIN_PWR %s", on ? "on" : "off");

    // 掉电后模组状态丢失；上升沿异步恢复 BT 模式1 + 4G Start（不阻塞 Flush/定时器）
    // reinit_peripherals_on_rise=false：关机 hold 上电，勿重拉模组
    if (rising && reinit_peripherals_on_rise) {
        ScheduleBtMode1AfterRailOn();
        ScheduleCellularRecoverAfterRailOn();
    }
    return ESP_OK;
}

esp_err_t power_hw_main_rail_set(bool on)
{
    return MainRailSetImpl(on, true);
}

static bool IsWifiNetwork()
{
    auto* dual = dynamic_cast<DualNetworkBoard*>(&Board::GetInstance());
    return dual != nullptr && dual->GetNetworkType() == NetworkType::WIFI;
}

esp_err_t power_hw_wifi_stop(void)
{
    if (!IsWifiNetwork()) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (Application::GetInstance().GetDeviceState() == kDeviceStateWifiConfiguring) {
        wifi_mode_t mode = WIFI_MODE_NULL;
        if (esp_wifi_get_mode(&mode) != ESP_OK) {
            ESP_LOGW(TAG, "WiFi config LP pause skipped (not started)");
            return ESP_OK;
        }
        WifiConfigurationAp::GetInstance().PauseForLp();
        NotifyNetworkIconChanged();
        return ESP_OK;
    }
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        ESP_LOGW(TAG, "WiFi STA pause skipped (not started)");
        return ESP_OK;
    }
    // 待机浅睡只 pause，勿 Stop/deinit/destroy netif（反复 BOOT 会 netif already added）
    WifiStation::GetInstance().PauseForLp();
    ESP_LOGI(TAG, "WiFi STA paused for LP");
    NotifyNetworkIconChanged();
    return ESP_OK;
}

esp_err_t power_hw_wifi_start(void)
{
    if (!IsWifiNetwork()) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (Application::GetInstance().GetDeviceState() == kDeviceStateWifiConfiguring) {
        auto& wifi_ap = WifiConfigurationAp::GetInstance();
        esp_err_t err = wifi_ap.ResumeFromLp();
        if (err == ESP_OK) {
            return ESP_OK;
        }
        wifi_mode_t mode = WIFI_MODE_NULL;
        if (esp_wifi_get_mode(&mode) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi config already up");
            return ESP_OK;
        }
        wifi_ap.SetLanguage(Lang::CODE);
        wifi_ap.SetSsidPrefix("MetalioEInk4");
        wifi_ap.Start();
        ESP_LOGI(TAG, "WiFi config AP started");
        return ESP_OK;
    }
    auto& sta = WifiStation::GetInstance();
    if (sta.IsLpPaused()) {
        esp_err_t err = sta.ResumeFromLp();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "WiFi STA resumed from LP");
            NotifyNetworkIconChanged();
            return ESP_OK;
        }
        ESP_LOGW(TAG, "WiFi STA resume failed (%s), full Start", esp_err_to_name(err));
        sta.Start();
        ESP_LOGI(TAG, "WiFi STA started");
        NotifyNetworkIconChanged();
        return ESP_OK;
    }
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi already started");
        return ESP_OK;
    }
    sta.Start();
    ESP_LOGI(TAG, "WiFi STA started");
    NotifyNetworkIconChanged();
    return ESP_OK;
}

esp_err_t power_hw_cpu_freq_set(int mhz)
{
    #if CONFIG_PM_ENABLE
    if (mhz < 10) {
        mhz = 10;
    }
    esp_pm_config_t cfg = {
        .max_freq_mhz = mhz,
        .min_freq_mhz = mhz,
        .light_sleep_enable = false,
    };
    esp_err_t err = esp_pm_configure(&cfg);
    ESP_LOGI(TAG, "cpu %dMHz (no auto_ls): %s", mhz, esp_err_to_name(err));
    return err;
    #else
    (void)mhz;
    ESP_LOGW(TAG, "cpu freq skipped (CONFIG_PM_ENABLE off)");
    return ESP_ERR_NOT_SUPPORTED;
    #endif
}

static esp_err_t PrepareForPowerOff()
{
    auto& io = IOExpander::getInstance();
    if (!io.isInitialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    power_hw_pa_set(false);
    // 关机刷图前须保证外设轨在位；同步 known 态，但不重发 BT（即将断电）
    esp_err_t err_main = MainRailSetImpl(true, false);
    esp_err_t err_sock = io.setLevel(IOExpander::Pin::SCREEN_SOCKET_PWR, true);
    if (err_sock != ESP_OK || err_main != ESP_OK) {
        ESP_LOGW(TAG, "power-off prep rails: sock=%s main=%s",
                 esp_err_to_name(err_sock), esp_err_to_name(err_main));
        return (err_sock != ESP_OK) ? err_sock : err_main;
    }
    ESP_LOGI(TAG, "power-off prep: PA off, MAIN/SCREEN rails held");
    return ESP_OK;
}

static void PwrOffTask(void* /*arg*/)
{
    PrepareForPowerOff();
    if (auto* disp = LVAdapterDisplay::Instance()) {
        disp->ShowPoweredOffScreen();
    }

    // 暂关：关机画面后的 settle / Deep Sleep / 脚位 park，直接发脉冲便于联调
    // if (auto* disp = LVAdapterDisplay::Instance()) {
    //     disp->SleepEpdForPowerOff();
    // }
    // vTaskDelay(pdMS_TO_TICKS(100)) ;
    // EpdPinsParkBeforePowerOff();
    // vTaskDelay(pdMS_TO_TICKS(kPostEpdParkSettleMs));

    auto& io = IOExpander::getInstance();
    ESP_LOGW(TAG, "PWR_KEY_PULSE");
    constexpr int kPulseHalfMs = 100;
    for (;;) {
        io.setLevel(IOExpander::Pin::PWR_KEY_PULSE, true);
        vTaskDelay(pdMS_TO_TICKS(kPulseHalfMs));
        io.setLevel(IOExpander::Pin::PWR_KEY_PULSE, false);
        vTaskDelay(pdMS_TO_TICKS(kPulseHalfMs));
    }
}

void power_hw_begin_power_off(void)
{
    static bool started = false;
    if (started) {
        return;
    }
    started = true;
    ESP_LOGW(TAG, "begin power off (UI + pulse)");
    if (xTaskCreatePinnedToCore(PwrOffTask, "pwr_off", kPwrOffStack, nullptr,
                                tskIDLE_PRIORITY + 5, nullptr, 0) != pdPASS) {
        ESP_LOGE(TAG, "pwr_off task create failed");
        started = false;
    }
}

esp_err_t power_hw_light_sleep_once(uint64_t max_sleep_us)
{
    #if CONFIG_PM_ENABLE
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_err_t err = gpio_wakeup_enable(BOOT_BUTTON_GPIO, GPIO_INTR_LOW_LEVEL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BOOT wake cfg failed: %s", esp_err_to_name(err));
        return err;
    }
    err = gpio_wakeup_enable(POWER_BUTTON_GPIO, GPIO_INTR_LOW_LEVEL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "POWER wake cfg failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_sleep_enable_gpio_wakeup();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "gpio wake enable failed: %s", esp_err_to_name(err));
        return err;
    }
    if (max_sleep_us > 0) {
        err = esp_sleep_enable_timer_wakeup(max_sleep_us);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "timer wake cfg failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    ESP_LOGW(TAG, "blocking light sleep; BOOT GPIO%d + POWER GPIO%d%s",
             static_cast<int>(BOOT_BUTTON_GPIO), static_cast<int>(POWER_BUTTON_GPIO),
             max_sleep_us > 0 ? " + timer" : "");
    // 浅睡后 UART 停：尽量刷出「即将入睡」日志
    fflush(stdout);
    #if defined(CONFIG_ESP_CONSOLE_UART_NUM) && (CONFIG_ESP_CONSOLE_UART_NUM >= 0)
    uart_wait_tx_idle_polling(static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM));
    #endif
    EpdRstHoldForLightSleep();
    err = esp_light_sleep_start();
    EpdRstReleaseAfterLightSleep();
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGW(TAG, "woke from light sleep: %s cause=%d", esp_err_to_name(err), (int)cause);
    return err;
    #else
    (void)max_sleep_us;
    ESP_LOGW(TAG, "light sleep skipped (CONFIG_PM_ENABLE off)");
    return ESP_ERR_NOT_SUPPORTED;
    #endif
}
