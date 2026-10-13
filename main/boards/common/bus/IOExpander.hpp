#ifndef IO_EXPANDER_HPP
#define IO_EXPANDER_HPP

#include <cstdint>
#include <initializer_list>

#include <driver/i2c_master.h>
#include <esp_io_expander.h>
#include <esp_io_expander_tca95xx_16bit.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// IOExpander：TCA9555 的薄封装单例。
// io_index: 0..7 对应 P0.0..P0.7，8..15 对应 P1.0..P1.7。
// 原理图中的 P11/P13 指 Port1 的 bit1/bit3，即 P1.1 / P1.3，io_index 分别为 9、11。
// 输入读取做短时缓存：按键 5ms 轮询且音量 +/- 各读一次，避免同一 I2C 总线过载。
class IOExpander {
public:
    enum class Pin : uint8_t {
        SCREEN_SOCKET_PWR = 0,  // P0.5 — 屏幕卡座供电
        MAIN_PWR,               // P0.6 — 总电源
        PA,                     // P0.4 — 音频功放开关
        PA_SWITCH,              // P0.1 — 功放切换：1=4G 通话，0=ESP32
        VOLUME_DOWN,            // P0.7 — 音量减按键（输入）
        VOLUME_UP,              // P1.0 — 音量加按键（输入）
        TOUCH_RST,              // P1.1 / 原理图 P11 — 触摸 CST816 RST（低有效）
        PWR_KEY_PULSE,          // P1.3 / 原理图 P13 — 开关机脉冲输出
        ACCEL_INT,              // P1.4 — 加速度计 INT（输入）
        USB_MUX_SEL,            // P0.0 — FSUSB42UMX：低=摄像头，高=烧录/调试
        kPinCount,
    };

    enum class Direction : uint8_t {
        kOutput = 0,
        kInput  = 1,
    };

    struct PinMapEntry {
        Pin       pin;
        uint8_t   io_index;
        Direction direction = Direction::kOutput;
    };

    static constexpr PinMapEntry kDefaultPinMap[] = {
        {Pin::SCREEN_SOCKET_PWR, 5, Direction::kOutput},  // P0.5
        {Pin::MAIN_PWR,          6, Direction::kOutput},  // P0.6
        {Pin::PA,                4, Direction::kOutput},  // P0.4
        {Pin::PA_SWITCH,         1, Direction::kOutput},  // P0.1
        {Pin::VOLUME_DOWN,       7, Direction::kInput},   // P0.7
        {Pin::VOLUME_UP,         8, Direction::kInput},   // P1.0
        {Pin::TOUCH_RST,         9, Direction::kOutput},  // P1.1 / 原理图 P11 — 触摸 RST
        {Pin::PWR_KEY_PULSE,    11, Direction::kOutput},  // P1.3 / 原理图 P13 — 关机脉冲
        {Pin::ACCEL_INT,        12, Direction::kInput},   // P1.4
        {Pin::USB_MUX_SEL,       0, Direction::kOutput},  // P0.0
    };

    static IOExpander& getInstance()
    {
        static IOExpander instance;
        return instance;
    }

    IOExpander(const IOExpander&)            = delete;
    IOExpander& operator=(const IOExpander&) = delete;

    static const char* PinName(Pin pin)
    {
        switch (pin) {
            case Pin::SCREEN_SOCKET_PWR: return "SCREEN_SOCKET_PWR";
            case Pin::MAIN_PWR:          return "MAIN_PWR";
            case Pin::PA:                return "PA";
            case Pin::PA_SWITCH:         return "PA_SWITCH";
            case Pin::VOLUME_DOWN:       return "VOLUME_DOWN";
            case Pin::VOLUME_UP:         return "VOLUME_UP";
            case Pin::TOUCH_RST:         return "TOUCH_RST";
            case Pin::PWR_KEY_PULSE:     return "PWR_KEY_PULSE";
            case Pin::ACCEL_INT:         return "ACCEL_INT";
            case Pin::USB_MUX_SEL:       return "USB_MUX_SEL";
            default:                     return "?";
        }
    }

    static const char* DirectionName(Direction d)
    {
        return d == Direction::kInput ? "IN" : "OUT";
    }

    esp_err_t setPinMap(std::initializer_list<PinMapEntry> map)
    {
        if (map.size() == 0) {
            ESP_LOGE(TAG, "Invalid pin map");
            return ESP_ERR_INVALID_ARG;
        }
        clearPinMap();
        for (const auto& entry : map) {
            assignPin(entry);
        }
        return ESP_OK;
    }

    esp_err_t begin(i2c_master_bus_handle_t i2c_bus,
                    uint32_t dev_addr = ESP_IO_EXPANDER_I2C_TCA9555_ADDRESS_000)
    {
        if (!hasAnyPin()) {
            applyDefaultPinMap();
        }
        return beginImpl(i2c_bus, dev_addr);
    }

    esp_err_t setLevel(Pin pin, uint8_t level)
    {
        if (!initialized_ || handle_ == nullptr) {
            ESP_LOGE(TAG, "Not initialized");
            return ESP_ERR_INVALID_STATE;
        }
        const PinSlot* slot = lookupSlot(pin);
        if (slot == nullptr) {
            logUnknownPin(pin);
            return ESP_ERR_NOT_FOUND;
        }
        if (slot->direction != Direction::kOutput) {
            ESP_LOGW(TAG, "setLevel(%s) ignored: pin is input", PinName(pin));
            return ESP_ERR_INVALID_STATE;
        }
        const uint32_t mask = 1U << static_cast<uint32_t>(slot->io_index);
        return esp_io_expander_set_level(handle_, mask, level ? 1 : 0);
    }

    esp_err_t setLevel(Pin pin, bool high)
    {
        return setLevel(pin, static_cast<uint8_t>(high ? 1 : 0));
    }

    esp_err_t getLevel(Pin pin, uint8_t* level)
    {
        if (!initialized_ || handle_ == nullptr) {
            ESP_LOGE(TAG, "Not initialized");
            return ESP_ERR_INVALID_STATE;
        }
        if (level == nullptr) {
            return ESP_ERR_INVALID_ARG;
        }
        const PinSlot* slot = lookupSlot(pin);
        if (slot == nullptr) {
            logUnknownPin(pin);
            return ESP_ERR_NOT_FOUND;
        }
        const uint32_t mask = 1U << static_cast<uint32_t>(slot->io_index);
        uint32_t value = 0;
        const esp_err_t ret = readInputCached(&value);
        if (ret == ESP_OK) {
            *level = (value & mask) ? 1 : 0;
        }
        return ret;
    }

    bool isInitialized() const { return initialized_; }
    esp_io_expander_handle_t handle() const { return handle_; }

private:
    static constexpr const char* TAG = "IOExpander";
    static constexpr uint8_t kUnmapped = 0xFF;
    static constexpr size_t kPinCountValue = static_cast<size_t>(Pin::kPinCount);
    // 音量键 5ms 轮询：缓存 20ms，两键共享一次 I2C 读
    static constexpr TickType_t kInputCacheTtlTicks = pdMS_TO_TICKS(20);
    // 失败后冷却，避免驱动层连续 ESP_LOGE 刷屏
    static constexpr TickType_t kInputFailBackoffTicks = pdMS_TO_TICKS(200);

    struct PinSlot {
        uint8_t   io_index  = kUnmapped;
        Direction direction = Direction::kOutput;
    };

    IOExpander()
    {
        clearPinMap();
        input_mutex_ = xSemaphoreCreateMutex();
    }

    void applyDefaultPinMap()
    {
        clearPinMap();
        for (const auto& entry : kDefaultPinMap) {
            assignPin(entry);
        }
    }

    void clearPinMap()
    {
        for (auto& slot : pin_to_slot_) {
            slot = PinSlot{};
        }
    }

    void assignPin(const PinMapEntry& entry)
    {
        const size_t idx = static_cast<size_t>(entry.pin);
        if (idx >= kPinCountValue || entry.io_index >= 16) {
            ESP_LOGE(TAG, "Invalid pin map entry");
            return;
        }
        pin_to_slot_[idx] = PinSlot{entry.io_index, entry.direction};
    }

    bool hasAnyPin() const
    {
        for (const auto& slot : pin_to_slot_) {
            if (slot.io_index != kUnmapped) {
                return true;
            }
        }
        return false;
    }

    const PinSlot* lookupSlot(Pin pin) const
    {
        const size_t idx = static_cast<size_t>(pin);
        if (idx >= kPinCountValue) {
            return nullptr;
        }
        const PinSlot& slot = pin_to_slot_[idx];
        return slot.io_index == kUnmapped ? nullptr : &slot;
    }

    void logUnknownPin(Pin pin) const
    {
        ESP_LOGE(TAG, "Pin '%s' not present in map", PinName(pin));
    }

    void buildDirectionMasks(uint32_t* output_mask, uint32_t* input_mask) const
    {
        uint32_t out = 0;
        uint32_t in  = 0;
        for (size_t i = 0; i < kPinCountValue; ++i) {
            const PinSlot& slot = pin_to_slot_[i];
            if (slot.io_index == kUnmapped) {
                continue;
            }
            const uint32_t bit = 1U << slot.io_index;
            if (slot.direction == Direction::kInput) {
                in |= bit;
            } else {
                out |= bit;
            }
        }
        *output_mask = out;
        *input_mask  = in;
    }

    esp_err_t readInputCached(uint32_t* value)
    {
        if (value == nullptr) {
            return ESP_ERR_INVALID_ARG;
        }
        if (input_mutex_ != nullptr) {
            xSemaphoreTake(input_mutex_, portMAX_DELAY);
        }

        const TickType_t now = xTaskGetTickCount();
        esp_err_t ret = ESP_OK;

        if (input_cache_valid_ && (now - input_cache_tick_) < kInputCacheTtlTicks) {
            *value = input_cache_value_;
            ret = ESP_OK;
        } else if (input_fail_until_ != 0 && now < input_fail_until_) {
            // 冷却期内沿用上次成功值，避免反复打 I2C/刷错误日志
            *value = input_cache_value_;
            ret = ESP_OK;
        } else {
            uint32_t raw = 0;
            ret = esp_io_expander_get_level(handle_, 0xFFFFu, &raw);
            if (ret == ESP_OK) {
                input_cache_value_ = raw;
                input_cache_tick_ = now;
                input_cache_valid_ = true;
                input_fail_until_ = 0;
                *value = raw;
            } else {
                input_fail_until_ = now + kInputFailBackoffTicks;
                if (input_cache_valid_) {
                    *value = input_cache_value_;
                    ret = ESP_OK;
                    const TickType_t since_log = now - input_fail_log_tick_;
                    if (input_fail_log_tick_ == 0 || since_log >= pdMS_TO_TICKS(5000)) {
                        input_fail_log_tick_ = now;
                        ESP_LOGW(TAG, "TCA9555 input read timeout, using cache (bus busy?)");
                    }
                }
            }
        }

        if (input_mutex_ != nullptr) {
            xSemaphoreGive(input_mutex_);
        }
        return ret;
    }

    esp_err_t beginImpl(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr)
    {
        if (i2c_bus == nullptr) {
            return ESP_ERR_INVALID_ARG;
        }
        if (handle_ != nullptr) {
            ESP_LOGW(TAG, "already initialized");
            return ESP_OK;
        }

        // 上电偶发：总线未稳 / SDA 粘滞 → 写方向寄存器卡满 I2C_TIMEOUT(1s)；
        // 板级 ESP_ERROR_CHECK 会 abort。短超时 probe，失败则复位控制器再试。
        // 勿在此处把全部输出拉低：MAIN_PWR/SCREEN 等会被瞬间掉电，未上电从设备
        // 可能钳住 SDA，下一轮开机 probe 全失败。驱动 reset 后输出锁存为 0xFFFF，
        // 仅改方向即可保持高；具体低电平由板级显式 setLevel。
        constexpr int kAttempts = 10;
        constexpr int kProbeTimeoutMs = 100;
        vTaskDelay(pdMS_TO_TICKS(20));
        esp_err_t ret = ESP_FAIL;
        for (int i = 0; i < kAttempts; ++i) {
            if (i > 0) {
                ESP_LOGW(TAG, "TCA9555 init retry %d/%d after %s", i + 1, kAttempts,
                         esp_err_to_name(ret));
                vTaskDelay(pdMS_TO_TICKS(30 * i));
                i2c_master_bus_reset(i2c_bus);
            }

            ret = i2c_master_probe(i2c_bus, static_cast<uint16_t>(dev_addr), kProbeTimeoutMs);
            if (ret != ESP_OK) {
                continue;
            }

            ret = esp_io_expander_new_i2c_tca95xx_16bit(i2c_bus, dev_addr, &handle_);
            if (ret == ESP_OK) {
                break;
            }
            handle_ = nullptr;
        }

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "TCA9555 init failed: %s", esp_err_to_name(ret));
            handle_ = nullptr;
            return ret;
        }

        uint32_t output_mask = 0;
        uint32_t input_mask  = 0;
        buildDirectionMasks(&output_mask, &input_mask);

        if (output_mask != 0) {
            ret = esp_io_expander_set_dir(handle_, output_mask, IO_EXPANDER_OUTPUT);
            if (ret != ESP_OK) {
                return ret;
            }
        }

        if (input_mask != 0) {
            ret = esp_io_expander_set_dir(handle_, input_mask, IO_EXPANDER_INPUT);
            if (ret != ESP_OK) {
                return ret;
            }
        }

        initialized_ = true;
        ESP_LOGI(TAG, "TCA9555 ready addr=0x%02lx out=0x%04lx in=0x%04lx",
                 (unsigned long)dev_addr,
                 (unsigned long)output_mask,
                 (unsigned long)input_mask);
        return ESP_OK;
    }

    PinSlot pin_to_slot_[kPinCountValue];
    esp_io_expander_handle_t handle_ = nullptr;
    bool initialized_ = false;

    SemaphoreHandle_t input_mutex_ = nullptr;
    uint32_t input_cache_value_ = 0xFFFFu;  // 默认高=未按下（低有效）
    TickType_t input_cache_tick_ = 0;
    TickType_t input_fail_until_ = 0;
    TickType_t input_fail_log_tick_ = 0;
    bool input_cache_valid_ = false;
};

#endif  // IO_EXPANDER_HPP
