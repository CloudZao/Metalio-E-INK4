#pragma once

#include "settings_test_common.h"

#include <cstdint>
#include <string>

constexpr const char* kTag = "SettingsTestCell"; // 日志标签
constexpr int kPingCount = 3;                    // ECPING 次数
constexpr int kPingSize = 32;                    // ECPING 包长
constexpr int kPingDelayMs = 15000;              // ECPING 间隔
constexpr uint32_t kEcpingTimeoutMs =
    static_cast<uint32_t>(kPingCount) * kPingDelayMs + 20000; // ECPING 总超时
constexpr uint32_t kSwitchRegWaitMs = 30000;     // 切槽后等注网
constexpr uint32_t kAtShortMs = 3000;            // 短 AT 超时
constexpr uint32_t kAtMediumMs = 8000;           // 中等 AT 超时
constexpr uint32_t kAtLongMs = 12000;            // 长 AT 超时
constexpr uint32_t kAtProbeMs = 800;             // AT 探测超时
constexpr uint32_t kPostCfunSettleMs = 40000;    // CFUN 后稳定等待
constexpr uint32_t kModemResetSettleMs = 1500;   // 模组复位后等待
constexpr int kAtProbeRetries = 5;              // AT 探测重试
constexpr int kAtProbeAfterResetRetries = 12;   // 复位后探测重试
constexpr int kAtDefaultRetries = 3;            // 默认 AT 重试
constexpr uint32_t kCellTaskStack = 8192;        // 自检任务栈
constexpr int kCellCreateMaxRetry = 5;          // 建链最大重试
constexpr uint32_t kCellCreateRetryMs = 1200;    // 建链重试间隔

extern int SettingsTestCell_pending_slot; // 自动序列待测槽
extern int SettingsTestCell_create_retry; // 当前建链重试计数

struct SettingsTestCellDoneMsg {
    int slot = 0;           // SIM 槽
    bool pass = false;      // 是否通过
    char detail[64] = {};   // 简短结果文案
};

/** @brief 确保模组 AT 通路可用 */
bool SettingsTestCell_EnsureModemAlive(Nt26Board* nt26, char* detail, size_t detail_len);
/** @brief 查询当前 SIM 槽号；失败返回 -1 */
int SettingsTestCell_QueryCurrentSimSlot(Nt26Board* nt26);
/** @brief 清除射频复位标记 */
void SettingsTestCell_ClearModemRadioReset();
/** @brief 切槽后是否需等待注网再 ping */
bool SettingsTestCell_NeedsRegistrationWait(int target_slot, int current_slot);
/** @brief 等待 SIM 注网完成后再 ping */
bool SettingsTestCell_WaitSimRegisteredForPing(Nt26Board* nt26, char* detail, size_t detail_len,
                                               uint32_t timeout_ms);
/** @brief 切换到目标 SIM 槽；失败写 detail */
bool SettingsTestCell_ApplySimSlot(Nt26Board* nt26, int target_slot, char* detail, size_t detail_len);
/** @brief 静默恢复原 SIM 槽 */
void SettingsTestCell_RestoreSimSlotQuiet(Nt26Board* nt26, int target_slot);
/** @brief 跑 ECPING 自检 */
bool SettingsTestCell_RunEcpingTest(Nt26Board* nt26, char* detail, size_t detail_len);
/** @brief 槽号对应的测试行 */
SettingsTestRow* SettingsTestCell_RowForSlot(int slot);
