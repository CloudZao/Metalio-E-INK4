#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHECKLIST_CACHE_TITLE_LEN 96 // 标题缓冲长度
#define CHECKLIST_CACHE_STATUS_LEN 16 // 状态串长度
#define CHECKLIST_CACHE_DATE_LEN 16 // 计划日期长度
#define CHECKLIST_CACHE_TIME_LEN 16 // 计划时间长度
#define CHECKLIST_CACHE_ID_LEN 48 // 条目 ID 长度
#define CHECKLIST_CACHE_MAX_ITEMS 64 // 最大缓存条数

typedef struct {
    char id[CHECKLIST_CACHE_ID_LEN]; // 条目 ID
    char title[CHECKLIST_CACHE_TITLE_LEN]; // 标题
    char status[CHECKLIST_CACHE_STATUS_LEN]; // 状态（pending/done 等）
    char plan_date[CHECKLIST_CACHE_DATE_LEN]; // 计划日期
    char plan_time[CHECKLIST_CACHE_TIME_LEN]; // 计划时间
} checklist_cache_item_t;

/** @brief 清空缓存并开始写入（条目建议放 SPIRAM） */
void checklist_cache_begin(void);

/** @brief 追加一条缓存项；超出上限时忽略并返回 false */
bool checklist_cache_append(const char* id, const char* title, const char* status,
                            const char* plan_date, const char* plan_time);

/** @brief 结束写入并标记 ready（允许空列表） */
void checklist_cache_end(void);

/** @brief 是否存在可用缓存；空列表成功拉取也算 ready */
bool checklist_cache_ready(void);

/** @brief 拷贝全部条目到输出缓冲 */
size_t checklist_cache_copy_all(checklist_cache_item_t* out, size_t max_out);

/** @brief 拷贝待办条目到输出缓冲（供待机页） */
size_t checklist_cache_copy_pending(checklist_cache_item_t* out, size_t max_out);

#ifdef __cplusplus
}
#endif
