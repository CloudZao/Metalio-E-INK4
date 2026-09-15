#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_43
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_17
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_6

#define NT26_RX_PIN GPIO_NUM_11
#define NT26_TX_PIN GPIO_NUM_12
#define NT26_MRDY_PIN GPIO_NUM_21
#define NT26_SRDY_PIN GPIO_NUM_5  

#define BT_AUDIO_TX_PIN GPIO_NUM_48
#define BT_AUDIO_RX_PIN GPIO_NUM_47

#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_0   /* ESP32 IO0 — BOOT */
#define POWER_BUTTON_GPIO       GPIO_NUM_3   /* ESP32 IO3 — 开关机芯片 */
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
/* 音量键在 TCA9555：P0.7=减、P1.0=加；IO39/40 给 SDMMC CMD/DAT0 用 */
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

/* SDMMC 1-bit：CLK/CMD/D0；DAT3(CD) 不参与数据但须保持高，防卡进 SPI */
#define SDMMC_CLK_PIN   GPIO_NUM_38
#define SDMMC_CMD_PIN   GPIO_NUM_40
#define SDMMC_D0_PIN    GPIO_NUM_39
#define SDMMC_DAT3_PIN  GPIO_NUM_46  /* microSD pin2 CD/DAT3；S3 GPIO46 仅输入 */

/* ESP32-S3 内置 USB OTG FS（与 USB Serial/JTAG 共用）：启用模拟 U 盘时切到 MSC */
#define USB_OTG_DM_PIN  GPIO_NUM_19
#define USB_OTG_DP_PIN  GPIO_NUM_20

/* GDEM0397T81 3.97" 800x480 SSD1677 — 引脚与 esp32-s3-epd-397 一致 */
#define DISPLAY_WIDTH   800
#define DISPLAY_HEIGHT  480

#define EPD_SPI_HOST    SPI3_HOST
#define EPD_SPI_CLK_HZ  10000000
#define EPD_PIN_MOSI    GPIO_NUM_8
#define EPD_PIN_SCLK    GPIO_NUM_14
#define EPD_PIN_CS      GPIO_NUM_45
#define EPD_PIN_DC      GPIO_NUM_13
#define EPD_PIN_RST     GPIO_NUM_18
#define EPD_PIN_BUSY    GPIO_NUM_9

#define I2C_SDA_PIN     GPIO_NUM_41
#define I2C_SCL_PIN     GPIO_NUM_42

#define TOUCH_INT_GPIO  GPIO_NUM_1
/* CST816S Fast Mode（Espressif 官方 test 亦用 400 kHz；宏默认 100k 偏保守） */
#define TOUCH_I2C_HZ    400000

/* TCA9555 INT → 主控；加速度计 INT 挂在扩展器 P1.4（见 IOExpander::Pin::ACCEL_INT） */
#define IO_EXPANDER_INT_GPIO GPIO_NUM_2

/* 盖板虚拟键触觉反馈：GPIO44 震动马达（高电平有效；勿用 #if 与 GPIO_NUM_NC 比较——枚举在预处理阶段为 0） */
#define VIBRATION_MOTOR_GPIO     GPIO_NUM_44
#define VIBRATION_MOTOR_PULSE_MS 35

/* 盖板外侧三个虚拟按键（CST816S 原生竖屏坐标，与 LVGL 480x800 一致） */
#define TOUCH_VK_HOME_X  80   // HOME 键
#define TOUCH_VK_HOME_Y  900
#define TOUCH_VK_PREV_X  400  // 上一页按键（盖板右侧）
#define TOUCH_VK_PREV_Y  900
#define TOUCH_VK_NEXT_X  240  // 下一页按键（盖板中间）
#define TOUCH_VK_NEXT_Y  900

#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false

#endif // _BOARD_CONFIG_H_
