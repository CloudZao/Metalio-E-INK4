#pragma once

#include "settings_network_tab_priv.h"

/** @brief 调度 SIM 卡槽切换任务 */
void SettingsNetwork_ScheduleSimSwitch(int target_slot);
/** @brief LV 异步：SIM 切换完成回写 UI */
void SettingsNetwork_AsyncSimSwitchDone(void* user_data);
/** @brief 投递 SIM 切换结果到 LVGL 线程 */
void SettingsNetwork_PostSimSwitchDone(struct SimSwitchResultMsg* result);
/** @brief 点击「配置 WIFI」进入扫网页 */
void SettingsNetwork_OnEnterWifiConfigClicked(lv_event_t* e);
/** @brief 点击内置 SIM */
void SettingsNetwork_OnSimIntClicked(lv_event_t* e);
/** @brief SIM 切换后台任务入口 */
void SettingsNetwork_SimSwitchTask(void* arg);
/** @brief LV 异步：SIM 槽位查询完成 */
void SettingsNetwork_AsyncSimSlotQueried(void* user_data);
/** @brief 调度一次 SIM 槽位查询 */
void SettingsNetwork_ScheduleSimSlotQuery();
/** @brief LV 异步：刷新 SIM 区 UI */
void SettingsNetwork_AsyncRefreshSimUi(void* user_data);
/** @brief 调度从 NVS 加载 SIM 槽缓存 */
void SettingsNetwork_ScheduleLoadSimSlotFromNvs();
/** @brief 投递 SIM 切换进度文案 */
void SettingsNetwork_PostSimProgress(const std::string& text);
/** @brief LV 异步：显示 SIM 进度文案 */
void SettingsNetwork_AsyncSimProgress(void* user_data);
/** @brief 点击外置 SIM */
void SettingsNetwork_OnSimExtClicked(lv_event_t* e);
/** @brief 从 ECSIMCFG 响应解析槽位号 */
int SettingsNetwork_ParseSimSlotFromEcsimcfg(const std::string& resp);
/** @brief SIM 槽位查询后台任务 */
void SettingsNetwork_SimSlotQueryTask(void* arg);
