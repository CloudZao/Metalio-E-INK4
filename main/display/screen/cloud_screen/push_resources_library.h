#pragma once

#include "book_screen/book_cloud_library.h"

#include <cstdint>
#include <string>
#include <vector>

namespace reader {

enum class PushResourceType : uint8_t {
    kUnknown = 0,
    kBook = 1,
    kBadge = 2,
    kFont = 3,
};

struct CloudPushResource {
    PushResourceType type = PushResourceType::kUnknown;
    std::string task_id;
    std::string resource_id;
    std::string name;           // 列表展示名（接口 name）
    std::string resource_name;  // 人类可读名（可选）
    std::string download_url;
    std::string cover_image_url;  // coverImageUrl：列表一次拉字节缓存；缩略/详情/旁路共用
    uint64_t file_size = 0;
    std::string sha256;
    std::string format;
    int page_count = 0;
    std::string font_style;
    int font_weight = 0;

    /** 书籍 / 壁纸 / 字体 */
    const char* TypeLabel() const;
    /** 列表/详情缩略 URL：仅 coverImageUrl（壁纸也不回退 downloadUrl，避免当完整图下） */
    const std::string& CoverThumbUrl() const;
    /** 书籍 / 壁纸 / 字体 有封面 URL 时显示缩略图 */
    bool HasCoverThumb() const;
    std::string LocalPath() const;
    bool IsDownloaded() const;
};

bool FetchPushResources(std::vector<CloudPushResource>& out, std::string& err_out,
                        DownloadGate* gate = nullptr);
bool DownloadPushResource(const CloudPushResource& item, std::string& err_out,
                          CloudDownloadProgressFn on_progress = nullptr,
                          void* progress_user = nullptr, DownloadGate* gate = nullptr,
                          const uint8_t* cover_bytes = nullptr, size_t cover_len = 0);
bool DeletePushResourceLocal(const CloudPushResource& item, std::string& err_out);
/** DELETE push-resources，body 使用 taskId（下载成功回执 / 用户删除推送共用） */
bool DeletePushResourceRemote(const CloudPushResource& item, std::string& err_out);
inline bool AckPushResourceDownloaded(const CloudPushResource& item, std::string& err_out) {
    return DeletePushResourceRemote(item, err_out);
}

}  // namespace reader
