#pragma once

#include "reader/download_gate.h"
#include "reader/reader_types.h"

#include <cstdint>
#include <string>

namespace reader {

struct CloudBookTask {
    std::string task_id;
    std::string book_id;
    std::string name;  // 接口原始 name，如「三国志.ebook」
    std::string title;
    std::string format;
    std::string download_url;
    std::string cover_image_url;  // 详情预览/书架旁路用 coverImageUrl；非列表 RAM 缩略
    uint64_t file_size = 0;
    std::string sha256;
    int page_count = 0;

    std::string FileExtension() const;
    std::string LocalPath() const;
    bool IsDownloaded() const;
};

using CloudDownloadProgressFn = void (*)(int percent, void* user);

bool DownloadCloudBook(const CloudBookTask& task, std::string& err_out,
                       CloudDownloadProgressFn on_progress = nullptr, void* progress_user = nullptr,
                       DownloadGate* gate = nullptr, const uint8_t* cover_bytes = nullptr,
                       size_t cover_len = 0);
bool DeleteCloudBookLocal(const CloudBookTask& task, std::string& err_out);
BookInfo ToBookInfo(const CloudBookTask& task);

std::string FormatCloudFileSize(uint64_t bytes);

}  // namespace reader
