#pragma once

#include "settings_test_wifi_priv.h"

/** @brief 创建弹层按钮 */
lv_obj_t* SettingsTestWifi_MakePopupBtn(lv_obj_t* parent, const char* text, lv_event_cb_t cb);
/** @brief 扫描完成事件捕获回调 */
void SettingsTestWifi_OnScanDoneCapture(void* arg, esp_event_base_t base, int32_t id, void* data);
/** @brief 若本页拥有 WiFi 栈则拆除 */
void SettingsTestWifi_WifiTeardownIfOwned();
/** @brief WiFi 测试行点击 */
void SettingsTestWifi_OnWifiRowClicked(lv_event_t* e);
/** @brief 确保 WiFi 已启动可供扫描 */
bool SettingsTestWifi_WifiEnsureForScan();
/** @brief 打开 WiFi 结果弹层 */
void SettingsTestWifi_OpenWifiPopup();
/** @brief 将扫描记录填入 UI AP 列表 */
void SettingsTestWifi_FillUiApsFromRecords(const std::vector<wifi_ap_record_t>& records);
/** @brief 关闭 WiFi 结果弹层 */
void SettingsTestWifi_CloseWifiPopup();
/** @brief 弹层内重新扫描 */
void SettingsTestWifi_OnWifiRescan(lv_event_t* e);
/** @brief 按当前页重建 AP 列表 */
void SettingsTestWifi_RebuildWifiListPage();
/** @brief 启用/禁用重新扫描按钮 */
void SettingsTestWifi_SetWifiRescanEnabled(bool enabled);
/** @brief 弹层关闭按钮 */
void SettingsTestWifi_OnWifiClose(lv_event_t* e);
/** @brief AP 列表上一页 */
void SettingsTestWifi_OnWifiPrev(lv_event_t* e);
/** @brief AP 列表下一页 */
void SettingsTestWifi_OnWifiNext(lv_event_t* e);
/** @brief AP 列表总页数 */
int SettingsTestWifi_WifiPageCount();
