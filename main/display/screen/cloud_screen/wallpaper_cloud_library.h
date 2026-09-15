#pragma once

#include "book_screen/book_cloud_library.h"

#include <cstdint>
#include <string>
#include <vector>

namespace reader {

struct CloudWallpaperTask {
    std::string task_id;
    std::string badge_image_id;
    std::string image_name;          // 列表展示
    std::string original_filename;   // 原始文件名（用于组成落盘名）
    std::string download_url;
    uint64_t file_size = 0;
    std::string sha256;

    std::string LocalPath() const;
    bool IsDownloaded() const;
};

/** 列表标题：imageName_originalFilename（缺字段时有合理回退） */
std::string FormatWallpaperItemTitle(const std::string& image_name,
                                     const std::string& original_filename);
bool ReadWallpaperMeta(const char* wallpaper_path, std::string& image_name_out,
                       std::string& original_filename_out);
bool WriteWallpaperMeta(const char* wallpaper_path, const std::string& image_name,
                        const std::string& original_filename);
void DeleteWallpaperMeta(const char* wallpaper_path);

bool DownloadCloudWallpaper(const CloudWallpaperTask& task, std::string& err_out,
                            CloudDownloadProgressFn on_progress = nullptr,
                            void* progress_user = nullptr, DownloadGate* gate = nullptr);
bool DeleteCloudWallpaperLocal(const CloudWallpaperTask& task, std::string& err_out);

}  // namespace reader
