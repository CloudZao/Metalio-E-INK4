#pragma once

#include "settings_network_tab.h"

#include "dual_network_board.h"
#include "nt26_board.h"

#include <cstdint>
#include <string>

#include <lvgl.h>

constexpr const char* TAG = "SettingsNetwork";
constexpr int kSimSlotExternal = 0; // 外置卡
constexpr int kSimSlotInternal = 1; // 内置卡

struct NetworkUi {
    lv_obj_t* root_scr = nullptr; // 根屏
    lv_obj_t* wifi_btn = nullptr; // WiFi 选项
    lv_obj_t* cell_btn = nullptr; // 4G 选项
    lv_obj_t* current_lbl = nullptr; // 当前网络文案
    lv_obj_t* wifi_cfg_box = nullptr; // 配置 WIFI 区块
    lv_obj_t* wifi_cfg_btn = nullptr; // 配置 WIFI 按钮
    lv_obj_t* sim_box = nullptr; // SIM 区块
    lv_obj_t* sim_ext_btn = nullptr; // 外置卡按钮
    lv_obj_t* sim_int_btn = nullptr; // 内置卡按钮
    lv_obj_t* sim_cur_lbl = nullptr; // 当前 SIM 文案
    lv_obj_t* overlay = nullptr; // 遮罩
    lv_obj_t* overlay_msg = nullptr; // 遮罩文案
    lv_timer_t* reboot_timer = nullptr; // 重启倒计时
    int reboot_remaining = 0; // 剩余秒数
    std::string reboot_headline; // 倒计时标题
};

extern NetworkUi SettingsNetwork_ui; // 网络 Tab UI
extern bool SettingsNetwork_sim_switch_pending; // SIM 切换进行中
extern int SettingsNetwork_cached_sim_slot; // 缓存槽位
extern bool SettingsNetwork_sim_slot_ready; // 槽位已就绪
extern bool SettingsNetwork_tab_alive; // Tab 存活

struct SwitchNetworkJob {
    NetworkType target = NetworkType::WIFI; // 目标网络类型
};

struct SimSlotQueryMsg {
    int slot = 0; // 查询到的槽位
};

struct AsyncStringMsg {
    std::string text; // 异步文案
};

struct SimSwitchResultMsg {
    bool success = false; // 是否成功
    int target_slot = 0; // 目标槽位
    std::string detail; // 详情
};

struct SimSwitchCtx {
    int target_slot = 0; // 目标槽位
};

/** @brief 取当前 Nt26Board */
Nt26Board* SettingsNetwork_GetNt26Board();
/** @brief 当前是否为蜂窝网络 */
bool SettingsNetwork_IsCellNetwork();
/** @brief SIM 槽位显示名 */
const char* SettingsNetwork_SimSlotName(int slot);
/** @brief 读取缓存 SIM 槽位 */
int SettingsNetwork_GetCachedSimSlot();
/** @brief 写入缓存 SIM 槽位 */
void SettingsNetwork_SetCachedSimSlot(int slot);
/** @brief 将 SIM 槽位写入 NVS */
void SettingsNetwork_PersistSimSlotToNvs(int slot);
/** @brief 调度 SIM 槽位持久化 */
void SettingsNetwork_SchedulePersistSimSlot(int slot);
/** @brief 关闭遮罩 */
void SettingsNetwork_CloseOverlay();
/** @brief 打开遮罩 */
void SettingsNetwork_OpenOverlay(const char* msg);
/** @brief 更新遮罩文案 */
void SettingsNetwork_SetOverlayText(const char* msg);
/** @brief 打开重启倒计时 */
void SettingsNetwork_OpenRestartCountdown(const char* headline);
/** @brief 刷新 SIM 区 UI */
void SettingsNetwork_RefreshSimUi();
/** @brief 调度 SIM 槽位查询 */
void SettingsNetwork_ScheduleSimSlotQuery();
/** @brief 调度从 NVS 加载 SIM 槽 */
void SettingsNetwork_ScheduleLoadSimSlotFromNvs();
