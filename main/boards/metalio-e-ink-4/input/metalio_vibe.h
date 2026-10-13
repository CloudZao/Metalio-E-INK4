#pragma once

/**
 * @brief 初始化震动马达 GPIO 与短震定时器
 */
void MetalioVibe_Init();

/**
 * @brief 短震一次（连续模式或未 Init 时忽略）
 * @param pulse_ms 脉宽 ms；<=0 时用板级默认 VIBRATION_MOTOR_PULSE_MS
 */
void MetalioVibe_Pulse(int pulse_ms);

/**
 * @brief 连续震动开关（老化等）
 * @param on true 持续开，false 关
 */
void MetalioVibe_Set(bool on);
