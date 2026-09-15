#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 每日清单缓存：网络拉取后写入，页面只读展示。
#define CHECKLIST_CACHE_TITLE_LEN 96
#define CHECKLIST_CACHE_STATUS_LEN 16
#define CHECKLIST_CACHE_DATE_LEN 16
#define CHECKLIST_CACHE_TIME_LEN 16
#define CHECKLIST_CACHE_ID_LEN 48
#define CHECKLIST_CACHE_MAX_ITEMS 64

typedef struct {
    char id[CHECKLIST_CACHE_ID_LEN];
    char title[CHECKLIST_CACHE_TITLE_LEN];
    char status[CHECKLIST_CACHE_STATUS_LEN];
    char plan_date[CHECKLIST_CACHE_DATE_LEN];
    char plan_time[CHECKLIST_CACHE_TIME_LEN];
} checklist_cache_item_t;

// 清空缓存并开始写入，条目建议存放在 SPIRAM。
void checklist_cache_begin(void);

// 追加一条缓存项；超出最大值时忽略。
bool checklist_cache_append(const char* id, const char* title, const char* status,
                            const char* plan_date, const char* plan_time);

// 结束写入并标记 ready，允许空列表。
void checklist_cache_end(void);

// 是否存在可用缓存；空列表成功拉取也算 ready。
bool checklist_cache_ready(void);

// 拷贝全部条目到输出缓冲。
size_t checklist_cache_copy_all(checklist_cache_item_t* out, size_t max_out);

// 拷贝待办条目到输出缓冲，供待机页展示。
size_t checklist_cache_copy_pending(checklist_cache_item_t* out, size_t max_out);

#ifdef __cplusplus
}
#endif
