#include "dual_network_board.h"
#include "bt_audio_codec.h"
#include "bq27220_gauge.h"
#include "display/lv_adapter_display.h"
#include "display/screen/boot_key_handler.h"
#include "display/screen/ota_upgrade_screen/ota_upgrade_screen.h"
#include "display/screen/ota_confirm_dialog/ota_confirm_dialog.h"
#include "display/screen/settings_screen/settings_test/settings_test_aging_screen.h"
#include "display/screen/standby_screen/standby_screen.h"
#include "display/screen/vk_key_handler.h"
#include "haptic_feedback.h"
#include "system_reset.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "IOExpander.hpp"
#include "SdCardManager.hpp"
#include "sd_paths.h"
#include "usb_virtual_disk.h"
#include "uvc_still_camera.h"
#include "SimpleUart.hpp"
#include "display/screen/bluetooth_screen/bluetooth_screen.h"
#include "display/screen/book_screen/book_screen.h"
#include "cx25601n.h"
#include "pcf8563.h"
#include "sc7a20h.h"
#include "settings.h"
#include "esp_lcd_panel_ssd1677.h"
#include "esp_lcd_ssd1677_commands.h"
#include "metalio_touch.h"
#include "power_policy.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_rom_sys.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_touch.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <button_types.h>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define TAG "MetalioEInk4Board"

static esp_timer_handle_t s_vibe_timer = nullptr;
static bool s_vibe_continuous = false;

static void VibeMotorOffTimerCb(void* /*arg*/) {
    if (s_vibe_continuous) {
        return;
    }
    gpio_set_level(VIBRATION_MOTOR_GPIO, 0);
}

static void SetVibrationMotor(bool on) {
    if (s_vibe_timer != nullptr) {
        esp_timer_stop(s_vibe_timer);
    }
    gpio_set_direction(VIBRATION_MOTOR_GPIO, GPIO_MODE_OUTPUT);
    s_vibe_continuous = on;
    gpio_set_level(VIBRATION_MOTOR_GPIO, on ? 1 : 0);
    ESP_LOGI(TAG, "vibe motor continuous=%d", on ? 1 : 0);
}

static void PulseVibrationMotor() {
    if (s_vibe_continuous) {
        return;
    }
    if (s_vibe_timer == nullptr) {
        return;
    }
    // 浅睡后部分脚可能丢输出态；每次脉冲前再钉成推挽输出
    gpio_set_direction(VIBRATION_MOTOR_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(VIBRATION_MOTOR_GPIO, 1);
    esp_timer_stop(s_vibe_timer);
    esp_err_t err = esp_timer_start_once(
        s_vibe_timer, static_cast<uint64_t>(VIBRATION_MOTOR_PULSE_MS) * 1000ULL);
    if (err != ESP_OK) {
        gpio_set_level(VIBRATION_MOTOR_GPIO, 0);
        ESP_LOGW(TAG, "vibe timer start failed: %s", esp_err_to_name(err));
    }
}

static void OnTouchAfterWake(void) {
    if (auto* disp = LVAdapterDisplay::Instance()) {
        disp->KickTouchInput();
    }
}

static bool OnTouchVirtualKeyEvent(const char* name, TouchVkEvent event, void* user_data) {
    (void)user_data;
    if (name == nullptr) {
        return false;
    }
    switch (event) {
        case TouchVkEvent::Click:
            // 短震已在 touch_feed 按下边沿发出，此处勿再震（否则松手连震）
            VkKey_Dispatch(name);
            return false;
        case TouchVkEvent::LongPress: {
            // 长按阈值到达：再震一次区分「短按确认」与「长按动作」
            const bool consumed = VkKey_OnLongPress(name);
            if (consumed) {
                HapticPulseIfEnabled();
            }
            return consumed;
        }
        case TouchVkEvent::PressUp:
            VkKey_OnPressUp(name);
            return false;
    }
    return false;
}

// TCA9555 按键：用 iot_button 自定义驱动轮询 IOExpander 电平（默认低电平有效）。
struct IoExpanderButtonDriver {
    button_driver_t base{};
    IOExpander::Pin pin = IOExpander::Pin::kPinCount;
    uint8_t active_level = 0;
};

static uint8_t IoExpanderButtonGetKeyLevel(button_driver_t* button_driver) {
    auto* self = reinterpret_cast<IoExpanderButtonDriver*>(button_driver);
    uint8_t level = 1;
    if (IOExpander::getInstance().getLevel(self->pin, &level) != ESP_OK) {
        return 0;
    }
    return level == self->active_level ? 1 : 0;
}

static esp_err_t IoExpanderButtonDelete(button_driver_t* button_driver) {
    delete reinterpret_cast<IoExpanderButtonDriver*>(button_driver);
    return ESP_OK;
}

static button_handle_t CreateIoExpanderButton(IOExpander::Pin pin, bool active_high = false) {
    auto* drv = new IoExpanderButtonDriver();
    drv->pin = pin;
    drv->active_level = active_high ? 1 : 0;
    drv->base.enable_power_save = false;
    drv->base.get_key_level = IoExpanderButtonGetKeyLevel;
    drv->base.enter_power_save = nullptr;
    drv->base.del = IoExpanderButtonDelete;

    button_config_t cfg = {};
    button_handle_t handle = nullptr;
    ESP_ERROR_CHECK(iot_button_create(&cfg, &drv->base, &handle));
    return handle;
}

// 关机：POWER 长按满 kPowerLongPressMs / 策略超时 → PowerPolicy::RequestPowerOff()
class MetalioEInk4Board : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    esp_lcd_touch_handle_t touch_ = nullptr;
    Display* display_ = nullptr;
    UvcStillCamera* camera_ = nullptr;

    Button boot_button_;
    Button power_button_;
    std::unique_ptr<Button> volume_up_button_;
    std::unique_ptr<Button> volume_down_button_;

    // 开机残留按住：屏蔽手势直至松手；松手当次 Click 也吞掉（PressUp 常先于 Click）
    bool power_key_suppress_ = false;
    bool power_key_swallow_click_ = false;

    // 从设备半掉电时可能钳住 SDA；控制器 bus_reset 解不开，需 bit-bang 时钟出。
    static void RecoverI2cBusPins() {
        const gpio_config_t conf = {
            .pin_bit_mask = (1ULL << I2C_SDA_PIN) | (1ULL << I2C_SCL_PIN),
            .mode = GPIO_MODE_INPUT_OUTPUT_OD,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&conf);
        gpio_set_level(I2C_SDA_PIN, 1);
        gpio_set_level(I2C_SCL_PIN, 1);
        esp_rom_delay_us(10);

        for (int i = 0; i < 9 && gpio_get_level(I2C_SDA_PIN) == 0; ++i) {
            gpio_set_level(I2C_SCL_PIN, 0);
            esp_rom_delay_us(5);
            gpio_set_level(I2C_SCL_PIN, 1);
            esp_rom_delay_us(5);
        }
        // STOP
        gpio_set_level(I2C_SDA_PIN, 0);
        esp_rom_delay_us(5);
        gpio_set_level(I2C_SCL_PIN, 1);
        esp_rom_delay_us(5);
        gpio_set_level(I2C_SDA_PIN, 1);
        esp_rom_delay_us(5);
    }

    void CreateI2cBus() {
        if (i2c_bus_ != nullptr) {
            i2c_del_master_bus(i2c_bus_);
            i2c_bus_ = nullptr;
        }
        RecoverI2cBusPins();
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = I2C_SDA_PIN,
            .scl_io_num = I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus_));
    }

    // 失败不 abort：无扩展器则音量键/关机脉冲/功放路由等降级，屏起来后状态栏提示。
    bool InitializeIOExpander() {
        auto& io = IOExpander::getInstance();
        esp_err_t err = io.begin(i2c_bus_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "TCA9555 begin failed (%s), recreate I2C bus and retry",
                     esp_err_to_name(err));
            CreateI2cBus();
            vTaskDelay(pdMS_TO_TICKS(50));
            err = io.begin(i2c_bus_);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "TCA9555 unavailable (%s); continue without IO expander",
                     esp_err_to_name(err));
            return false;
        }
        // 先开总电源，再开屏幕卡座供电（begin 后输出锁存默认高，此处显式钉住）
        ESP_ERROR_CHECK(io.setLevel(IOExpander::Pin::MAIN_PWR, true));
        ESP_ERROR_CHECK(io.setLevel(IOExpander::Pin::SCREEN_SOCKET_PWR, true));
        // 功放路由给 ESP32；PA 由 PowerPolicy 按 Speaking/通话 按需开，不在开机强开
        ESP_ERROR_CHECK(io.setLevel(IOExpander::Pin::PA_SWITCH, false));
        ESP_ERROR_CHECK(io.setLevel(IOExpander::Pin::PA, false));
        ESP_ERROR_CHECK(io.setLevel(IOExpander::Pin::PWR_KEY_PULSE, true));
        // USB 默认走烧录/调试通路，拍照时再切到摄像头（见 UsbUvcStill）
        ESP_ERROR_CHECK(io.setLevel(IOExpander::Pin::USB_MUX_SEL, true));
        // 触摸 RST = P1.1(原理图 P11) 上电复位；关机脉冲 = P1.3(P13) 空闲已拉高
        ESP_ERROR_CHECK(metalio_touch_hw_reset_power_on(io.handle()));
        ESP_LOGI(TAG,
                 "MAIN_PWR + SCREEN_SOCKET_PWR on; PA off (policy); USB_MUX=flash; "
                 "TP_RST(P1.1/P11) L->H; PWR_KEY(P1.3/P13)=H");

        // TCA9555 INT → 主控 GPIO2（开漏低有效，内部上拉）
        gpio_config_t int_conf = {};
        int_conf.pin_bit_mask = 1ULL << IO_EXPANDER_INT_GPIO;
        int_conf.mode = GPIO_MODE_INPUT;
        int_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        int_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        int_conf.intr_type = GPIO_INTR_DISABLE;
        esp_err_t int_err = gpio_config(&int_conf);
        if (int_err != ESP_OK) {
            ESP_LOGW(TAG, "IO expander INT GPIO%d config failed: %s",
                     static_cast<int>(IO_EXPANDER_INT_GPIO), esp_err_to_name(int_err));
        } else {
            ESP_LOGI(TAG, "IO expander INT on GPIO%d; ACCEL_INT on TCA9555 P1.4",
                     static_cast<int>(IO_EXPANDER_INT_GPIO));
        }
        return true;
    }

    void InitializeVibrationMotor() {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = 1ULL << VIBRATION_MOTOR_GPIO;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "vibe motor GPIO%d config failed: %s",
                     static_cast<int>(VIBRATION_MOTOR_GPIO), esp_err_to_name(err));
            return;
        }
        gpio_set_level(VIBRATION_MOTOR_GPIO, 0);
        // 浅睡勿接管此脚，避免醒后推挽丢失导致偶发不震
        gpio_sleep_sel_dis(VIBRATION_MOTOR_GPIO);

        const esp_timer_create_args_t timer_args = {
            .callback = &VibeMotorOffTimerCb,
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "vibe_off",
            .skip_unhandled_events = true,
        };
        err = esp_timer_create(&timer_args, &s_vibe_timer);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "vibe timer create failed: %s", esp_err_to_name(err));
            s_vibe_timer = nullptr;
            return;
        }
        ESP_LOGI(TAG, "vibe motor ready GPIO%d pulse=%dms",
                 static_cast<int>(VIBRATION_MOTOR_GPIO), VIBRATION_MOTOR_PULSE_MS);
    }

    void InitializeBTAudio() {
        SimpleUart& uart = SimpleUart::getInstance();
        if (!uart.begin(BT_AUDIO_TX_PIN, BT_AUDIO_RX_PIN, 115200, UART_NUM_2)) {
            ESP_LOGE(TAG, "BT audio UART init failed (TX=%d RX=%d)", BT_AUDIO_TX_PIN,
                     BT_AUDIO_RX_PIN);
            return;
        }
        ESP_LOGI(TAG, "BT audio UART ready (TX=%d RX=%d)", BT_AUDIO_TX_PIN, BT_AUDIO_RX_PIN);

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
            ESP_LOGI(TAG, "BT RX (%u): %s", static_cast<unsigned>(data.size()), line.c_str());
        });

        // 开机默认蓝牙模式1（接收）
        BluetoothScreen::ApplyDefaultMode();
    }

    #if 0
    // ponytail: 全刷 SPI 单测用；测完改 0 恢复正常开机。1=白 0=黑，与 VRAM/LVGL I1 一致。
    static void FbSetPixel(uint8_t* fb, int w, int h, int x, int y, bool white) {
        if (x < 0 || y < 0 || x >= w || y >= h) {
            return;
        }
        uint8_t* p = &fb[y * (w / 8) + x / 8];
        const uint8_t mask = static_cast<uint8_t>(1u << (7 - (x & 7)));
        if (white) {
            *p |= mask;
        } else {
            *p &= static_cast<uint8_t>(~mask);
        }
    }

    static void FbFillRect(uint8_t* fb, int w, int h, int x0, int y0, int x1, int y1, bool white) {
        if (x0 > x1) {
            const int t = x0;
            x0 = x1;
            x1 = t;
        }
        if (y0 > y1) {
            const int t = y0;
            y0 = y1;
            y1 = t;
        }
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                FbSetPixel(fb, w, h, x, y, white);
            }
        }
    }

    /** 格内粗线几何黑字 A（约占格子 80%）。 */
    static void StampBigBlackAInCell(uint8_t* fb, int w, int h, int x0, int y0, int cell) {
        const int m = cell / 10;
        const int top = y0 + m;
        const int bot = y0 + cell - 1 - m;
        const int left = x0 + m;
        const int right = x0 + cell - 1 - m;
        const int cx = x0 + cell / 2;
        const int bar_y = y0 + (cell * 55) / 100;
        const int thick = (cell / 10) | 1; // 奇数线宽，至少 1
        const int half = thick / 2;

        auto plot = [&](int x, int y) {
            FbFillRect(fb, w, h, x - half, y - half, x + half, y + half, false);
        };

        // 左斜边：顶中 → 左下
        {
            const int dy = bot - top;
            const int dx = left - cx;
            for (int i = 0; i <= dy; i++) {
                const int y = top + i;
                const int x = cx + (dx * i) / dy;
                plot(x, y);
            }
        }
        // 右斜边：顶中 → 右下
        {
            const int dy = bot - top;
            const int dx = right - cx;
            for (int i = 0; i <= dy; i++) {
                const int y = top + i;
                const int x = cx + (dx * i) / dy;
                plot(x, y);
            }
        }
        // 横杠
        FbFillRect(fb, w, h, left + cell / 8, bar_y - half, right - cell / 8, bar_y + half, false);
    }

    /** 仅白格画黑字 A；黑格保持空白。 */
    static void StampLetterAInWhiteCells(uint8_t* fb, int w, int h, int cell, bool invert) {
        for (int cy = 0; cy * cell < h; cy++) {
            for (int cx = 0; cx * cell < w; cx++) {
                bool cell_white = ((cx + cy) & 1) != 0;
                if (invert) {
                    cell_white = !cell_white;
                }
                if (!cell_white) {
                    continue;
                }
                StampBigBlackAInCell(fb, w, h, cx * cell, cy * cell, cell);
            }
        }
    }

    static void FillCheckerboard(uint8_t* fb, int w, int h, int cell, bool invert) {
        const int stride = w / 8;
        for (int y = 0; y < h; y++) {
            for (int bx = 0; bx < stride; bx++) {
                uint8_t byte = 0;
                for (int bit = 0; bit < 8; bit++) {
                    const int x = bx * 8 + bit;
                    bool white = (((x / cell) + (y / cell)) & 1) != 0;
                    if (invert) {
                        white = !white;
                    }
                    if (white) {
                        byte |= static_cast<uint8_t>(1u << (7 - bit));
                    }
                }
                fb[y * stride + bx] = byte;
            }
        }
        StampLetterAInWhiteCells(fb, w, h, cell, invert);
    }

    /** 上电 init 后：150px 棋盘（白格黑字 A）全刷；每 10s 黑白反相再全刷；不启 LVGL/业务。 */
    void EpdCheckerboardFullRefreshThenHalt() {
        constexpr int kCellPx = 150;
        constexpr int kInvertPeriodMs = 10000;
        ESP_LOGW(TAG, "EPD SPI test: checkerboard %dpx +A FULL, invert every %dms", kCellPx,
                 kInvertPeriodMs);

        uint8_t* fb = static_cast<uint8_t*>(
            heap_caps_malloc(SSD1677_PANEL_BUFFER_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        if (fb == nullptr) {
            fb = static_cast<uint8_t*>(heap_caps_malloc(SSD1677_PANEL_BUFFER_SIZE, MALLOC_CAP_DEFAULT));
        }
        ESP_ERROR_CHECK(fb ? ESP_OK : ESP_ERR_NO_MEM);

        bool invert = false;
        while (true) {
            FillCheckerboard(fb, DISPLAY_WIDTH, DISPLAY_HEIGHT, kCellPx, invert);
            ESP_LOGI(TAG, "EPD SPI test: FULL checkerboard invert=%d", invert ? 1 : 0);

            // 与 FullRefreshBothSame 同序：0x26=0x24 → FULL 0xC7 → BUSY
            epaper_panel_set_bitmap_color(panel_, SSD1677_EPAPER_BITMAP_PREVIOUS);
            ESP_ERROR_CHECK(
                esp_lcd_panel_draw_bitmap(panel_, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, fb));
            epaper_panel_set_bitmap_color(panel_, SSD1677_EPAPER_BITMAP_CURRENT);
            ESP_ERROR_CHECK(
                esp_lcd_panel_draw_bitmap(panel_, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, fb));
            epaper_panel_set_refresh_mode(panel_, SSD1677_EPAPER_REFRESH_FULL);
            ESP_ERROR_CHECK(epaper_panel_refresh_screen(panel_));

            invert = !invert;
            vTaskDelay(pdMS_TO_TICKS(kInvertPeriodMs));
        }
    }
    #endif

    void InitializeSsd1677() {
        spi_bus_config_t buscfg = {
            .mosi_io_num = EPD_PIN_MOSI,
            .miso_io_num = -1,
            .sclk_io_num = EPD_PIN_SCLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = SSD1677_PANEL_BUFFER_SIZE + 8,
        };
        ESP_LOGI(TAG, "SSD1677: spi_bus_initialize...");
        ESP_ERROR_CHECK(spi_bus_initialize(EPD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
        ESP_LOGI(TAG, "SSD1677: spi_bus_initialize ok");

        esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = EPD_PIN_CS,
            .dc_gpio_num = EPD_PIN_DC,
            .spi_mode = 0,
            .pclk_hz = EPD_SPI_CLK_HZ,
            .trans_queue_depth = 10,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EPD_SPI_HOST, &io_config,
                                                 &panel_io_));

        esp_lcd_ssd1677_config_t ssd1677_cfg = {
            .busy_gpio_num = EPD_PIN_BUSY,
            .non_copy_mode = true,
            .use_fast_full_update = true,
        };
        esp_lcd_panel_dev_config_t panel_cfg = {
            .reset_gpio_num = EPD_PIN_RST,
            .flags =
                {
                    .reset_active_high = false,
                },
            .vendor_config = &ssd1677_cfg,
        };

        // BUSY 脚 ISR 需要 GPIO ISR service
        ESP_LOGI(TAG, "SSD1677: gpio_install_isr_service...");
        esp_err_t isr_ret = gpio_install_isr_service(0);
        if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(isr_ret);
        }
        ESP_LOGI(TAG, "SSD1677: gpio_install_isr_service ok (%s)", esp_err_to_name(isr_ret));

        ESP_LOGI(TAG, "SSD1677: new_panel...");
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1677(panel_io_, &panel_cfg, &panel_));
        ESP_LOGI(TAG, "SSD1677: new_panel ok");

        ESP_LOGI(TAG, "SSD1677: panel_reset...");
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "SSD1677: panel_reset ok");

        ESP_LOGI(TAG, "SSD1677: panel_init...");
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "SSD1677 ready %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);

#if 0 // EPD 全刷 SPI 单测：改 0 恢复正常产品流程
        EpdCheckerboardFullRefreshThenHalt();
#endif
    }

    // 返回 true 表示触摸可用；检测不到时不崩溃，touch_ 保持 nullptr。
    bool InitializeTouch() {
        touch_ = nullptr;
        return metalio_touch_init(i2c_bus_, &touch_) == ESP_OK;
    }

    void InitializeDisplay() {
        display_ = new LVAdapterDisplay(panel_, panel_io_, touch_, DISPLAY_WIDTH, DISPLAY_HEIGHT);

        if (touch_ == nullptr) {
            return;
        }

        static const TouchVirtualKey kTouchKeys[] = {
            // HOME：与 BOOT 同阈值 kBootLongPressMs(500)；长按→回系统首页（含百问页）
            {"vk_home", TOUCH_VK_HOME_X, TOUCH_VK_HOME_Y, kBootLongPressMs},
            // prev/next：百问长按→首页/末页；其它页未消费则松手仍走短按
            {"vk_prev", TOUCH_VK_PREV_X, TOUCH_VK_PREV_Y, kBootLongPressMs},
            {"vk_next", TOUCH_VK_NEXT_X, TOUCH_VK_NEXT_Y, kBootLongPressMs},
        };
        if (auto* disp = LVAdapterDisplay::Instance()) {
            disp->RegisterTouchVirtualKeys(kTouchKeys, sizeof(kTouchKeys) / sizeof(kTouchKeys[0]),
                                           OnTouchVirtualKeyEvent, nullptr);
        }
    }

    void InitializeButtons() {
        // 注册回调时若仍按住，屏蔽至首次松开，避免上电残留长按关机 / 松手误进待机
        power_key_suppress_ = (gpio_get_level(POWER_BUTTON_GPIO) == 0);
        power_key_swallow_click_ = false;
        if (power_key_suppress_) {
            ESP_LOGW(TAG, "POWER held at button init (GPIO%d), suppress until release",
                     static_cast<int>(POWER_BUTTON_GPIO));
        }

        // BOOT(IO0)：按下/松开/短按/双击/长按经 BootKey_* 查当前页 VkKey 注册表分发
        boot_button_.OnPressDown([]() {
            ESP_LOGI(TAG, "按键按下: BOOT (GPIO%d)", static_cast<int>(BOOT_BUTTON_GPIO));
            BootKey_OnPressDown();
        });
        boot_button_.OnPressUp([]() {
            ESP_LOGI(TAG, "按键松开: BOOT (GPIO%d)", static_cast<int>(BOOT_BUTTON_GPIO));
            BootKey_OnPressUp();
        });
        boot_button_.OnClick([]() {
            ESP_LOGI(TAG, "按键点击: BOOT (GPIO%d)", static_cast<int>(BOOT_BUTTON_GPIO));
            BootKey_OnClick();
        });
        boot_button_.OnDoubleClick([]() {
            ESP_LOGI(TAG, "按键双击: BOOT (GPIO%d)", static_cast<int>(BOOT_BUTTON_GPIO));
            BootKey_OnDoubleClick();
        });
        boot_button_.OnLongPress([]() {
            ESP_LOGI(TAG, "按键长按 %ums: BOOT (GPIO%d)",
                     static_cast<unsigned>(kBootLongPressMs),
                     static_cast<int>(BOOT_BUTTON_GPIO));
            BootKey_OnLongPress();
        });
        power_button_.OnPressDown([this]() {
            if (power_key_suppress_) {
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
        power_button_.OnPressUp([this]() {
            if (power_key_suppress_) {
                power_key_suppress_ = false;
                power_key_swallow_click_ = true;
                ESP_LOGI(TAG, "POWER released after boot hold, gestures armed");
            }
        });
        power_button_.OnClick([this]() {
            if (power_key_suppress_) {
                return;
            }
            if (power_key_swallow_click_) {
                power_key_swallow_click_ = false;
                ESP_LOGI(TAG, "POWER click swallowed (boot-hold release)");
                return;
            }
            ESP_LOGI(TAG, "按键点击: POWER 进/出待机 (GPIO%d)",
                     static_cast<int>(POWER_BUTTON_GPIO));
            PowerKey_OnClick();
        });
        power_button_.OnLongPress([this]() {
            if (power_key_suppress_) {
                ESP_LOGI(TAG, "POWER long-press suppressed (boot hold)");
                return;
            }
            ESP_LOGI(TAG, "按键长按 %ums: POWER (GPIO%d)",
                     static_cast<unsigned>(kPowerLongPressMs),
                     static_cast<int>(POWER_BUTTON_GPIO));
            PowerKey_OnLongPress();
        });

        // 音量键挂在 TCA9555，需在 IOExpander begin 成功后创建
        if (!IOExpander::getInstance().isInitialized()) {
            ESP_LOGW(TAG, "skip volume buttons: IOExpander not ready");
        } else {
            volume_down_button_ =
                std::make_unique<Button>(CreateIoExpanderButton(IOExpander::Pin::VOLUME_DOWN));
            volume_up_button_ =
                std::make_unique<Button>(CreateIoExpanderButton(IOExpander::Pin::VOLUME_UP));

            // 正文阅读：音量+ = 上一页，音量- = 下一页（含长按连翻）；其它页仍调音量
            volume_up_button_->OnClick([this]() {
                if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
                    ESP_LOGI(TAG, "VOLUME_UP blocked (ota upgrade)");
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
                auto* codec = GetAudioCodec();
                int volume = codec->output_volume() + 10;
                if (volume > 100) {
                    volume = 100;
                }
                codec->SetOutputVolume(volume);
                ESP_LOGI(TAG, "VOLUME_UP (P1.0): volume=%d", volume);
                GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
            });
            volume_up_button_->OnLongPress([this]() {
                if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
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
                GetAudioCodec()->SetOutputVolume(100);
                ESP_LOGI(TAG, "VOLUME_UP long: volume=100");
                GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
            });
            volume_up_button_->OnPressUp([]() {
                if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
                    return;
                }
                if (SettingsTestAgingScreen::IsActive()) {
                    return;
                }
                if (BookScreen::IsReadingActive()) {
                    VkKey_OnPressUp("vk_prev");
                }
            });

            volume_down_button_->OnClick([this]() {
                if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
                    ESP_LOGI(TAG, "VOLUME_DOWN blocked (ota upgrade)");
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
                auto* codec = GetAudioCodec();
                int volume = codec->output_volume() - 10;
                if (volume < 0) {
                    volume = 0;
                }
                codec->SetOutputVolume(volume);
                ESP_LOGI(TAG, "VOLUME_DOWN (P0.7): volume=%d", volume);
                GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
            });
            volume_down_button_->OnLongPress([this]() {
                if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
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
                GetAudioCodec()->SetOutputVolume(0);
                ESP_LOGI(TAG, "VOLUME_DOWN long: volume=0 (muted)");
                GetDisplay()->ShowNotification(Lang::Strings::MUTED);
            });
            volume_down_button_->OnPressUp([]() {
                if (OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive()) {
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

        ESP_LOGI(TAG,
                 "Buttons ready: BOOT=GPIO%d POWER=GPIO%d VOL-=P0.7 VOL+=P1.0",
                 static_cast<int>(BOOT_BUTTON_GPIO), static_cast<int>(POWER_BUTTON_GPIO));
    }

    // PCF8563 RTC：开机先灌芯片时间到系统；联网对时成功后再回写芯片。
    void InitializePcf8563() {
        auto& rtc = Pcf8563::GetInstance();
        if (!rtc.Begin(i2c_bus_)) {
            ESP_LOGW(TAG, "PCF8563 init skipped");
            return;
        }
        if (rtc.ApplyRtcToSystem()) {
            ESP_LOGI(TAG, "boot time source: PCF8563 (offline / pre-network)");
        } else {
            ESP_LOGW(TAG, "PCF8563 present but ApplyRtcToSystem failed");
        }
    }

    // CX25601N 充电 IC（I2C 0x6B）。老设备无此芯片，probe 失败则跳过。
    void InitializeCx25601n() {
        esp_err_t probe = i2c_master_probe(i2c_bus_, CX25601N_I2C_ADDR, 100);
        if (probe != ESP_OK) {
            ESP_LOGI(TAG, "CX25601N not found at 0x%02X (legacy board?), skip init",
                     CX25601N_I2C_ADDR);
            return;
        }
        esp_err_t err = cx25601n_init(i2c_bus_);
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
            ESP_LOGI(TAG, "CX25601N init OK at 0x%02X, ichg=%d mA", CX25601N_I2C_ADDR,
                     ichg_ma);
        }
        err = cx25601n_enable_charge(true);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "CX25601N enable charge failed: %s", esp_err_to_name(err));
        }
    }

    // 开机把 SD 卡挂到 /sdcard。失败不致命（卡没插 / 没格式化都会失败）。
    void InitializeSdCard() {
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
            ESP_LOGI(TAG, "SD DAT3/CD GPIO%d input+pullup (idle high)",
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

    // 每秒打印双核 CPU 占用与内部 SRAM 剩余（依赖
    // CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS + USE_TRACE_FACILITY +
    // RUN_TIME_STATS_USING_ESP_TIMER）。
    void StartSystemMonitor() {
        xTaskCreate(
            [](void*) {
                constexpr int kCoreCount = portNUM_PROCESSORS;
                configRUN_TIME_COUNTER_TYPE prev_idle[kCoreCount] = {};
                for (int c = 0; c < kCoreCount; ++c) {
                    prev_idle[c] = ulTaskGetIdleRunTimeCounterForCore(c);
                }
                uint64_t prev_us = static_cast<uint64_t>(esp_timer_get_time());
                // int cx25601n_log_tick = 0;

                while (true) {
                    vTaskDelay(pdMS_TO_TICKS(1000));

                    const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
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
                    ESP_LOGI(kMonitorTag,
                             "@@@CPU   | 内核0: %3d%% | 内核1: %3d%% | 平均: %3d%%",
                             usage[0], core1_usage, avg_usage);

                    const unsigned free_kb = static_cast<unsigned>(
                        heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
                    const unsigned min_free_kb = static_cast<unsigned>(
                        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024);
                    const unsigned psram_free_kb = static_cast<unsigned>(
                        heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
                    const unsigned psram_min_kb = static_cast<unsigned>(
                        heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) / 1024);
                    ESP_LOGI(kMonitorTag,
                             "@@@RAM   | 剩余: %6u KB | 历史最小: %6u KB",
                             free_kb, min_free_kb);
                    ESP_LOGI(kMonitorTag,
                             "@@@PSRAM | 剩余: %6u KB | 历史最小: %6u KB",
                             psram_free_kb, psram_min_kb);

                    // ---- 电池电量（BQ27220）----
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

                    // ---- CX25601N VREG 寄存器（每 5s，便于核对恒压目标）----
                    // {
                    //     if (++cx25601n_log_tick >= 5) {
                    //         cx25601n_log_tick = 0;
                    //         if (!cx25601n_is_ready()) {
                    //             ESP_LOGI(kMonitorTag, "@@@CX25601N | 未初始化或未检测到芯片");
                    //         } else {
                    //             uint8_t reg04 = 0;
                    //             uint8_t reg05 = 0;
                    //             uint8_t reg16 = 0;
                    //             uint32_t vreg_mv = 0;
                    //             uint8_t chrg_stat = 0;
                    //             uint8_t vbus_stat = 0;
                    //             bool en_chg = false;
                    //
                    //             const esp_err_t err04 = cx25601n_read_reg(0x04, &reg04);
                    //             const esp_err_t err05 = cx25601n_read_reg(0x05, &reg05);
                    //             const esp_err_t err16 = cx25601n_read_reg(0x16, &reg16);
                    //             const esp_err_t err_vreg = cx25601n_get_vreg_mv(&vreg_mv);
                    //             const esp_err_t err_chrg = cx25601n_get_chrg_stat(&chrg_stat);
                    //             const esp_err_t err_vbus = cx25601n_get_vbus_stat(&vbus_stat);
                    //             const esp_err_t err_en = cx25601n_is_charge_enabled(&en_chg);
                    //
                    //             if (err04 != ESP_OK || err05 != ESP_OK || err_vreg != ESP_OK) {
                    //                 ESP_LOGW(kMonitorTag,
                    //                          "@@@CX25601N | VREG 读取失败 | REG0x04=%s REG0x05=%s "
                    //                          "get_vreg=%s",
                    //                          esp_err_to_name(err04), esp_err_to_name(err05),
                    //                          esp_err_to_name(err_vreg));
                    //             } else {
                    //                 const uint8_t vreg_lo =
                    //                     static_cast<uint8_t>((reg04 >> 3) & 0x1F);
                    //                 const uint8_t vreg_hi = static_cast<uint8_t>(reg05 & 0x0F);
                    //                 const uint32_t code =
                    //                     static_cast<uint32_t>(vreg_lo) |
                    //                     (static_cast<uint32_t>(vreg_hi) << 5);
                    //                 const uint16_t reg_le =
                    //                     static_cast<uint16_t>(reg04) |
                    //                     (static_cast<uint16_t>(reg05) << 8);
                    //
                    //                 ESP_LOGI(kMonitorTag,
                    //                          "@@@CX25601N | VREG=%lu mV | code=%lu (0x%03lX) | "
                    //                          "REG0x04=0x%02X REG0x05=0x%02X | "
                    //                          "VREG[4:0]=%u@0x04[7:3] VREG[8:5]=%u@0x05[3:0] | "
                    //                          "16bit_LE=0x%04X | EN_CHG=%s(%s) | CHG=%s(%s) | "
                    //                          "VBUS=%s(%s)",
                    //                          static_cast<unsigned long>(vreg_mv),
                    //                          static_cast<unsigned long>(code),
                    //                          static_cast<unsigned long>(code),
                    //                          reg04, reg05, vreg_lo, vreg_hi, reg_le,
                    //                          en_chg ? "开" : "关",
                    //                          err_en == ESP_OK ? "OK" : esp_err_to_name(err_en),
                    //                          cx25601n_chrg_stat_str(chrg_stat),
                    //                          err_chrg == ESP_OK ? "OK" : esp_err_to_name(err_chrg),
                    //                          cx25601n_vbus_stat_str(vbus_stat),
                    //                          err_vbus == ESP_OK ? "OK" : esp_err_to_name(err_vbus));
                    //                 if (err16 == ESP_OK) {
                    //                     ESP_LOGI(kMonitorTag,
                    //                              "@@@CX25601N | REG0x16=0x%02X | EN_CHG(bit5)=%u "
                    //                              "EN_HIZ(bit4)=%u WDT[1:0]=%u",
                    //                              reg16, (reg16 >> 5) & 1, (reg16 >> 4) & 1,
                    //                              reg16 & 0x03);
                    //                 }
                    //             }
                    //         }
                    //     }
                    // }

                    // ---- 网络信号（参考 metalio-claw-4 系统监控）----
                    // WiFi → RSSI；4G(NT26) → CSQ。已连通才打数值，避免未就绪时刷无效值。
                    // {
                    //     auto& dual = static_cast<DualNetworkBoard&>(Board::GetInstance());
                    //     const NetworkType net_type = dual.GetNetworkType();
                    //     if (net_type == NetworkType::WIFI) {
                    //         auto& wifi = WifiStation::GetInstance();
                    //         if (wifi.IsConnected()) {
                    //             ESP_LOGI(kMonitorTag,
                    //                      "@@@信号  | 网络: WiFi | RSSI: %d dBm",
                    //                      static_cast<int>(wifi.GetRssi()));
                    //         } else {
                    //             ESP_LOGI(kMonitorTag, "@@@信号  | 网络: WiFi | 未连接");
                    //         }
                    //     } else {
                    //         auto& nt26 = static_cast<Nt26Board&>(dual.GetCurrentBoard());
                    //         const int csq = nt26.GetSignalStrength();
                    //         if (csq == 99 || csq < 0) {
                    //             ESP_LOGI(kMonitorTag, "@@@信号  | 网络: 4G | CSQ: 未知");
                    //         } else {
                    //             ESP_LOGI(kMonitorTag, "@@@信号  | 网络: 4G | CSQ: %2d", csq);
                    //         }
                    //     }
                    // }
                }
            },
            "sys_mon", 4096, nullptr, 1, nullptr);
    }

public:
    MetalioEInk4Board()
        : DualNetworkBoard(NT26_TX_PIN, NT26_RX_PIN, NT26_MRDY_PIN, NT26_SRDY_PIN, 0),
          boot_button_(BOOT_BUTTON_GPIO, false, kBootLongPressMs),
          power_button_(POWER_BUTTON_GPIO, false, kPowerLongPressMs) {
#if 0 // EPD 全刷 SPI 单测：只起屏，其它外设/业务全跳过
        InitializeSsd1677(); // 内部棋盘全刷后挂死，不会返回
#else
        CreateI2cBus();
        const bool io_ok = InitializeIOExpander();
        InitializeVibrationMotor();
        // BQ27220 挂到 I2C；失败不崩溃，GetBatteryLevel 内会节流自愈重试。
        (void)Bq27220Gauge::GetInstance().Begin(i2c_bus_);
        // SC7A20H 加速度计；未焊接时 probe 失败即可，产测页显示「未检测到」。
        (void)Sc7a20h::GetInstance().Begin(i2c_bus_);
        InitializePcf8563();
        InitializeCx25601n();
        InitializeBTAudio();
        InitializeSdCard();
        InitializeSsd1677();
        const bool touch_ok = InitializeTouch();
        InitializeDisplay();
        if (!touch_ok) {
            ESP_LOGW(TAG, "Touch missing; TouchMissingScreen should be showing");
        }
        if (!io_ok) {
            if (auto* disp = LVAdapterDisplay::Instance()) {
                // 状态栏控件在 HomeScreen Create 时才绑定，先排队
                disp->QueueStatusNotification("IO扩展异常", 60000);
            }
        }
        InitializeButtons();
        camera_ = new UvcStillCamera();
        StartSystemMonitor();
        PowerPolicy::GetInstance().Init(this);
#endif
    }

    virtual Camera* GetCamera() override { return camera_; }

    virtual Led* GetLed() override {
#if BUILTIN_LED_GPIO != GPIO_NUM_NC
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
#else
        static NoLed led;
        return &led;
#endif
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BTAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_MIC_GPIO_WS,
                                              AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    // 转发到 Bq27220Gauge；通知栏 UpdateStatusBar 会按 level/充电态刷新电池图标。
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        return Bq27220Gauge::GetInstance().GetBatteryLevel(level, charging, discharging);
    }

    virtual void PulseVibration() override { PulseVibrationMotor(); }

    virtual void SetVibration(bool on) override { SetVibrationMotor(on); }

    virtual esp_err_t TouchEnterSleep() override {
        return metalio_touch_enter_sleep(touch_);
    }

    virtual esp_err_t TouchWakeByReset() override {
        auto& io = IOExpander::getInstance();
        if (!io.isInitialized() || io.handle() == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        return metalio_touch_wake_by_reset(io.handle(), OnTouchAfterWake);
    }

    // 联网且 OTA 写入 server_time 后：系统时间 → RTC → 再读回系统，以芯片为准。
    virtual void OnNetworkTimeSynced() override {
        auto& rtc = Pcf8563::GetInstance();
        if (!rtc.IsReady()) {
            ESP_LOGW(TAG, "OnNetworkTimeSynced: PCF8563 not ready");
            return;
        }
        if (!rtc.SyncSystemToRtc()) {
            ESP_LOGW(TAG, "OnNetworkTimeSynced: SyncSystemToRtc failed");
            return;
        }
        if (!rtc.ApplyRtcToSystem()) {
            ESP_LOGW(TAG, "OnNetworkTimeSynced: ApplyRtcToSystem failed");
            return;
        }
        ESP_LOGI(TAG, "network time synced to PCF8563; system uses RTC");
    }
};

DECLARE_BOARD(MetalioEInk4Board);
