#pragma once

#include <cstdint>

#include <lvgl.h>

#include "reader/reader.h"

constexpr lv_coord_t kDetailCtaBottom = 24; // 继续阅读距屏幕底（+8）
constexpr lv_coord_t kDetailProgCardH = 88; // 进度卡固定高（标题与百分比同行）
constexpr lv_coord_t kDetailTitleLines = 1; // 书名单行省略；其余控件整体上移一行
constexpr lv_coord_t kDetailSectionGap = 8; // 详情纵距收紧
constexpr uint32_t kDetailCoverWorkerStack = 16 * 1024;

/** @brief 创建书籍详情页 */
lv_obj_t* CreateDetailScreen(int book_index);
/** @brief 恢复详情屏 */
lv_obj_t* ResumeDetailScreen();
/** @brief 异步打开详情 */
void OpenDetailAsync(void* user_data);
/** @brief 异步返回详情 */
void BackToDetailAsync(void* user_data);
/** @brief 清空详情封面控件指针 */
void ClearDetailCoverUiPtrs();
/** @brief 无旁路时后台读文件内封面；有旁路则 CreateDetailScreen 已同步显示。 */
void StartDetailCoverWorker(int book_index);
/** @brief 详情 vk_home / vk_prev → 书架或阅读首页。 */
void CancelDetailCoverLoad();
/** @brief 详情封面占位 */
void FillDetailCoverPlaceholder(lv_obj_t* host, reader::BookFormat format);
/** @brief 把封面位图应用到详情槽 */
void ApplyDetailCoverImage();
/** @brief 刷新详情书名/作者等标签 */
void UpdateDetailMetaLabels(const reader::BookInfo& shown);
/** @brief 详情「开始阅读」 */
void OnDetailStartClicked(lv_event_t* e);
