#pragma once

#include "lvgl.h"

// 页面级虚拟键分发与生命周期管理，统一处理 HOME/PREV/NEXT 和 BOOT 事件。
typedef enum {
    VK_SCREEN_LIFECYCLE_LOAD = 0,
    VK_SCREEN_LIFECYCLE_UNLOAD,
} vk_screen_lifecycle_t;

// 返回栈中用到的页面工厂函数，需为无捕获静态函数。
using ScreenFactory = lv_obj_t* (*)();

// 返回 true 表示页面已消费该按键，不再继续走默认策略。
using VkKeyHandler = bool (*)(const char* key_name);

// BOOT 事件回调；返回 true 表示页面已处理。
using BootKeyAction = bool (*)();

struct VkKeyScreenDesc {
    ScreenFactory factory = nullptr;
    VkKeyHandler on_key = nullptr;
    BootKeyAction on_boot_click = nullptr;
    BootKeyAction on_boot_long_press = nullptr;
    BootKeyAction on_boot_press_down = nullptr;
    BootKeyAction on_boot_press_up = nullptr;
    VkKeyHandler on_key_long_press = nullptr;
    VkKeyHandler on_key_press_up = nullptr;
    BootKeyAction on_boot_double_click = nullptr;
};

// 页面生命周期：压栈、出栈和前台页名查询。
void VkKey_OnScreenLifecycle(const char* name, vk_screen_lifecycle_t event);
const char* VkKey_ActiveScreen();
ScreenFactory VkKey_GetScreenFactory(const char* name);
void VkKey_SetScreenFactory(const char* name, ScreenFactory factory);

// BOOT 回调注册与查询。
BootKeyAction VkKey_GetBootClick(const char* name);
BootKeyAction VkKey_GetBootLongPress(const char* name);
BootKeyAction VkKey_GetBootPressDown(const char* name);
BootKeyAction VkKey_GetBootPressUp(const char* name);
BootKeyAction VkKey_GetBootDoubleClick(const char* name);

// 挂接页面和处理虚拟键回调。
void VkKey_AttachScreen(lv_obj_t* scr, const char* name);
void VkKey_AttachScreen(lv_obj_t* scr, const char* name, const VkKeyScreenDesc& desc);
void VkKey_Dispatch(const char* key_name);
bool VkKey_OnLongPress(const char* key_name);
bool VkKey_OnPressUp(const char* key_name);
