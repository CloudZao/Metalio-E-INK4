#include "sc7a20h.h"

#include <cmath>
#include <esp_log.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TAG "Sc7a20h"

namespace {
constexpr uint8_t  kRegWhoAmI   = 0x0F;
constexpr uint8_t  kRegCtrlReg1 = 0x20;
constexpr uint8_t  kRegCtrlReg4 = 0x23;
constexpr uint8_t  kRegOutXL    = 0x28;
constexpr uint8_t  kAutoIncMask = 0x80;
// CTRL_REG1=0x57：ODR=100Hz，XYZ 使能；CTRL_REG4=0x88：BDU+HR，±2g。
constexpr uint8_t  kCtrlReg1Val = 0x57;
constexpr uint8_t  kCtrlReg4Val = 0x88;
constexpr float    kMgPerLsb    = 1.0f;
constexpr float    kGravityMg   = 1000.0f;
constexpr uint32_t kI2cSpeedHz  = 400 * 1000;
constexpr int      kI2cTimeoutMs = 100;
constexpr int      kProbeTimeoutMs = 100;
constexpr uint8_t  kWhoAmIKnown[] = {0x11, 0x33, 0x32, 0x44};
}  // namespace

bool Sc7a20h::WriteReg(uint8_t reg, uint8_t value) {
    if (dev_ == nullptr) {
        return false;
    }
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(dev_, buf, sizeof(buf), kI2cTimeoutMs) == ESP_OK;
}

bool Sc7a20h::ReadRegs(uint8_t reg, uint8_t* data, size_t len) {
    if (dev_ == nullptr || data == nullptr || len == 0) {
        return false;
    }
    return i2c_master_transmit_receive(dev_, &reg, 1, data, len, kI2cTimeoutMs) == ESP_OK;
}

bool Sc7a20h::Configure() {
    if (!WriteReg(kRegCtrlReg1, kCtrlReg1Val)) {
        return false;
    }
    return WriteReg(kRegCtrlReg4, kCtrlReg4Val);
}

bool Sc7a20h::Begin(i2c_master_bus_handle_t bus, uint8_t addr) {
    if (bus == nullptr) {
        ESP_LOGW(TAG, "Begin() null bus");
        return false;
    }
    bus_ = bus;
    addr_ = addr;
    if (dev_ != nullptr) {
        return true;
    }

    esp_err_t probe = i2c_master_probe(bus_, addr_, kProbeTimeoutMs);
    if (probe != ESP_OK) {
        ESP_LOGW(TAG, "SC7A20H @0x%02X probe failed: %s", addr_, esp_err_to_name(probe));
        return false;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr_,
        .scl_speed_hz = kI2cSpeedHz,
        .scl_wait_us = 0,
        .flags = {.disable_ack_check = 0},
    };
    esp_err_t err = i2c_master_bus_add_device(bus_, &cfg, &dev_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "add_device failed: %s", esp_err_to_name(err));
        dev_ = nullptr;
        return false;
    }

    uint8_t who = 0;
    if (ReadRegs(kRegWhoAmI, &who, 1)) {
        last_who_am_i_ = who;
        bool known = false;
        for (uint8_t v : kWhoAmIKnown) {
            if (who == v) {
                known = true;
                break;
            }
        }
        if (known) {
            ESP_LOGI(TAG, "SC7A20H online @0x%02X WHO_AM_I=0x%02X", addr_, who);
        } else {
            ESP_LOGW(TAG, "SC7A20H @0x%02X WHO_AM_I=0x%02X（未知，best-effort）", addr_, who);
        }
    } else {
        ESP_LOGW(TAG, "SC7A20H ACK 通过但读 WHO_AM_I 失败");
    }

    if (!Configure()) {
        ESP_LOGW(TAG, "SC7A20H Configure 失败，保留 device 句柄");
    }
    return true;
}

bool Sc7a20h::ReadAccelMg(int& ax, int& ay, int& az) {
    uint8_t buf[6] = {};
    if (!ReadRegs(static_cast<uint8_t>(kRegOutXL | kAutoIncMask), buf, sizeof(buf))) {
        return false;
    }
    const int16_t rx = static_cast<int16_t>((buf[1] << 8) | buf[0]);
    const int16_t ry = static_cast<int16_t>((buf[3] << 8) | buf[2]);
    const int16_t rz = static_cast<int16_t>((buf[5] << 8) | buf[4]);
    ax = static_cast<int>((rx >> 4) * kMgPerLsb);
    ay = static_cast<int>((ry >> 4) * kMgPerLsb);
    az = static_cast<int>((rz >> 4) * kMgPerLsb);
    return true;
}

bool Sc7a20h::ReadPitchRollDeg(float& pitch_deg, float& roll_deg) {
    int ax = 0;
    int ay = 0;
    int az = 0;
    if (!ReadAccelMg(ax, ay, az)) {
        return false;
    }
    const float ax_g = static_cast<float>(ax) / kGravityMg;
    const float ay_g = static_cast<float>(ay) / kGravityMg;
    const float az_g = static_cast<float>(az) / kGravityMg;
    constexpr float kRad2Deg = 180.0f / static_cast<float>(M_PI);
    pitch_deg = std::atan2(ax_g, std::sqrt(ay_g * ay_g + az_g * az_g)) * kRad2Deg;
    roll_deg = std::atan2(ay_g, std::sqrt(ax_g * ax_g + az_g * az_g)) * kRad2Deg;
    return true;
}
