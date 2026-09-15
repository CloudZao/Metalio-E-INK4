#pragma once

// 板载 BOOT/POWER 键事件分发，负责短按、长按、双击和待机管理。
void BootKey_OnPressDown();
void BootKey_OnPressUp();
void BootKey_OnClick();
void BootKey_OnDoubleClick();
void BootKey_OnLongPress();

// 非待机页按电源键进入低功耗待机，长按则关机。
void PowerKey_OnClick();
void PowerKey_OnLongPress();

// 当前按键状态，用于长按、短按互斥和页面 hold-through。
bool BootKey_IsHeld();
bool BootKey_DidLongPress();
