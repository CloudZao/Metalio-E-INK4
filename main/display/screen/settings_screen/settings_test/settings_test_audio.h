#pragma once

#include "lvgl.h"

/** @brief 设置→测试→自动测试：按住说话 / 松开播放（参考 395 AudioTest） */
void SettingsTestAudio_BuildRow(lv_obj_t* parent);
/** @brief 进页：准备录音通路 */
void SettingsTestAudio_OnLoad();
/** @brief 离页：停录播、恢复设备态 */
void SettingsTestAudio_Teardown();
/** @brief 周期轮询录播状态机 */
void SettingsTestAudio_Poll();
