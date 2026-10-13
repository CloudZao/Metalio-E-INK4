#pragma once

/**
 * @brief 设置→日志：全屏上传进度（对齐传输下载页交互）
 * @note 上传中暂停激活轮询、SD 日志写盘（不改 NVS）；结束成对恢复。
 *       依次上传 SD：metalio_logs.txt、metalio_coredump.elf（有则传；elf 不读 Flash 槽）。
 *       Worker 栈须内 DRAM（读 SD 关 cache 时禁 PSRAM 栈）。
 */

/** @brief 是否正在全屏上传（含联网/收尾提示） */
bool SettingsLogUpload_IsActive();

/** @brief 点击「上传日志」：未配网仅提示；否则全屏进度并唤醒联网上传 */
void SettingsLogUpload_Start();

/**
 * @brief 请求中断上传并收起进度（取消键/待机/关机等）
 * @note Http::Close/join 丢独立任务；UI 经 lv_async 收起
 */
void SettingsLogUpload_RequestAbort(const char* why);

/** @brief 设置壳销毁或离开日志 Tab：中断并拆 UI（须在 LVGL 线程） */
void SettingsLogUpload_Teardown();

/**
 * @brief 设置页 VkKey：短按 Home/返回均取消并收起进度，不离开设置
 * @return true 已消费
 */
bool SettingsLogUpload_OnVkKey(const char* key);

/** @brief 盖板 Home 长按：收起上传层并回系统首页 */
void SettingsLogUpload_DismissAndGoHome();
