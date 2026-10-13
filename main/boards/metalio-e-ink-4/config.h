#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000 // 麦采样率
#define AUDIO_OUTPUT_SAMPLE_RATE 16000 // 喇叭采样率

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_43 // I2S 麦 WS
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_17 // I2S 麦 DIN
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7  // I2S 喇叭 DOUT
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_6  // I2S 喇叭 BCLK

#define NT26_RX_PIN   GPIO_NUM_11 // NT26 UART RX
#define NT26_TX_PIN   GPIO_NUM_12 // NT26 UART TX
#define NT26_MRDY_PIN GPIO_NUM_21 // NT26 MRDY
#define NT26_SRDY_PIN GPIO_NUM_5  // NT26 SRDY

#define BT_AUDIO_TX_PIN GPIO_NUM_48 // 蓝牙音频 UART TX
#define BT_AUDIO_RX_PIN GPIO_NUM_47 // 蓝牙音频 UART RX

#define BOOT_BUTTON_GPIO  GPIO_NUM_0 // BOOT（IO0）
#define POWER_BUTTON_GPIO GPIO_NUM_3 // 电源键（开关机芯片）
#define TOUCH_BUTTON_GPIO GPIO_NUM_NC // 未用物理触摸键
// 音量键在 TCA9555：P0.7=减、P1.0=加；IO39/40 给 SDMMC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC // 音量+（扩展器）
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC // 音量-（扩展器）

// SDMMC 1-bit：DAT3(CD) 须保持高，防卡进 SPI
#define SDMMC_CLK_PIN  GPIO_NUM_38 // SD CLK
#define SDMMC_CMD_PIN  GPIO_NUM_40 // SD CMD
#define SDMMC_D0_PIN   GPIO_NUM_39 // SD DAT0
#define SDMMC_DAT3_PIN GPIO_NUM_46 // SD DAT3/CD（S3 仅输入）

#define USB_OTG_DM_PIN GPIO_NUM_19 // USB OTG D-
#define USB_OTG_DP_PIN GPIO_NUM_20 // USB OTG D+

// GDEM0397T81 3.97" 800x480 SSD1677
#define DISPLAY_WIDTH  800 // 面板宽
#define DISPLAY_HEIGHT 480 // 面板高

#define EPD_SPI_HOST   SPI3_HOST   // 墨水屏 SPI 主机
#define EPD_SPI_CLK_HZ 10000000    // 墨水屏 SPI 时钟
#define EPD_PIN_MOSI   GPIO_NUM_8  // EPD MOSI
#define EPD_PIN_SCLK   GPIO_NUM_14 // EPD SCLK
#define EPD_PIN_CS     GPIO_NUM_45 // EPD CS
#define EPD_PIN_DC     GPIO_NUM_13 // EPD DC
#define EPD_PIN_RST    GPIO_NUM_18 // EPD RST
#define EPD_PIN_BUSY   GPIO_NUM_9  // EPD BUSY

#define I2C_SDA_PIN GPIO_NUM_41 // I2C SDA
#define I2C_SCL_PIN GPIO_NUM_42 // I2C SCL

#define TOUCH_INT_GPIO GPIO_NUM_1 // 触摸 INT
// CST816S Fast Mode 400 kHz（勿用宏默认 100k）
#define TOUCH_I2C_HZ 400000 // 触摸 I2C 速率

#define IO_EXPANDER_INT_GPIO GPIO_NUM_2 // TCA9555 INT（ACCEL_INT 在扩展器 P1.4）

// 震动马达：高有效；勿用 #if 与 GPIO_NUM_NC 比较（预处理为 0）
#define VIBRATION_MOTOR_GPIO     GPIO_NUM_44 // 震动马达
#define VIBRATION_MOTOR_PULSE_MS 35          // 短震脉宽 ms

// 盖板虚拟键（CST816S 原生竖屏坐标，与 LVGL 480x800 一致）
#define TOUCH_VK_HOME_X 80  // HOME X
#define TOUCH_VK_HOME_Y 900 // HOME Y
#define TOUCH_VK_PREV_X 400 // 上一页 X（盖板右）
#define TOUCH_VK_PREV_Y 900 // 上一页 Y
#define TOUCH_VK_NEXT_X 240 // 下一页 X（盖板中）
#define TOUCH_VK_NEXT_Y 900 // 下一页 Y

#define DISPLAY_MIRROR_X false // 面板 X 镜像
#define DISPLAY_MIRROR_Y false // 面板 Y 镜像

#endif // _BOARD_CONFIG_H_
