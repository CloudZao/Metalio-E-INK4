#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 SD 日志：读 NVS（默认开），装 vprintf 钩子与写盘任务
 * @note 无卡或卸载期间不记；挂载后写入 metalio/e-ink/logs/metalio_logs.txt（环形 256KB）。
 *       开机若 Flash 有 coredump 且本开关开启，覆盖写入 metalio_coredump.elf 后擦 Flash 槽。
 */
void SdLogWriter_Start(void);

/**
 * @brief SD 已挂载：确保目录并打开日志文件
 */
void SdLogWriter_OnSdMounted(void);

/**
 * @brief SD 将卸载：关文件并暂停采日志（虚拟 U 盘期间不写环、不落盘）
 */
void SdLogWriter_OnSdUnmounting(void);

/** @brief 是否开启写 SD 日志 */
bool SdLogWriter_IsEnabled(void);

/**
 * @brief 设置是否写 SD 日志（异步写 NVS）
 * @param enabled true 开启，false 仅停止写入不删文件
 */
void SdLogWriter_SetEnabled(bool enabled);

/**
 * @brief 将环形缓冲刷入 SD 并关闭写句柄，便于随后只读打开日志文件上传
 * @note FatFS 仅 fflush 时目录 size 常仍为 0，须 fclose 后二次打开才能读到内容
 */
void SdLogWriter_Flush(void);

/**
 * @brief 上传占用开始：停采环/写盘并刷盘关句柄（不改 NVS 用户开关）
 * @note 可重入计数；须与 EndUploadHold 成对；与 SetEnabled 并发时以用户开关为准、hold 优先挡写
 */
void SdLogWriter_BeginUploadHold(void);

/**
 * @brief 上传占用结束：解除挡写；若用户开关仍开则重新打开日志文件
 */
void SdLogWriter_EndUploadHold(void);

/**
 * @brief 若 Flash 有有效 coredump、SD 已挂且日志开启，则覆盖写入 SD 后擦除 Flash 槽
 * @note 目标文件 metalio/e-ink/logs/metalio_coredump.elf（仅保留最新一份）；失败不擦 Flash
 */
void SdLogWriter_ExportCoreDumpIfNeeded(void);

#ifdef __cplusplus
}
#endif
