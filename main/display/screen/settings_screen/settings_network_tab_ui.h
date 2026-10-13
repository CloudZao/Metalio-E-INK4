#pragma once

#include "settings_network_tab_priv.h"

/** @brief 取当前 Nt26Board（经双网板解引用） */
Nt26Board* SettingsNetwork_GetNt26Board();
/** @brief 关闭网络页遮罩 */
void SettingsNetwork_CloseOverlay();
/** @brief 写入缓存的 SIM 槽位（0=外置，1=内置） */
void SettingsNetwork_SetCachedSimSlot(int slot);
/** @brief 当前网络类型（WiFi/蜂窝） */
NetworkType SettingsNetwork_CurrentNetworkType();
/** @brief 当前是否为蜂窝网络 */
bool SettingsNetwork_IsCellNetwork();
/** @brief 点击 WiFi/4G 选项 */
void SettingsNetwork_OnNetworkOptionClicked(lv_event_t* e);
/** @brief 切换上网类型后台任务（完成后重启） */
void SettingsNetwork_SwitchNetworkTask(void* arg);
/** @brief 刷新 WiFi/4G 选项选中态 */
void SettingsNetwork_RefreshNetworkOptions();
/** @brief 构建「配置 WIFI」区块 */
void SettingsNetwork_BuildWifiConfigSection(lv_obj_t* page);
/** @brief 读取缓存的 SIM 槽位 */
int SettingsNetwork_GetCachedSimSlot();
/** @brief 调度将 SIM 槽位持久化到 NVS */
void SettingsNetwork_SchedulePersistSimSlot(int slot);
/** @brief 打开重启倒计时遮罩 */
void SettingsNetwork_OpenRestartCountdown(const char* headline);
/** @brief 显示/隐藏 SIM 切换区块 */
void SettingsNetwork_SetSimSectionVisible(bool visible);
/** @brief 打开全屏遮罩并显示文案 */
void SettingsNetwork_OpenOverlay(const char* msg);
/** @brief 重启倒计时定时器回调 */
void SettingsNetwork_RebootTimerCb(lv_timer_t* timer);
/** @brief 将 SIM 槽位写入 NVS */
void SettingsNetwork_PersistSimSlotToNvs(int slot);
/** @brief 执行重启的后台任务 */
void SettingsNetwork_RebootTask(void* arg);
/** @brief 更新遮罩文案 */
void SettingsNetwork_SetOverlayText(const char* msg);
/** @brief SIM 槽位显示名 */
const char* SettingsNetwork_SimSlotName(int slot);
/** @brief 仅 WiFi 时显示「配置 WIFI」区块 */
void SettingsNetwork_SetWifiConfigSectionVisible(bool visible);
/** @brief 刷新 SIM 区按钮与当前槽文案 */
void SettingsNetwork_RefreshSimUi();
/** @brief 构建内置/外置 SIM 切换区块 */
void SettingsNetwork_BuildSimSection(lv_obj_t* page);
/** @brief 取 DualNetworkBoard（非双网板返回 nullptr） */
DualNetworkBoard* SettingsNetwork_GetDualBoard();
