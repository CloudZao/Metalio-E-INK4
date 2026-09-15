#pragma once

#include "lvgl.h"

// 设置 → 测试 → 自动测试：按住说话 / 松开播放（参考 395 AudioTest）。
void SettingsTestAudio_BuildRow(lv_obj_t* parent);
void SettingsTestAudio_OnLoad();
void SettingsTestAudio_Teardown();
void SettingsTestAudio_Poll();
