/*
 * Metalio E-Ink 4 — CST816S 触摸：初始化 / 休眠 / 硬件 RST 唤醒
 *
 * RST 经 TCA9555 P1.1（原理图 P11，io_index=9），低有效。
 */
#pragma once

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_io_expander.h>
#include <esp_lcd_touch.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 上电复位序列：RST 低 → Tpr → 高 → Tron */
esp_err_t metalio_touch_hw_reset_power_on(esp_io_expander_handle_t expander);

/** 运行中复位脉冲：H → L → H（规格 Trst/Tpr/Tron） */
esp_err_t metalio_touch_hw_reset_pulse(esp_io_expander_handle_t expander);

/**
 * 探测并创建 CST816S。
 * @return ESP_OK 且 *out_touch 非空；失败时 *out_touch=NULL，返回错误码（不崩溃）
 */
esp_err_t metalio_touch_init(i2c_master_bus_handle_t i2c_bus, esp_lcd_touch_handle_t* out_touch);

/** 写 0xA5=0x03 进入触摸深睡 */
esp_err_t metalio_touch_enter_sleep(esp_lcd_touch_handle_t touch);

/** 硬件 RST 唤醒后可选回调（例如通知 LVGL 触摸 indev） */
typedef void (*metalio_touch_after_wake_cb_t)(void);

/** A5=03 深睡后仅硬件 RST 脉冲唤醒（不写触摸寄存器） */
esp_err_t metalio_touch_wake_by_reset(esp_io_expander_handle_t expander,
                                      metalio_touch_after_wake_cb_t after_wake_cb);

#ifdef __cplusplus
}
#endif
