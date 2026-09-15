#ifndef BQ27220_GAUGE_H
#define BQ27220_GAUGE_H

#include <cstdint>
#include <driver/i2c_master.h>

// BQ27220 电量计：单例封装，板级初始化后直接通过 GetInstance() 使用。
// 设计上不透出板子私有类，UI / 调试任务 / 应用层都只依赖这个接口。
// Begin() 会做地址级 ACK 检测；设备没挂上时只返回 false，不会直接崩溃。
// GetBatteryLevel() 使用电压到 SOC 的线性插值和单向步进滤波，避免抖动。
class Bq27220Gauge {
public:
    static constexpr uint8_t kDefaultAddr = 0x55;

    static Bq27220Gauge& GetInstance() {
        static Bq27220Gauge instance;
        return instance;
    }

    // 在已有的 v2 master bus 上挂 BQ27220。重复调用安全，已挂上时直接返回 true。
    // probe NACK 时返回 false，调用方不需要做任何事 —— 后续 GetBatteryLevel
    // 内部会节流自愈，10s 重试一次。
    bool Begin(i2c_master_bus_handle_t bus, uint8_t addr = kDefaultAddr);

    // 当前是否已经挂上了 BQ27220 device handle。
    bool IsReady() const { return dev_ != nullptr; }

    // ---- 低层寄存器读 ----
    // 返回 false 表示总线错误 / 设备没挂上；调用方可以兜底显示 "--"。
    bool ReadVoltageMv(uint16_t& mv);
    bool ReadCurrentMa(int16_t& current_ma);

    // 别名：和板子原本的命名兼容。
    bool GetVoltageMv(uint16_t& mv) { return ReadVoltageMv(mv); }

    // ---- 高层接口 ----
    // 完整电量 / 充放电状态。语义和 Board::GetBatteryLevel 对齐：
    //   level       : 0..100；≥4.28V 连续 30s 才允许 100%
    //   charging    : 电流 > +5mA，且 level < 100（满电不报充电）
    //   discharging : 电流 < -5mA
    // 返回 false 表示 device 没挂上、或这一帧总线读失败；调用方应保留上次值
    // 或显示占位。
    bool GetBatteryLevel(int& level, bool& charging, bool& discharging);

    // 重置滑动平均与步进显示。换电池 / 长时间断开总线后可调一次。
    void ResetFilter();

private:
    Bq27220Gauge() = default;
    Bq27220Gauge(const Bq27220Gauge&) = delete;
    Bq27220Gauge& operator=(const Bq27220Gauge&) = delete;

    // 读 BQ27220 一个 uint16 寄存器（标准命令均按 little-endian 解析）。
    bool ReadU16(uint8_t reg, uint16_t* out);

    // 60 点滑动平均；首次填满整窗口避免开机一两秒内目标大跳。
    float FilterPush(float sample);

    // 充电只允许升、未充电只允许降；约 30s 步进 1%。
    int ApplyMonoStep(int target_pct, bool charging);

    static constexpr int kFilterSize = 60;
    static constexpr int64_t kSocStepUs = 30LL * 1000 * 1000;
    static constexpr int64_t kFullHoldUs = 30LL * 1000 * 1000; // ≥4.28V 连续多久才允许 100%
    static constexpr uint16_t kFullConfirmMv = 4280;
    static constexpr uint16_t kFullExitMv = 4230; // 离开 100% 的滞回，防 4280 附近抖

    i2c_master_bus_handle_t bus_   = nullptr;
    i2c_master_dev_handle_t dev_   = nullptr;
    uint8_t                 addr_  = kDefaultAddr;

    int  consecutive_err_  = 0;   // 连续读失败计数，用于警告去抖
    int  retry_counter_    = 0;   // GetBatteryLevel 中的重挂节流

    float filter_buf_[kFilterSize] = {0};
    int   filter_idx_      = 0;
    int   filter_count_    = 0;
    float filter_sum_      = 0.0f;
    bool  filter_primed_   = false;

    int     displayed_soc_ = -1;  // <0 表示尚未锚定
    int64_t last_soc_step_us_ = 0;
    int64_t full_above_since_us_ = 0; // 电压持续 ≥kFullConfirmMv 的起点；0=未满足
    int     last_charge_dir_ = 0;     // >0 充电 / <0 放电 / 0 空载；变向则重开步进计时
};

#ifdef __cplusplus
extern "C" {
#endif

// CX25601N 自适应 VREG 任务（MTK 参考驱动）所需的外部采样接口。
// 电压：mV；电流：0.1µA 单位（charger_vreg_task 内 raw/10，1e6 ≈ 1A）。
signed int battery_get_bat_voltage(void);
signed int battery_get_bat_current(void);

#ifdef __cplusplus
}
#endif

#endif  // BQ27220_GAUGE_H
