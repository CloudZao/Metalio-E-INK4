#pragma once

#include "settings_test_cell_priv.h"

/** @brief 切换到目标 SIM 槽；失败写 detail */
bool SettingsTestCell_ApplySimSlot(Nt26Board* nt26, int target_slot, char* detail, size_t detail_len);
/** @brief 解析 ECPING 摘要中的 tx/rx 计数 */
bool SettingsTestCell_ParseEcpingSummary(const std::string& resp, int* tx, int* rx);
/** @brief 切槽后是否需等待注网再 ping */
bool SettingsTestCell_NeedsRegistrationWait(int target_slot, int current_slot);
/** @brief 静默恢复原 SIM 槽（不更新 UI） */
void SettingsTestCell_RestoreSimSlotQuiet(Nt26Board* nt26, int target_slot);
/** @brief 应答是否含 IPv4 地址 */
bool SettingsTestCell_HasIpv4Address(const std::string& resp);
/** @brief ECPING 是否判定成功 */
bool SettingsTestCell_IsEcpingSuccess(const std::string& resp);
/** @brief 带重试的 AT 命令（期望 OK） */
bool SettingsTestCell_RunAtOkRetry(Nt26Board* nt26, const std::string& cmd, std::string& resp, uint32_t timeout_ms,
                                   int retries);
/** @brief 应答是否含 ERROR */
bool SettingsTestCell_HasAtError(const std::string& resp);
/** @brief 解析 CEREG 注网状态码；失败返回 -1 */
int SettingsTestCell_ParseCeregStat(const std::string& resp);
/** @brief 软复位模组射频 */
void SettingsTestCell_SoftResetModem(Nt26Board* nt26);
/** @brief 查询当前 SIM 槽号；失败返回 -1 */
int SettingsTestCell_QueryCurrentSimSlot(Nt26Board* nt26);
/** @brief 跑 ECPING 自检；结果写入 detail */
bool SettingsTestCell_RunEcpingTest(Nt26Board* nt26, char* detail, size_t detail_len);
/** @brief 确保模组 AT 通路可用 */
bool SettingsTestCell_EnsureModemAlive(Nt26Board* nt26, char* detail, size_t detail_len);
/** @brief 槽号对应的测试行；无效返回 nullptr */
SettingsTestRow* SettingsTestCell_RowForSlot(int slot);
/** @brief 发送一条 AT 并收应答 */
void SettingsTestCell_RunAt(Nt26Board* nt26, const std::string& cmd, std::string& resp, uint32_t timeout_ms);
/** @brief 等待 SIM 注网完成后再 ping */
bool SettingsTestCell_WaitSimRegisteredForPing(Nt26Board* nt26, char* detail, size_t detail_len,
                                               uint32_t timeout_ms);
/** @brief 连续探测 AT 通路 */
bool SettingsTestCell_TryAtProbeBurst(Nt26Board* nt26, int attempts, uint32_t timeout_ms, uint32_t gap_ms,
                                      std::string& resp, esp_err_t* out_err);
/** @brief 标记已做过射频复位（避免重复） */
void SettingsTestCell_MarkModemRadioReset();
/** @brief 清除射频复位标记 */
void SettingsTestCell_ClearModemRadioReset();
/** @brief 应答是否表示无卡 */
bool SettingsTestCell_IsSimAbsent(const std::string& resp);
/** @brief 单次 AT（期望 OK） */
bool SettingsTestCell_RunAtOk(Nt26Board* nt26, const std::string& cmd, std::string& resp, uint32_t timeout_ms);
/** @brief 探测模组 AT 是否应答 */
bool SettingsTestCell_ProbeModemAt(Nt26Board* nt26, std::string& resp, uint32_t timeout_ms, esp_err_t* out_err);
/** @brief 从 ECSIMCFG 解析槽号；失败返回 -1 */
int SettingsTestCell_ParseSimSlotFromEcsimcfg(const std::string& resp);
/** @brief 应答是否表示 SIM 就绪 */
bool SettingsTestCell_IsSimReady(const std::string& resp);
