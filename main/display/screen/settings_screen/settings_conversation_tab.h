#pragma once

#include "lvgl.h"

// 设置 → 对话：百问AI TTS 开关（服务端 GET/PUT，本机不拦截播报）
void SettingsConversationTab_Build(lv_obj_t* page);
void SettingsConversationTab_Reset();

// 进入「对话」Tab 时拉取服务端 TTS 状态
void SettingsConversationTab_OnActivated();
