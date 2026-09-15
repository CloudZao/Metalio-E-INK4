#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace reader {

// TXT 目录条目（显示行索引 → 打开后绑定 first_page；first_byte_off 供编排中定位）
struct TocEntry {
    std::string title;
    uint32_t first_line = 0;      // 对应 txt_lines_ 下标
    uint32_t first_byte_off = 0;  // 源文件字节锚点
    int first_page = 0;           // 绑定后的页码
};

// 判断一行 UTF-8 正文是否像章节标题（调用方保证：顶格、已去首尾空白）。
// 覆盖：第N章/回/节/集/话/篇、第N卷/部、卷N、序章/楔子/番外、
// Chapter/Part/Volume、纯数字短章名（如「1清和宫上」）。
// 与 tools/ebook/epdbook/chapter_detect.py 规则对齐（设备侧无正则，手写匹配）。
bool IsTxtChapterTitleLine(const char* utf8, size_t len);

inline bool IsTxtChapterTitleLine(const std::string& s) {
    return IsTxtChapterTitleLine(s.data(), s.size());
}

}  // namespace reader
