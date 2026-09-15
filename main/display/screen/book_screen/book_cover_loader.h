#pragma once

#include "reader.h"

namespace book_ui {

// 封面统一策略（快显 + 槽位尺寸正确）：
// - 同步：旁路 .a2i1 秒开（够大则定稿；偏小也可先预览）
// - 异步：旁路不够当前槽 → 权威档抽内嵌升级旁路，再缩到槽位

/** 仅旁路且尺寸足够；不 Open 文档。 */
bool TryLoadBookDetailSidecar(const reader::BookInfo& info, int max_w, int max_h,
                              reader::RasterImage& out);

/**
 * @brief 有旁路即读出缩放到槽（可不盖满）；用于详情先出图再后台升级
 */
bool TryLoadBookSidecarPreview(const reader::BookInfo& info, int max_w, int max_h,
                               reader::RasterImage& out);

/** 旁路优先，失败再读文件内封面并回写旁路（可能 Open EPUB/EBOOK；须在 worker）。 */
bool LoadBookDetailCover(reader::BookInfo& info, int max_w, int max_h, reader::RasterImage& out);

}  // namespace book_ui
