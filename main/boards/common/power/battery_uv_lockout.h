#pragma once

/**
 * @brief 欠压关机流程中：触摸/物理键应忽略
 */
bool BatteryUvLockout_IsActive();

/** @brief 由 PowerPolicy 1Hz tick 调用；非待机且 Vbat 低于 3.2V 且放电时启动警告任务 */
void BatteryUvShutdownTick();
