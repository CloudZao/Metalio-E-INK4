#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// 阅读偏好（NVS namespace "reader"）：
// - font：SD fonts 目录下 .ef 文件名
// - spacing：段落行距档位（行距+段距成套）
// - margin：页边距档位（左右/上下成套）
// - hide_prog：是否隐藏正文底部章节进度
// - underline：正文下划线 0关 / 1实线 / 2虚线
// 读走 RAM 缓存；首次 Ensure 须在 DRAM 栈；落盘在内部 RAM 栈任务。

enum {
    kBookReaderSpacingPresetCount = 4,
    kBookReaderMarginPresetCount = 4,
    kBookReaderUnderlineOff = 0,
    kBookReaderUnderlineSolid = 1,
    kBookReaderUnderlineDashed = 2,
    kBookReaderUnderlineModeCount = 3,
};

void BookReaderPrefsEnsureLoaded(void);

/** 当前字体文件名（仅 basename）；未 Ensure 时返回默认值。永不返回 NULL。 */
const char* BookReaderPrefsFontFile(void);
void BookReaderPrefsSetFontFile(const char* filename);
char* BookReaderPrefsFontFullPath(char* out, size_t out_len);

int BookReaderPrefsSpacingPreset(void);
void BookReaderPrefsSetSpacingPreset(int preset);
const char* BookReaderPrefsSpacingLabel(int preset);
int BookReaderPrefsLineGap(void);
int BookReaderPrefsParaGap(void);
void BookReaderPrefsSpacingGaps(int preset, int* line_gap, int* para_gap);

/** 页边距档位 0..N-1 */
int BookReaderPrefsMarginPreset(void);
void BookReaderPrefsSetMarginPreset(int preset);
const char* BookReaderPrefsMarginLabel(int preset);
int BookReaderPrefsMarginLeft(void);
int BookReaderPrefsMarginRight(void);
int BookReaderPrefsMarginTop(void);
int BookReaderPrefsMarginBottom(void);
void BookReaderPrefsMarginBox(int preset, int* left, int* right, int* top, int* bottom);

/** true=隐藏正文底部章节进度条文案 */
int BookReaderPrefsHideProgress(void);
void BookReaderPrefsSetHideProgress(int hide);

/** 正文下划线：kBookReaderUnderlineOff / Solid / Dashed */
int BookReaderPrefsUnderlineMode(void);
void BookReaderPrefsSetUnderlineMode(int mode);

#ifdef __cplusplus
}
#endif
