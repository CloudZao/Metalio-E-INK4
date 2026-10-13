#pragma once

#include <driver/i2c_master.h>

/**
 * @brief 初始化蓝牙音频 UART，并应用默认蓝牙模式
 */
void MetalioPeriph_InitBtAudio();

/**
 * @brief 挂载 SD 卡并初始化虚拟 U 盘 worker
 */
void MetalioPeriph_InitSdCard();

/**
 * @brief 初始化 PCF8563，并将芯片时间灌入系统
 * @param bus I2C 总线
 */
void MetalioPeriph_InitPcf8563(i2c_master_bus_handle_t bus);

/**
 * @brief 探测并初始化 CX25601N 充电 IC（老板无芯片则跳过）
 * @param bus I2C 总线
 */
void MetalioPeriph_InitCx25601n(i2c_master_bus_handle_t bus);

/**
 * @brief 启动系统监控任务（CPU/内存/电池周期日志）
 */
void MetalioPeriph_StartSystemMonitor();
