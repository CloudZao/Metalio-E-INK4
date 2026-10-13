#include "dual_network_board.h"
#include "bt_audio_codec.h"
#include "bq27220_gauge.h"
#include "display/lv_adapter_display.h"
#include "config.h"
#include "metalio_board_periph.h"
#include "metalio_buttons.h"
#include "metalio_display_setup.h"
#include "metalio_epd.h"
#include "metalio_i2c_bus.h"
#include "metalio_io_expander.h"
#include "metalio_touch.h"
#include "metalio_vibe.h"
#include "pcf8563.h"
#include "power_policy.h"
#include "sc7a20h.h"
#include "uvc_still_camera.h"
#include "IOExpander.hpp"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_touch.h>
#include <memory>

#define TAG "MetalioEInk4Board"

// 关机：POWER 长按满 kPowerLongPressMs / 策略超时 → PowerPolicy::RequestPowerOff()
class MetalioEInk4Board : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;    // I2C 总线
    esp_lcd_panel_io_handle_t panel_io_ = nullptr; // 墨水屏 panel IO
    esp_lcd_panel_handle_t panel_ = nullptr;       // 墨水屏 panel
    esp_lcd_touch_handle_t touch_ = nullptr;       // 触摸句柄（可空）
    Display* display_ = nullptr;                   // LVAdapterDisplay
    UvcStillCamera* camera_ = nullptr;             // UVC 拍照

    Button boot_button_;                         // BOOT
    Button power_button_;                        // 电源键
    std::unique_ptr<Button> volume_up_button_;   // 音量+
    std::unique_ptr<Button> volume_down_button_; // 音量-
    MetalioPowerKeyState power_key_;             // 开机残留按住：屏蔽/吞 Click

public:
    MetalioEInk4Board()
        : DualNetworkBoard(NT26_TX_PIN, NT26_RX_PIN, NT26_MRDY_PIN, NT26_SRDY_PIN, 0),
          boot_button_(BOOT_BUTTON_GPIO, false, kBootLongPressMs),
          power_button_(POWER_BUTTON_GPIO, false, kPowerLongPressMs) {
        MetalioI2cBus_Create(&i2c_bus_);
        const bool io_ok = MetalioIoExpander_Init(&i2c_bus_);
        MetalioVibe_Init();
        // BQ27220 挂到 I2C；失败不崩溃，GetBatteryLevel 内会节流自愈重试。
        Bq27220Gauge::GetInstance().Begin(i2c_bus_);
        // SC7A20H 加速度计；未焊接时 probe 失败即可，产测页显示「未检测到」。
        Sc7a20h::GetInstance().Begin(i2c_bus_);
        MetalioPeriph_InitPcf8563(i2c_bus_);
        MetalioPeriph_InitCx25601n(i2c_bus_);
        MetalioPeriph_InitBtAudio();
        MetalioPeriph_InitSdCard();
        MetalioEpd_Init(&panel_io_, &panel_);
        touch_ = nullptr;
        const bool touch_ok = (metalio_touch_init(i2c_bus_, &touch_) == ESP_OK);
        display_ = MetalioDisplay_Create(panel_, panel_io_, touch_);
        if (!touch_ok) {
            ESP_LOGW(TAG, "Touch missing; TouchMissingScreen should be showing");
        }
        if (!io_ok) {
            if (auto* disp = LVAdapterDisplay::Instance()) {
                // 状态栏控件在 HomeScreen Create 时才绑定，先排队
                disp->QueueStatusNotification("IO扩展异常", 60000);
            }
        }
        MetalioButtons_Init(boot_button_, power_button_, volume_up_button_, volume_down_button_,
                            power_key_, this);
        camera_ = new UvcStillCamera();
        MetalioPeriph_StartSystemMonitor();
        PowerPolicy::GetInstance().Init(this);
    }

    virtual Camera* GetCamera() override { return camera_; }

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

    virtual void PulseVibration(int pulse_ms) override { MetalioVibe_Pulse(pulse_ms); }

    virtual void SetVibration(bool on) override { MetalioVibe_Set(on); }

    virtual esp_err_t TouchEnterSleep() override {
        return metalio_touch_enter_sleep(touch_);
    }

    virtual esp_err_t TouchWakeByReset() override {
        auto& io = IOExpander::getInstance();
        if (!io.isInitialized() || io.handle() == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        return metalio_touch_wake_by_reset(io.handle(), MetalioTouch_AfterWake);
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
        ESP_LOGD(TAG, "network time synced to PCF8563; system uses RTC");
    }
};

DECLARE_BOARD(MetalioEInk4Board);
