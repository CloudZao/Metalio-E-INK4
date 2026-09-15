#include "bq27220_gauge.h"

#include <esp_log.h>
#include <esp_timer.h>

#define TAG "Bq27220Gauge"

namespace {
// BQ27220 standard 寄存器（参考 TI 数据手册 Table 2-1 Standard Commands）。
// 读取均为 little-endian uint16。
constexpr uint8_t  kRegVoltage     = 0x08;  // mV
constexpr uint8_t  kRegCurrent     = 0x0C;  // int16, mA (+ 充电 / - 放电)
constexpr uint32_t kI2cSpeedHz     = 400 * 1000;  // TI Fast Mode，上限 400 kHz
constexpr int      kI2cTimeoutMs   = 50;
constexpr int      kProbeTimeoutMs = 50;

// 电池电压 → SOC 线性内插（不依赖未标定的 SOC 寄存器）。
// 显示满电按 4.28V=100%（静置开路）；充电芯片 VREG 仍为 4.35V。
constexpr float kBatteryEmptyV = 3.3f;
constexpr float kBatteryFullV  = 4.28f;
}  // namespace

bool Bq27220Gauge::Begin(i2c_master_bus_handle_t bus, uint8_t addr) {
    if (bus == nullptr) {
        ESP_LOGW(TAG, "Begin() called with null bus");
        return false;
    }
    bus_  = bus;
    addr_ = addr;
    if (dev_ != nullptr) {
        return true;
    }

    esp_err_t probe = i2c_master_probe(bus_, addr_, kProbeTimeoutMs);
    if (probe != ESP_OK) {
        // 仅首次开机时打一条提示，避免 1Hz 自愈重试刷屏。
        static bool warned = false;
        if (!warned) {
            ESP_LOGW(TAG,
                     "BQ27220 @0x%02X probe NACK (err=0x%x)，"
                     "暂不显示电量；插上电池/上电后会自动重试",
                     addr_, probe);
            warned = true;
        }
        return false;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr_,
        .scl_speed_hz    = kI2cSpeedHz,
        .scl_wait_us     = 0,
        .flags           = {.disable_ack_check = 0},
    };
    esp_err_t err = i2c_master_bus_add_device(bus_, &cfg, &dev_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2c_master_bus_add_device(BQ27220) failed: 0x%x", err);
        dev_ = nullptr;
        return false;
    }

    uint16_t mv = 0;
    if (ReadU16(kRegVoltage, &mv)) {
        ESP_LOGI(TAG, "BQ27220 online @0x%02X, voltage=%u mV", addr_, mv);
    } else {
        ESP_LOGW(TAG, "BQ27220 ACK 通过但读电压失败，先保留 device 句柄");
    }
    return true;
}

bool Bq27220Gauge::ReadVoltageMv(uint16_t& mv) {
    if (dev_ == nullptr) return false;
    return ReadU16(kRegVoltage, &mv);
}

bool Bq27220Gauge::ReadCurrentMa(int16_t& current_ma) {
    if (dev_ == nullptr) return false;
    uint16_t raw = 0;
    if (!ReadU16(kRegCurrent, &raw)) return false;
    current_ma = static_cast<int16_t>(raw);
    return true;
}

int Bq27220Gauge::ApplyMonoStep(int target_pct, bool charging) {
    if (target_pct < 0) {
        target_pct = 0;
    }
    if (target_pct > 100) {
        target_pct = 100;
    }

    const int64_t now = esp_timer_get_time();
    if (displayed_soc_ < 0) {
        displayed_soc_ = target_pct;
        last_soc_step_us_ = now;
        return displayed_soc_;
    }

    if (now - last_soc_step_us_ < kSocStepUs) {
        return displayed_soc_;
    }

    if (charging) {
        // 充电中：只许升，挡住插电 IR 虚高后又回落
        if (target_pct > displayed_soc_) {
            ++displayed_soc_;
            last_soc_step_us_ = now;
        }
    } else {
        // 未充电：只许降，避免空载回弹假回血
        if (target_pct < displayed_soc_) {
            --displayed_soc_;
            last_soc_step_us_ = now;
        }
    }
    return displayed_soc_;
}

bool Bq27220Gauge::GetBatteryLevel(int& level, bool& charging, bool& discharging) {
    if (dev_ == nullptr) {
        // 没挂上则节流自愈重试（~10s/次），避免 1Hz 走 i2c_master_probe
        // 的 timeout 拖慢调用方。
        if (++retry_counter_ % 10 != 1) {
            return false;
        }
        if (bus_ == nullptr || !Begin(bus_, addr_)) {
            return false;
        }
    }

    uint16_t mv = 0;
    if (!ReadU16(kRegVoltage, &mv)) {
        return false;
    }
    float bat_v = static_cast<float>(mv) / 1000.0f;

    float raw_pct;
    if (bat_v >= kBatteryFullV) {
        raw_pct = 100.0f;
    } else if (bat_v <= kBatteryEmptyV) {
        raw_pct = 0.0f;
    } else {
        raw_pct = (bat_v - kBatteryEmptyV) /
                  (kBatteryFullV - kBatteryEmptyV) * 100.0f;
    }
    float smoothed = FilterPush(raw_pct);
    int target = static_cast<int>(smoothed + 0.5f);
    if (target < 0) {
        target = 0;
    }
    if (target > 100) {
        target = 100;
    }

    // ≥4280mV 需连续满 30s 才允许目标到 100%
    const int64_t now = esp_timer_get_time();
    bool full_ok = false;
    if (mv >= kFullConfirmMv) {
        if (full_above_since_us_ == 0) {
            full_above_since_us_ = now;
        }
        full_ok = (now - full_above_since_us_ >= kFullHoldUs);
    } else {
        full_above_since_us_ = 0;
    }
    if (target >= 100 && !full_ok) {
        target = 99;
    }
    // 已显示 100%：电压未跌破滞回前保持目标 100，避免 4278↔4280 来回掉格
    if (displayed_soc_ >= 100 && mv >= kFullExitMv) {
        target = 100;
    }

    int16_t current_ma = 0;
    (void)ReadCurrentMa(current_ma);   // 失败时按 0 mA 处理
    const bool raw_charging = (current_ma > 5);
    discharging = (current_ma < -5);
    const int charge_dir = raw_charging ? 1 : (discharging ? -1 : 0);

    // 插拔电 / 充放方向变：重开 30s 步进计时，避免计时已到期时立刻 ±1%
    if (displayed_soc_ >= 0 && charge_dir != last_charge_dir_) {
        last_soc_step_us_ = now;
        last_charge_dir_ = charge_dir;
    } else if (displayed_soc_ < 0) {
        last_charge_dir_ = charge_dir;
    }

    // 单向步进用真实充放电方向；UI 充电标志在满电时再清掉
    level = ApplyMonoStep(target, raw_charging);
    if (full_ok && target >= 100 && displayed_soc_ >= 99) {
        displayed_soc_ = 100;
        level = 100;
    }
    charging = raw_charging && (level < 100);
    return true;
}

void Bq27220Gauge::ResetFilter() {
    filter_idx_     = 0;
    filter_count_   = 0;
    filter_sum_     = 0.0f;
    filter_primed_  = false;
    displayed_soc_  = -1;
    last_soc_step_us_ = 0;
    full_above_since_us_ = 0;
    last_charge_dir_ = 0;
}

bool Bq27220Gauge::ReadU16(uint8_t reg, uint16_t* out) {
    if (dev_ == nullptr || out == nullptr) return false;
    uint8_t buf[2] = {0};
    esp_err_t err = i2c_master_transmit_receive(dev_, &reg, 1, buf,
                                                sizeof(buf), kI2cTimeoutMs);
    if (err != ESP_OK) {
        if (++consecutive_err_ % 10 == 1) {
            ESP_LOGW(TAG, "BQ27220 read reg 0x%02X failed: 0x%x "
                          "(consecutive=%d)",
                     reg, err, consecutive_err_);
        }
        return false;
    }
    consecutive_err_ = 0;
    *out = static_cast<uint16_t>(buf[0]) |
           (static_cast<uint16_t>(buf[1]) << 8);
    return true;
}

float Bq27220Gauge::FilterPush(float sample) {
    if (!filter_primed_) {
        for (int i = 0; i < kFilterSize; ++i) {
            filter_buf_[i] = sample;
        }
        filter_sum_    = sample * kFilterSize;
        filter_count_  = kFilterSize;
        filter_idx_    = 0;
        filter_primed_ = true;
        return sample;
    }
    filter_sum_ -= filter_buf_[filter_idx_];
    filter_buf_[filter_idx_] = sample;
    filter_sum_ += sample;
    filter_idx_ = (filter_idx_ + 1) % kFilterSize;
    if (filter_count_ < kFilterSize) filter_count_++;
    return filter_sum_ / filter_count_;
}

extern "C" signed int battery_get_bat_voltage(void)
{
    uint16_t mv = 0;
    if (!Bq27220Gauge::GetInstance().ReadVoltageMv(mv)) {
        return 0;
    }
    return static_cast<signed int>(mv);
}

extern "C" signed int battery_get_bat_current(void)
{
    int16_t ma = 0;
    if (!Bq27220Gauge::GetInstance().ReadCurrentMa(ma)) {
        return 0;
    }
    /* MTK cx25601n 参考：ibat = raw/10，>=1e6 表示约 1A；raw 为 0.1µA。 */
    return static_cast<signed int>(ma) * 10000;
}
