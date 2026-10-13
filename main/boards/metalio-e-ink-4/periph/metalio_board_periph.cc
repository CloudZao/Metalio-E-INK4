#include "metalio_board_periph.h"

#include "bq27220_gauge.h"
#include "config.h"
#include "cx25601n.h"
#include "display/screen/bluetooth_screen/bluetooth_screen.h"
#include "pcf8563.h"
#include "SdCardManager.hpp"
#include "sd_paths.h"
#include "settings.h"
#include "SimpleUart.hpp"
#include "usb_virtual_disk.h"

#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>
#include <vector>

#define TAG "MetalioPeriph"

void MetalioPeriph_InitBtAudio() {
    SimpleUart& uart = SimpleUart::getInstance();
    if (!uart.begin(BT_AUDIO_TX_PIN, BT_AUDIO_RX_PIN, 115200, UART_NUM_2)) {
        ESP_LOGE(TAG, "BT audio UART init failed (TX=%d RX=%d)", BT_AUDIO_TX_PIN,
                 BT_AUDIO_RX_PIN);
        return;
    }
    ESP_LOGD(TAG, "BT audio UART ready (TX=%d RX=%d)", BT_AUDIO_TX_PIN, BT_AUDIO_RX_PIN);

    uart.registerCallback([](const std::vector<uint8_t>& data) {
        // 按可打印字符输出，CR/LF 换成空格，便于串口日志阅读
        std::string line;
        line.reserve(data.size());
        for (uint8_t b : data) {
            if (b == '\r' || b == '\n') {
                line.push_back(' ');
            } else if (b >= 0x20 && b < 0x7F) {
                line.push_back(static_cast<char>(b));
            } else {
                line.push_back('.');
            }
        }
        ESP_LOGD(TAG, "BT RX (%u): %s", static_cast<unsigned>(data.size()), line.c_str());
    });

    // 开机默认蓝牙模式1（接收）
    BluetoothScreen::ApplyDefaultMode();
}

void MetalioPeriph_InitSdCard() {
    // microSD pin2 = CD/DAT3：1-bit 模式下不走数据线，但必须为高，否则卡易进 SPI。
    // ESP32-S3 GPIO46 仅输入，无法推挽拉高，用内部上拉（板级有外拉更好）。
    gpio_config_t dat3_cfg = {
        .pin_bit_mask = BIT64(SDMMC_DAT3_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t dat3_ret = gpio_config(&dat3_cfg);
    if (dat3_ret != ESP_OK) {
        ESP_LOGW(TAG, "SD DAT3/CD GPIO%d pull-up config failed: %s",
                 static_cast<int>(SDMMC_DAT3_PIN), esp_err_to_name(dat3_ret));
    } else {
        ESP_LOGD(TAG, "SD DAT3/CD GPIO%d input+pullup (idle high)",
                 static_cast<int>(SDMMC_DAT3_PIN));
    }

    if (!SdCardManager::GetInstance().Mount()) {
        ESP_LOGW(TAG, "SD card not mounted at boot (card may be absent)");
    } else {
        SdEnsureUploadReadme();
    }
    // 虚拟 U 盘 worker：默认保持 USB Serial/JTAG，设置页启用时再切 MSC。
    UsbVirtualDisk::GetInstance().Init();
}

void MetalioPeriph_InitPcf8563(i2c_master_bus_handle_t bus) {
    auto& rtc = Pcf8563::GetInstance();
    if (!rtc.Begin(bus)) {
        ESP_LOGW(TAG, "PCF8563 init skipped");
        return;
    }
    if (rtc.ApplyRtcToSystem()) {
        ESP_LOGD(TAG, "boot time source: PCF8563 (offline / pre-network)");
    } else {
        ESP_LOGW(TAG, "PCF8563 present but ApplyRtcToSystem failed");
    }
}

void MetalioPeriph_InitCx25601n(i2c_master_bus_handle_t bus) {
    esp_err_t probe = i2c_master_probe(bus, CX25601N_I2C_ADDR, 100);
    if (probe != ESP_OK) {
        ESP_LOGI(TAG, "CX25601N not found at 0x%02X (legacy board?), skip init",
                 CX25601N_I2C_ADDR);
        return;
    }
    esp_err_t err = cx25601n_init(bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CX25601N found at 0x%02X but init failed: %s", CX25601N_I2C_ADDR,
                 esp_err_to_name(err));
        return;
    }
    // 应用设置里保存的充电电流（默认快速充电 1000mA）
    Settings charge_settings("charge");
    int ichg_ma = charge_settings.GetInt("ichg_ma", 1000);
    if (ichg_ma != 500 && ichg_ma != 1000) {
        ichg_ma = 1000;
    }
    err = cx25601n_set_ichg_ma(static_cast<uint32_t>(ichg_ma));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CX25601N set ichg=%d failed: %s", ichg_ma, esp_err_to_name(err));
    } else {
        ESP_LOGD(TAG, "CX25601N init OK at 0x%02X, ichg=%d mA", CX25601N_I2C_ADDR, ichg_ma);
    }
    err = cx25601n_enable_charge(true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CX25601N enable charge failed: %s", esp_err_to_name(err));
    }
}

void MetalioPeriph_StartSystemMonitor() {
    xTaskCreate(
        [](void*) {
            constexpr int kCoreCount = portNUM_PROCESSORS;
            configRUN_TIME_COUNTER_TYPE prev_idle[kCoreCount] = {};
            for (int c = 0; c < kCoreCount; ++c) {
                prev_idle[c] = ulTaskGetIdleRunTimeCounterForCore(c);
            }
            uint64_t prev_us = static_cast<uint64_t>(esp_timer_get_time());
            uint64_t last_loop_end_us = 0;

            while (true) {
                vTaskDelay(pdMS_TO_TICKS(3000));

                const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
                if (last_loop_end_us != 0) {
                    const uint64_t gap_us = now_us - last_loop_end_us;
                    // 期望 ~3000ms；明显偏大说明 sys_mon(pri=1) 长期得不到 CPU
                    if (gap_us > 7500000) {
                        ESP_LOGW("系统监控",
                                 "@@@调度  | sys_mon 实际间隔 %lu ms (期望 ~3000ms)",
                                 static_cast<unsigned long>(gap_us / 1000));
                    }
                }
                const uint64_t dt_us = now_us - prev_us;
                int usage[kCoreCount] = {};
                int total_usage = 0;
                if (dt_us > 0) {
                    for (int c = 0; c < kCoreCount; ++c) {
                        const configRUN_TIME_COUNTER_TYPE now_idle =
                            ulTaskGetIdleRunTimeCounterForCore(c);
                        const configRUN_TIME_COUNTER_TYPE didle = now_idle - prev_idle[c];
                        uint64_t idle_pct = static_cast<uint64_t>(didle) * 100ULL / dt_us;
                        if (idle_pct > 100) {
                            idle_pct = 100;
                        }
                        usage[c] = 100 - static_cast<int>(idle_pct);
                        total_usage += usage[c];
                        prev_idle[c] = now_idle;
                    }
                }
                prev_us = now_us;
                const int avg_usage = (kCoreCount > 0) ? (total_usage / kCoreCount) : 0;
                const int core1_usage = (kCoreCount > 1) ? usage[1] : 0;

                constexpr const char* kMonitorTag = "系统监控";
                ESP_LOGI(kMonitorTag, "@@@CPU   | 内核0: %3d%% | 内核1: %3d%% | 平均: %3d%%",
                         usage[0], core1_usage, avg_usage);

                const unsigned free_kb =
                    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
                const unsigned min_free_kb = static_cast<unsigned>(
                    heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024);
                const unsigned psram_free_kb =
                    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
                const unsigned psram_min_kb = static_cast<unsigned>(
                    heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) / 1024);
                ESP_LOGI(kMonitorTag, "@@@RAM   | 剩余: %6u KB | 历史最小: %6u KB", free_kb,
                         min_free_kb);
                ESP_LOGI(kMonitorTag, "@@@PSRAM | 剩余: %6u KB | 历史最小: %6u KB",
                         psram_free_kb, psram_min_kb);

                auto& gauge = Bq27220Gauge::GetInstance();
                int battery_level = 0;
                bool charging = false;
                bool discharging = false;
                if (gauge.GetBatteryLevel(battery_level, charging, discharging)) {
                    uint16_t mv = 0;
                    if (gauge.GetVoltageMv(mv)) {
                        ESP_LOGI(kMonitorTag,
                                 "@@@电池  | 电量: %3d%% | 电压: %5u mV | "
                                 "充电: %s | 放电: %s",
                                 battery_level, mv, charging ? "是" : "否",
                                 discharging ? "是" : "否");
                    } else {
                        ESP_LOGI(kMonitorTag,
                                 "@@@电池  | 电量: %3d%% | 电压: 读取失败 | "
                                 "充电: %s | 放电: %s",
                                 battery_level, charging ? "是" : "否",
                                 discharging ? "是" : "否");
                    }
                }

                last_loop_end_us = static_cast<uint64_t>(esp_timer_get_time());
            }
        },
        "sys_mon", 4096, nullptr, 1, nullptr);
}
