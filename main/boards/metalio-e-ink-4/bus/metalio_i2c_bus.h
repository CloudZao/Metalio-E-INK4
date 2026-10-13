#pragma once

#include <driver/i2c_master.h>

/**
 * @brief 创建（或重建）板级 I2C0 总线；创建前 bit-bang 恢复 SDA/SCL
 * @param bus 入参可为已有句柄（非空则先删再建）；出参为新总线
 */
void MetalioI2cBus_Create(i2c_master_bus_handle_t* bus);
