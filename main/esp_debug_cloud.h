#pragma once

// ESP Debug Cloud：经 UDP 上报串口/ESP_LOG 文本到调试云。
// 通过 ESP_DEBUG_CLOUD_ENABLE 开关；启用时在 app_main 调用 EspDebugCloud_Start()。
// 钩子内不做 malloc / Queue，失败直接丢包，不阻塞业务。

#ifndef ESP_DEBUG_CLOUD_ENABLE
#define ESP_DEBUG_CLOUD_ENABLE 0
#endif

#ifndef ESP_DEBUG_CLOUD_SERVER
#define ESP_DEBUG_CLOUD_SERVER "192.168.5.67:19527"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** 安装日志钩子并启动后台发送任务。可重复调用，仅首次生效。 */
void EspDebugCloud_Start(void);

#ifdef __cplusplus
}
#endif
