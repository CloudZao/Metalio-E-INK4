#include "book_screen/book_reader_prefs.h"

#include "assets/lang_config.h"
#include "sd_paths.h"
#include "settings.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr const char* TAG = "BookPrefs";
constexpr const char* kNvsNs = "reader";
constexpr const char* kNvsFontKey = "font";
constexpr const char* kNvsSpacingKey = "spacing";
constexpr const char* kNvsMarginKey = "margin";
constexpr const char* kNvsHideProgKey = "hide_prog";
constexpr const char* kNvsUnderlineKey = "underline";
constexpr const char* kDefaultFile = "misans_25_2.ef";
constexpr size_t kMaxFileLen = 63;
constexpr int kDefaultSpacingPreset = 1;  // 标准
constexpr int kDefaultMarginPreset = 1;   // 标准（约原 kListPad=12）

struct SpacingPreset {
    int line_gap;
    int para_gap;
};

struct MarginPreset {
    int left;
    int right;
    int top;
    int bottom;
};

constexpr SpacingPreset kSpacingTable[kBookReaderSpacingPresetCount] = {
    {2, 12},
    {4, 24},
    {6, 36},
    {8, 48},
};

// 成套页边距：与分页 viewport 共用，避免只改 UI pad 导致裁切/留白不一致
constexpr MarginPreset kMarginTable[kBookReaderMarginPresetCount] = {
    {8, 8, 8, 6},
    {12, 12, 12, 8},
    {20, 20, 18, 12},
    {28, 28, 24, 16},
};

char s_font_file[kMaxFileLen + 1] = {};
int s_spacing_preset = kDefaultSpacingPreset;
int s_margin_preset = kDefaultMarginPreset;
int s_hide_prog = 0;
int s_underline = 0;
bool s_ready = false;

bool IsSafeBasename(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    const size_t n = std::strlen(name);
    if (n == 0 || n > kMaxFileLen) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        const char c = name[i];
        if (c == '/' || c == '\\' || c == ':' || c == '\0') {
            return false;
        }
    }
    return true;
}

int ClampSpacing(int preset) {
    if (preset < 0 || preset >= kBookReaderSpacingPresetCount) {
        return kDefaultSpacingPreset;
    }
    return preset;
}

int ClampMargin(int preset) {
    if (preset < 0 || preset >= kBookReaderMarginPresetCount) {
        return kDefaultMarginPreset;
    }
    return preset;
}

int ClampUnderline(int mode) {
    if (mode < 0 || mode >= kBookReaderUnderlineModeCount) {
        return kBookReaderUnderlineOff;
    }
    return mode;
}

void LoadFromNvs() {
    Settings settings(kNvsNs, false);
    const std::string v = settings.GetString(kNvsFontKey, kDefaultFile);
    if (IsSafeBasename(v.c_str())) {
        std::snprintf(s_font_file, sizeof(s_font_file), "%s", v.c_str());
    } else {
        std::snprintf(s_font_file, sizeof(s_font_file), "%s", kDefaultFile);
    }
    s_spacing_preset = ClampSpacing(settings.GetInt(kNvsSpacingKey, kDefaultSpacingPreset));
    s_margin_preset = ClampMargin(settings.GetInt(kNvsMarginKey, kDefaultMarginPreset));
    s_hide_prog = settings.GetInt(kNvsHideProgKey, 0) != 0 ? 1 : 0;
    s_underline = ClampUnderline(settings.GetInt(kNvsUnderlineKey, 0));
    s_ready = true;
    ESP_LOGI(TAG, "nvs load font=%s spacing=%d margin=%d hide_prog=%d underline=%d", s_font_file,
             s_spacing_preset, s_margin_preset, s_hide_prog, s_underline);
}

enum class PersistKind : uint8_t {
    kFont = 0,
    kSpacing = 1,
    kMargin = 2,
    kHideProg = 3,
    kUnderline = 4,
};

struct PersistArg {
    PersistKind kind = PersistKind::kFont;
    char file[kMaxFileLen + 1] = {};
    int ival = 0;
};

void PersistTask(void* arg) {
    auto* p = static_cast<PersistArg*>(arg);
    Settings settings(kNvsNs, true);
    switch (p->kind) {
        case PersistKind::kFont:
            settings.SetString(kNvsFontKey, p->file);
            ESP_LOGI(TAG, "nvs save font=%s", p->file);
            break;
        case PersistKind::kSpacing:
            settings.SetInt(kNvsSpacingKey, p->ival);
            ESP_LOGI(TAG, "nvs save spacing=%d", p->ival);
            break;
        case PersistKind::kMargin:
            settings.SetInt(kNvsMarginKey, p->ival);
            ESP_LOGI(TAG, "nvs save margin=%d", p->ival);
            break;
        case PersistKind::kHideProg:
            settings.SetInt(kNvsHideProgKey, p->ival);
            ESP_LOGI(TAG, "nvs save hide_prog=%d", p->ival);
            break;
        case PersistKind::kUnderline:
            settings.SetInt(kNvsUnderlineKey, p->ival);
            ESP_LOGI(TAG, "nvs save underline=%d", p->ival);
            break;
    }
    delete p;
    vTaskDelete(nullptr);
}

bool StartPersist(PersistArg* arg) {
    if (arg == nullptr) {
        return false;
    }
    if (xTaskCreate(PersistTask, "book_prefs_nvs", 4096, arg, 5, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate(book_prefs_nvs) failed");
        delete arg;
        return false;
    }
    return true;
}

}  // namespace

void BookReaderPrefsEnsureLoaded(void) {
    if (!s_ready) {
        LoadFromNvs();
    }
}

const char* BookReaderPrefsFontFile(void) {
    if (!s_ready) {
        ESP_LOGW(TAG, "font cache not ready; using default=%s", kDefaultFile);
        return kDefaultFile;
    }
    return s_font_file[0] != '\0' ? s_font_file : kDefaultFile;
}

void BookReaderPrefsSetFontFile(const char* filename) {
    if (!IsSafeBasename(filename)) {
        ESP_LOGW(TAG, "reject bad font name");
        return;
    }
    std::snprintf(s_font_file, sizeof(s_font_file), "%s", filename);
    s_ready = true;

    auto* arg = new PersistArg{};
    arg->kind = PersistKind::kFont;
    std::snprintf(arg->file, sizeof(arg->file), "%s", s_font_file);
    (void)StartPersist(arg);
}

char* BookReaderPrefsFontFullPath(char* out, size_t out_len) {
    if (out == nullptr || out_len < 16) {
        return nullptr;
    }
    const char* file = BookReaderPrefsFontFile();
    const int n = std::snprintf(out, out_len, "%s/%s", SD_PATH_FONTS, file);
    if (n <= 0 || static_cast<size_t>(n) >= out_len) {
        return nullptr;
    }
    return out;
}

int BookReaderPrefsSpacingPreset(void) {
    if (!s_ready) {
        return kDefaultSpacingPreset;
    }
    return ClampSpacing(s_spacing_preset);
}

void BookReaderPrefsSetSpacingPreset(int preset) {
    if (preset < 0 || preset >= kBookReaderSpacingPresetCount) {
        ESP_LOGW(TAG, "reject bad spacing preset=%d", preset);
        return;
    }
    s_spacing_preset = preset;
    s_ready = true;
    auto* arg = new PersistArg{};
    arg->kind = PersistKind::kSpacing;
    arg->ival = s_spacing_preset;
    (void)StartPersist(arg);
}

const char* BookReaderPrefsSpacingLabel(int preset) {
    switch (ClampSpacing(preset)) {
        case 0:
            return Lang::Strings::BOOK_SPACING_COMPACT;
        case 1:
            return Lang::Strings::BOOK_SPACING_STANDARD;
        case 2:
            return Lang::Strings::BOOK_SPACING_RELAXED;
        case 3:
            return Lang::Strings::BOOK_SPACING_VERY_RELAXED;
        default:
            return Lang::Strings::BOOK_SPACING_STANDARD;
    }
}

void BookReaderPrefsSpacingGaps(int preset, int* line_gap, int* para_gap) {
    const SpacingPreset& sp = kSpacingTable[ClampSpacing(preset)];
    if (line_gap != nullptr) {
        *line_gap = sp.line_gap;
    }
    if (para_gap != nullptr) {
        *para_gap = sp.para_gap;
    }
}

int BookReaderPrefsLineGap(void) {
    int line = kSpacingTable[kDefaultSpacingPreset].line_gap;
    BookReaderPrefsSpacingGaps(BookReaderPrefsSpacingPreset(), &line, nullptr);
    return line;
}

int BookReaderPrefsParaGap(void) {
    int para = kSpacingTable[kDefaultSpacingPreset].para_gap;
    BookReaderPrefsSpacingGaps(BookReaderPrefsSpacingPreset(), nullptr, &para);
    return para;
}

int BookReaderPrefsMarginPreset(void) {
    if (!s_ready) {
        return kDefaultMarginPreset;
    }
    return ClampMargin(s_margin_preset);
}

void BookReaderPrefsSetMarginPreset(int preset) {
    if (preset < 0 || preset >= kBookReaderMarginPresetCount) {
        ESP_LOGW(TAG, "reject bad margin preset=%d", preset);
        return;
    }
    s_margin_preset = preset;
    s_ready = true;
    auto* arg = new PersistArg{};
    arg->kind = PersistKind::kMargin;
    arg->ival = s_margin_preset;
    (void)StartPersist(arg);
}

const char* BookReaderPrefsMarginLabel(int preset) {
    switch (ClampMargin(preset)) {
        case 0:
            return Lang::Strings::BOOK_MARGIN_NARROW;
        case 1:
            return Lang::Strings::BOOK_MARGIN_STANDARD;
        case 2:
            return Lang::Strings::BOOK_MARGIN_WIDE;
        case 3:
            return Lang::Strings::BOOK_MARGIN_VERY_WIDE;
        default:
            return Lang::Strings::BOOK_MARGIN_STANDARD;
    }
}

void BookReaderPrefsMarginBox(int preset, int* left, int* right, int* top, int* bottom) {
    const MarginPreset& m = kMarginTable[ClampMargin(preset)];
    if (left != nullptr) {
        *left = m.left;
    }
    if (right != nullptr) {
        *right = m.right;
    }
    if (top != nullptr) {
        *top = m.top;
    }
    if (bottom != nullptr) {
        *bottom = m.bottom;
    }
}

int BookReaderPrefsMarginLeft(void) {
    int v = kMarginTable[kDefaultMarginPreset].left;
    BookReaderPrefsMarginBox(BookReaderPrefsMarginPreset(), &v, nullptr, nullptr, nullptr);
    return v;
}

int BookReaderPrefsMarginRight(void) {
    int v = kMarginTable[kDefaultMarginPreset].right;
    BookReaderPrefsMarginBox(BookReaderPrefsMarginPreset(), nullptr, &v, nullptr, nullptr);
    return v;
}

int BookReaderPrefsMarginTop(void) {
    int v = kMarginTable[kDefaultMarginPreset].top;
    BookReaderPrefsMarginBox(BookReaderPrefsMarginPreset(), nullptr, nullptr, &v, nullptr);
    return v;
}

int BookReaderPrefsMarginBottom(void) {
    int v = kMarginTable[kDefaultMarginPreset].bottom;
    BookReaderPrefsMarginBox(BookReaderPrefsMarginPreset(), nullptr, nullptr, nullptr, &v);
    return v;
}

int BookReaderPrefsHideProgress(void) {
    if (!s_ready) {
        return 0;
    }
    return s_hide_prog != 0 ? 1 : 0;
}

void BookReaderPrefsSetHideProgress(int hide) {
    s_hide_prog = hide != 0 ? 1 : 0;
    s_ready = true;
    auto* arg = new PersistArg{};
    arg->kind = PersistKind::kHideProg;
    arg->ival = s_hide_prog;
    (void)StartPersist(arg);
}

int BookReaderPrefsUnderlineMode(void) {
    if (!s_ready) {
        return kBookReaderUnderlineOff;
    }
    return ClampUnderline(s_underline);
}

void BookReaderPrefsSetUnderlineMode(int mode) {
    if (mode < 0 || mode >= kBookReaderUnderlineModeCount) {
        ESP_LOGW(TAG, "reject bad underline mode=%d", mode);
        return;
    }
    s_underline = mode;
    s_ready = true;
    auto* arg = new PersistArg{};
    arg->kind = PersistKind::kUnderline;
    arg->ival = s_underline;
    (void)StartPersist(arg);
}
