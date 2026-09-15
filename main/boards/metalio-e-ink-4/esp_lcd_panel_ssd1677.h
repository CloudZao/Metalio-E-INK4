/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*esp_lcd_epaper_panel_cb_t)(const esp_lcd_panel_handle_t handle, const void *edata, void *user_data);

typedef struct {
    esp_lcd_epaper_panel_cb_t on_epaper_refresh_done;
} epaper_panel_callbacks_t;

typedef struct {
    int busy_gpio_num;
    bool non_copy_mode;
    bool use_fast_full_update;
} esp_lcd_ssd1677_config_t;

typedef enum {
    SSD1677_EPAPER_BITMAP_CURRENT = 0,
    SSD1677_EPAPER_BITMAP_PREVIOUS,
} esp_lcd_ssd1677_bitmap_color_t;

typedef enum {
    SSD1677_EPAPER_REFRESH_FULL = 0, // 0x22=0xC4 运行中全刷
    SSD1677_EPAPER_REFRESH_FULL_FAST,
    SSD1677_EPAPER_REFRESH_PARTIAL,
    SSD1677_EPAPER_REFRESH_STANDBY,  // 0x22=0xC7 进/出待机与关机
} esp_lcd_ssd1677_refresh_mode_t;

esp_err_t esp_lcd_new_panel_ssd1677(const esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief 启动刷新；FULL 类：轮询 BUSY；PARTIAL 挂 BUSY 下降沿 ISR
 * @note MCU LUT 仅在全↔局切换时重下
 */
esp_err_t epaper_panel_refresh_screen(esp_lcd_panel_t *panel);

esp_err_t epaper_panel_set_refresh_mode(esp_lcd_panel_t *panel, esp_lcd_ssd1677_refresh_mode_t mode);

esp_err_t epaper_panel_set_bitmap_color(esp_lcd_panel_t *panel, esp_lcd_ssd1677_bitmap_color_t color);

esp_err_t epaper_panel_register_event_callbacks(esp_lcd_panel_t *panel, epaper_panel_callbacks_t *cbs, void *user_ctx);

/**
 * @brief 每次局刷前：写边框 0x80（无 HW RST / soft reset）
 */
esp_err_t epaper_panel_prepare_for_partial(esp_lcd_panel_t *panel);

/**
 * @brief 进入 Deep Sleep（0x10）；唤醒需硬件 RST
 */
esp_err_t epaper_panel_deep_sleep(esp_lcd_panel_t *panel);

#ifdef __cplusplus
}
#endif
