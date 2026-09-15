#ifndef SC7A20H_H
#define SC7A20H_H

#include <cstddef>
#include <cstdint>
#include <driver/i2c_master.h>

// SC7A20H 三轴加速度计，I2C 地址 0x19，兼容 LIS2DH12 寄存器。
// Begin() 成功后会配置为 ±2g HR 模式；ReadAccelMg() / ReadPitchRollDeg() 可供业务读取。
class Sc7a20h {
public:
    static constexpr uint8_t kDefaultAddr = 0x19;

    static Sc7a20h& GetInstance() {
        static Sc7a20h instance;
        return instance;
    }

    // probe 失败返回 false；已挂上则直接返回 true，并执行配置。
    bool Begin(i2c_master_bus_handle_t bus, uint8_t addr = kDefaultAddr);
    bool IsReady() const { return dev_ != nullptr; }

    uint8_t LastWhoAmI() const { return last_who_am_i_; }

    // 单位为 mg，失败返回 false。
    bool ReadAccelMg(int& ax, int& ay, int& az);

    // 根据加速度估算俯仰角和横滚角，失败返回 false。
    bool ReadPitchRollDeg(float& pitch_deg, float& roll_deg);

private:
    Sc7a20h() = default;
    Sc7a20h(const Sc7a20h&) = delete;
    Sc7a20h& operator=(const Sc7a20h&) = delete;

    bool WriteReg(uint8_t reg, uint8_t value);
    bool ReadRegs(uint8_t reg, uint8_t* data, size_t len);
    bool Configure();

    i2c_master_bus_handle_t bus_  = nullptr;
    i2c_master_dev_handle_t dev_  = nullptr;
    uint8_t                 addr_ = kDefaultAddr;
    uint8_t                 last_who_am_i_ = 0;
};

#endif  // SC7A20H_H
