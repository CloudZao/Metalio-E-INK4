#include "metalio_vibe.h"

#include "config.h"
#include "power_hw.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>

#define TAG "MetalioVibe"

static esp_timer_handle_t s_vibe_timer = nullptr; // 短震关断定时器
static bool s_vibe_continuous = false;            // true：连续震动（老化等）

static void MetalioVibe_OffTimerCb(void* /*arg*/) {
    if (s_vibe_continuous) {
        return;
    }
    gpio_set_level(VIBRATION_MOTOR_GPIO, 0);
}

void MetalioVibe_Set(bool on) {
    if (s_vibe_timer != nullptr) {
        esp_timer_stop(s_vibe_timer);
    }
    gpio_set_direction(VIBRATION_MOTOR_GPIO, GPIO_MODE_OUTPUT);
    s_vibe_continuous = on;
    gpio_set_level(VIBRATION_MOTOR_GPIO, on ? 1 : 0);
    ESP_LOGI(TAG, "vibe motor continuous=%d", on ? 1 : 0);
}

void MetalioVibe_Pulse(int pulse_ms) {
    if (s_vibe_continuous) {
        ESP_LOGW(TAG, "pulse skip continuous");
        return;
    }
    if (s_vibe_timer == nullptr) {
        ESP_LOGW(TAG, "pulse skip no timer");
        return;
    }
    const int ms = (pulse_ms > 0) ? pulse_ms : VIBRATION_MOTOR_PULSE_MS;
    // 浅睡后部分脚可能丢输出态；每次脉冲前再钉成推挽输出
    gpio_set_direction(VIBRATION_MOTOR_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(VIBRATION_MOTOR_GPIO, 1);
    esp_timer_stop(s_vibe_timer);
    esp_err_t err = esp_timer_start_once(s_vibe_timer, static_cast<uint64_t>(ms) * 1000ULL);
    if (err != ESP_OK) {
        gpio_set_level(VIBRATION_MOTOR_GPIO, 0);
        ESP_LOGW(TAG, "vibe timer start failed: %s", esp_err_to_name(err));
        return;
    }
    // known=false 时 is_on 亦为 false，不等于硬件掉电；仅策略明确掉轨才告警
    if (power_hw_main_rail_is_known() && !power_hw_main_rail_is_on()) {
        ESP_LOGW(TAG, "pulse while MAIN_PWR off gpio=%d %dms",
                 static_cast<int>(VIBRATION_MOTOR_GPIO), ms);
    }
}

void MetalioVibe_Init() {
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
        .callback = &MetalioVibe_OffTimerCb,
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
