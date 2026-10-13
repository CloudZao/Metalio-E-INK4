// network_keyboard.cc — split from parent .cc
#include "network_screen/network_screen.h"
#include "network_screen_priv.h"
#include "network_keyboard.h"
#include "network_screen/network_screen_priv.h"

#include "application.h"
#include "assets/lang_config.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "power_policy.h"
#include "screen_common.h"
#include "ssid_manager.h"
#include "vk_key_handler.h"
#include "wifi_station.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

void Network_RefreshPasswordDisplay() {
    if (Network_State().ui.pwd_lbl == nullptr) {
        return;
    }
    if (Network_State().password[0] == '\0') {
        lv_label_set_text(Network_State().ui.pwd_lbl, Lang::Strings::NETWORK_PWD_PLACEHOLDER);
        lv_obj_set_style_text_opa(Network_State().ui.pwd_lbl, LV_OPA_50, 0);
    } else {
        lv_label_set_text(Network_State().ui.pwd_lbl, Network_State().password);
        lv_obj_set_style_text_opa(Network_State().ui.pwd_lbl, LV_OPA_COVER, 0);
    }
}

void Network_AppendToPassword(const char* text) {
    if (Network_State().ui.pwd_lbl == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }
    const size_t cur = std::strlen(Network_State().password);
    const size_t add = std::strlen(text);
    if (add == 0 || cur + add > kMaxPasswordLen) {
        return;
    }
    std::memcpy(Network_State().password + cur, text, add + 1);
    Network_RefreshPasswordDisplay();
}

void Network_BackspacePassword() {
    size_t len = std::strlen(Network_State().password);
    if (len == 0) {
        return;
    }
    // UTF-8：退掉末尾完整码点（中文标点等）
    do {
        --len;
    } while (len > 0 && (static_cast<unsigned char>(Network_State().password[len]) & 0xC0) == 0x80);
    Network_State().password[len] = '\0';
    Network_RefreshPasswordDisplay();
}

void Network_ClearPassword() {
    Network_State().password[0] = '\0';
    Network_RefreshPasswordDisplay();
}

void Network_OnBackspaceLongPressed(lv_event_t* /*e*/) {
    Network_ClearPassword();
}

static int Network_KbCols() {
    return Network_State().kb_layout == KbLayoutStyle::kGrid ? kKbGridCols : kKbQwertyCols;
}

static int Network_KbRows() {
    return Network_State().kb_layout == KbLayoutStyle::kGrid ? kKbGridRows : kKbQwertyRows;
}

static void Network_RefreshKbLayoutToggleUi() {
    auto paint = [](lv_obj_t* btn, bool on) {
        if (btn == nullptr || !lv_obj_is_valid(btn)) {
            return;
        }
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        lv_obj_set_style_bg_color(btn, on ? lv_color_black() : lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if (lbl != nullptr && lv_obj_is_valid(lbl)) {
            lv_obj_set_style_text_color(lbl, on ? lv_color_white() : lv_color_black(), 0);
        }
    };
    const bool qwerty = (Network_State().kb_layout != KbLayoutStyle::kGrid);
    paint(Network_State().ui.pwd_kb_qwerty_btn, qwerty);
    paint(Network_State().ui.pwd_kb_grid_btn, !qwerty);
}

static void Network_OnKbLayoutQwertyClicked(lv_event_t* /*e*/) {
    if (Network_State().kb_layout == KbLayoutStyle::kQwerty) {
        return;
    }
    Network_State().kb_layout = KbLayoutStyle::kQwerty;
    Network_RefreshKbLayoutToggleUi();
    Network_RebuildCustomKeyboard();
}

static void Network_OnKbLayoutGridClicked(lv_event_t* /*e*/) {
    if (Network_State().kb_layout == KbLayoutStyle::kGrid) {
        return;
    }
    Network_State().kb_layout = KbLayoutStyle::kGrid;
    Network_RefreshKbLayoutToggleUi();
    Network_RebuildCustomKeyboard();
}

void Network_FillKbLayout(KbKeyDesc out[kKbKeysCap]) {
    for (int i = 0; i < kKbKeysCap; ++i) {
        out[i] = {"", nullptr, KbAction::kEmpty};
    }

    const int cols = Network_KbCols();
    const int cap = cols * Network_KbRows();

    auto put = [&](int idx, const char* label, const char* insert, KbAction act) {
        if (idx < 0 || idx >= cap || idx >= kKbKeysCap) {
            return;
        }
        out[idx] = {label, insert, act};
    };

    if (Network_State().kb_layout == KbLayoutStyle::kGrid) {
        // 原宫格：顺序 1–0 + a–z / 符号，底行功能
        constexpr int kCharSlots = 6 * kKbGridCols;
        if (Network_State().kb_mode == KbMode::kEn) {
            static char bufs[kCharSlots][2];
            int n = 0;
            for (char c = '1'; c <= '9' && n < kCharSlots; ++c, ++n) {
                bufs[n][0] = c;
                bufs[n][1] = '\0';
                put(n, bufs[n], bufs[n], KbAction::kChar);
            }
            if (n < kCharSlots) {
                bufs[n][0] = '0';
                bufs[n][1] = '\0';
                put(n, bufs[n], bufs[n], KbAction::kChar);
                ++n;
            }
            for (char c = 'a'; c <= 'z' && n < kCharSlots; ++c, ++n) {
                bufs[n][0] = Network_State().kb_upper ? static_cast<char>(c - 'a' + 'A') : c;
                bufs[n][1] = '\0';
                put(n, bufs[n], bufs[n], KbAction::kChar);
            }
        } else {
            static const char* const sym[] = {
                "!", "@", "#", "$", "%", "^",
                "&", "*", "(", ")", "-", "_",
                "=", "+", "[", "]", "{", "}",
                "\\", "|", ";", ":", "'", "\"",
                ",", ".", "<", ">", "/", "?",
                "`", "~", "，", "。", "？", "！",
                "、", "；", "：", "…", "“", "”",
                "‘", "’", "（", "）", "【", "】",
                "《", "》", "—", "·", "￥", "～",
            };
            const int nsym = static_cast<int>(sizeof(sym) / sizeof(sym[0]));
            for (int i = 0; i < nsym && i < kCharSlots; ++i) {
                put(i, sym[i], sym[i], KbAction::kChar);
            }
        }
        const int base = 6 * cols;
        put(base + 0, "英", nullptr, KbAction::kModeEn);
        put(base + 1, "符", nullptr, KbAction::kModeSym);
        put(base + 2, Network_State().kb_upper ? "大" : "小", nullptr, KbAction::kShift);
        put(base + 3, "␣", " ", KbAction::kSpace);
        put(base + 4, "␣", " ", KbAction::kSpace);
        put(base + 5, "删", nullptr, KbAction::kBackspace);
        return;
    }

    // 标准 QWERTY
    auto put_row_chars = [&](int row, const char* const* cells, int n) {
        for (int c = 0; c < n && c < cols; ++c) {
            if (cells[c] == nullptr || cells[c][0] == '\0') {
                continue;
            }
            put(row * cols + c, cells[c], cells[c], KbAction::kChar);
        }
    };

    if (Network_State().kb_mode == KbMode::kEn) {
        static char letter_bufs[26][2];
        auto letter = [&](char lower) -> const char* {
            const int i = lower - 'a';
            letter_bufs[i][0] =
                Network_State().kb_upper ? static_cast<char>(lower - 'a' + 'A') : lower;
            letter_bufs[i][1] = '\0';
            return letter_bufs[i];
        };

        static const char* const row0[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
        put_row_chars(0, row0, 10);

        const char* row1[] = {letter('q'), letter('w'), letter('e'), letter('r'), letter('t'),
                              letter('y'), letter('u'), letter('i'), letter('o'), letter('p')};
        put_row_chars(1, row1, 10);

        const char* row2[] = {letter('a'), letter('s'), letter('d'), letter('f'), letter('g'),
                              letter('h'), letter('j'), letter('k'), letter('l'), ";"};
        put_row_chars(2, row2, 10);

        put(3 * cols + 0, Network_State().kb_upper ? "大" : "小", nullptr, KbAction::kShift);
        const char* row3_letters[] = {letter('z'), letter('x'), letter('c'), letter('v'),
                                      letter('b'), letter('n'), letter('m')};
        for (int i = 0; i < 7; ++i) {
            put(3 * cols + 1 + i, row3_letters[i], row3_letters[i], KbAction::kChar);
        }
        put(3 * cols + 8, ",", ",", KbAction::kChar);
        put(3 * cols + 9, ".", ".", KbAction::kChar);
    } else {
        static const char* const row0[] = {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")"};
        static const char* const row1[] = {"-", "_", "=", "+", "[", "]", "{", "}", "\\", "|"};
        static const char* const row2[] = {";", ":", "'", "\"", "<", ">", "/", "?", "`", "~"};
        static const char* const row3[] = {"，", "。", "？", "！", "、", "；", "：", "…", "“", "”"};
        put_row_chars(0, row0, 10);
        put_row_chars(1, row1, 10);
        put_row_chars(2, row2, 10);
        put_row_chars(3, row3, 10);
    }

    const int base = 4 * cols;
    put(base + 0, "英", nullptr, KbAction::kModeEn);
    put(base + 1, "符", nullptr, KbAction::kModeSym);
    put(base + 2, "空格", " ", KbAction::kSpace);
    put(base + 9, "删", nullptr, KbAction::kBackspace);
}

// 字符/空格/退格：按下即写入（跟盖板键「按下即 Click」、拨号盘跟手一致）。
void Network_OnCustomKeyPressed(lv_event_t* e) {
    const auto action = static_cast<KbAction>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_obj_t* lbl = (btn != nullptr) ? lv_obj_get_child(btn, 0) : nullptr;
    const char* label = (lbl != nullptr) ? lv_label_get_text(lbl) : nullptr;

    switch (action) {
        case KbAction::kBackspace:
            Network_BackspacePassword();
            break;
        case KbAction::kSpace:
            Network_AppendToPassword(" ");
            break;
        case KbAction::kChar:
            if (label != nullptr) {
                Network_AppendToPassword(label);
            }
            break;
        case KbAction::kShift:
        case KbAction::kModeEn:
        case KbAction::kModeSym:
        case KbAction::kEmpty:
        default:
            break;
    }
}

// 切布局：松手再建盘，避免 PRESSED 里 lv_obj_clean 删掉当前按键。
void Network_OnCustomKeyClicked(lv_event_t* e) {
    const auto action = static_cast<KbAction>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));

    switch (action) {
        case KbAction::kShift:
            Network_State().kb_upper = !Network_State().kb_upper;
            Network_RebuildCustomKeyboard();
            break;
        case KbAction::kModeEn:
            Network_State().kb_mode = KbMode::kEn;
            Network_RebuildCustomKeyboard();
            break;
        case KbAction::kModeSym:
            Network_State().kb_mode = KbMode::kSym;
            Network_RebuildCustomKeyboard();
            break;
        default:
            break;
    }
}

void Network_StyleKbKey(lv_obj_t* btn, lv_obj_t* lbl, KbAction action, bool active_mode) {
    const bool highlight = active_mode || action == KbAction::kShift;
    if (highlight && (action == KbAction::kModeEn || action == KbAction::kModeSym ||
                      (action == KbAction::kShift && Network_State().kb_upper))) {
        lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    }
}

void Network_LayoutCustomKeyboardSquares() {
    if (Network_State().ui.pwd_keyboard == nullptr) {
        return;
    }
    const lv_coord_t w = lv_obj_get_content_width(Network_State().ui.pwd_keyboard);
    const lv_coord_t h = lv_obj_get_content_height(Network_State().ui.pwd_keyboard);
    if (w <= 0 || h <= 0) {
        return;
    }
    const int cols = Network_KbCols();
    const int rows = Network_KbRows();
    const lv_coord_t cell_w = (w - kKbGap * (cols - 1)) / cols;
    const lv_coord_t full_cell_h = (h - kKbGap * (rows - 1)) / rows;
    if (cell_w <= 0 || full_cell_h <= 0) {
        return;
    }

    lv_coord_t key_w = cell_w;
    lv_coord_t key_h = full_cell_h;
    if (Network_State().kb_layout == KbLayoutStyle::kGrid) {
        // 宫格：正方形键，铺满可用区
        const lv_coord_t side = (cell_w < full_cell_h) ? cell_w : full_cell_h;
        key_w = side;
        key_h = side;
    } else {
        // 标准：矩形键，高度取满铺一半
        key_h = full_cell_h / 2;
        if (key_h <= 0) {
            return;
        }
    }

    const lv_coord_t total_w = key_w * cols + kKbGap * (cols - 1);
    const lv_coord_t total_h = key_h * rows + kKbGap * (rows - 1);
    const lv_coord_t ox = (w - total_w) / 2;
    const lv_coord_t oy = (h - total_h) / 2;
    constexpr int kSpaceCol = 2;
    constexpr int kSpaceSpan = 7;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            lv_obj_t* key = Network_State().kb_keys[r * cols + c];
            if (key == nullptr || lv_obj_has_flag(key, LV_OBJ_FLAG_HIDDEN)) {
                continue;
            }
            lv_coord_t kw = key_w;
            if (Network_State().kb_layout == KbLayoutStyle::kQwerty && r == rows - 1 &&
                c == kSpaceCol) {
                kw = key_w * kSpaceSpan + kKbGap * (kSpaceSpan - 1);
            }
            lv_obj_set_size(key, kw, key_h);
            lv_obj_set_pos(key, ox + c * (key_w + kKbGap), oy + r * (key_h + kKbGap));
        }
    }
}

void Network_OnKbAreaSizeChanged(lv_event_t* /*e*/) {
    Network_LayoutCustomKeyboardSquares();
}

void Network_RebuildCustomKeyboard() {
    if (Network_State().ui.pwd_keyboard == nullptr) {
        return;
    }
    lv_obj_clean(Network_State().ui.pwd_keyboard);
    for (int i = 0; i < kKbKeysCap; ++i) {
        Network_State().kb_keys[i] = nullptr;
    }

    const int cols = Network_KbCols();
    const int rows = Network_KbRows();
    const int nkeys = cols * rows;

    KbKeyDesc layout[kKbKeysCap];
    Network_FillKbLayout(layout);

    const lv_font_t* font = Network_KbFont();
    for (int i = 0; i < nkeys; ++i) {
        const KbKeyDesc& d = layout[i];
        lv_obj_t* btn = lv_obj_create(Network_State().ui.pwd_keyboard);
        Network_Strip(btn);
        Network_State().kb_keys[i] = btn;
        if (d.action == KbAction::kEmpty) {
            lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, lv_color_black(), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        void* ud = reinterpret_cast<void*>(static_cast<uintptr_t>(d.action));
        // 输入键按下即上屏；切 英/符/大小写 仍 CLICKED，防止重建时销毁按中的键
        if (d.action == KbAction::kChar || d.action == KbAction::kSpace ||
            d.action == KbAction::kBackspace) {
            lv_obj_add_event_cb(btn, Network_OnCustomKeyPressed, LV_EVENT_PRESSED, ud);
        } else if (d.action == KbAction::kShift || d.action == KbAction::kModeEn ||
                   d.action == KbAction::kModeSym) {
            lv_obj_add_event_cb(btn, Network_OnCustomKeyClicked, LV_EVENT_CLICKED, ud);
        }
        if (d.action == KbAction::kBackspace) {
            // 与电话拨号页一致：短按删一字，长按清空
            lv_obj_add_event_cb(btn, Network_OnBackspaceLongPressed, LV_EVENT_LONG_PRESSED, nullptr);
        }

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, d.label != nullptr ? d.label : "");
        if (font != nullptr) {
            lv_obj_set_style_text_font(lbl, font, 0);
        }
        lv_obj_center(lbl);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

        const bool mode_on =
            (d.action == KbAction::kModeEn && Network_State().kb_mode == KbMode::kEn) ||
            (d.action == KbAction::kModeSym && Network_State().kb_mode == KbMode::kSym);
        Network_StyleKbKey(btn, lbl, d.action, mode_on);
    }
    Network_LayoutCustomKeyboardSquares();
}

void Network_OnPwdConnect(lv_event_t* /*e*/) {
    Network_ScheduleConnect(Network_State().pending_ssid, Network_State().password);
}

void Network_OnPwdCancel(lv_event_t* /*e*/) { Network_ClosePasswordPage(); }

void Network_ClosePasswordPage() {
    if (Network_State().ui.pwd_page != nullptr) {
        lv_obj_add_flag(Network_State().ui.pwd_page, LV_OBJ_FLAG_HIDDEN);
    }
    if (Network_State().ui.list_panel != nullptr) {
        lv_obj_clear_flag(Network_State().ui.list_panel, LV_OBJ_FLAG_HIDDEN);
    }
    Network_ClearPassword();
    Network_State().kb_mode = KbMode::kEn;
    Network_State().kb_upper = false;
}

void Network_BuildPasswordPage(lv_obj_t* parent, lv_coord_t top_y) {
    lv_obj_t* page = lv_obj_create(parent);
    Network_Strip(page);
    Network_State().ui.pwd_page = page;
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES - top_y);
    lv_obj_align(page, LV_ALIGN_TOP_MID, 0, top_y);
    lv_obj_set_style_bg_color(page, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(page, 12, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(page, 10, 0);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);

    const lv_font_t* ui = Network_UiFont();

    lv_obj_t* top = lv_obj_create(page);
    Network_Strip(top);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_height(top, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(top, 10, 0);

    // 标题行：左「连接到：」+ SSID（单行省略），右「标准|宫格」
    lv_obj_t* title_row = lv_obj_create(top);
    Network_Strip(title_row);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, kKbViewToggleH);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(title_row, 4, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    // 前缀固定宽度，长 SSID 时仍能看见「连接到：」
    lv_obj_t* prefix_lbl = lv_label_create(title_row);
    {
        char prefix[40];
        snprintf(prefix, sizeof(prefix), Lang::Strings::NETWORK_CONNECT_TO_FMT, "");
        lv_label_set_text(prefix_lbl, prefix);
    }
    if (ui != nullptr) {
        lv_obj_set_style_text_font(prefix_lbl, ui, 0);
    }
    lv_obj_set_style_text_color(prefix_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(prefix_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_flex_grow(prefix_lbl, 0, 0);

    Network_State().ui.pwd_ssid_lbl = lv_label_create(title_row);
    lv_label_set_text(Network_State().ui.pwd_ssid_lbl, "");
    // flex 吃掉前缀与切换之间的剩余宽度；宽=1 才能让 LONG_DOT 按分配宽度省略
    lv_obj_set_flex_grow(Network_State().ui.pwd_ssid_lbl, 1);
    lv_obj_set_width(Network_State().ui.pwd_ssid_lbl, 1);
    lv_obj_set_style_min_width(Network_State().ui.pwd_ssid_lbl, 24, 0);
    lv_obj_set_height(Network_State().ui.pwd_ssid_lbl, kKbViewToggleH);
    lv_label_set_long_mode(Network_State().ui.pwd_ssid_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(Network_State().ui.pwd_ssid_lbl, LV_TEXT_ALIGN_LEFT, 0);
    if (ui != nullptr) {
        lv_obj_set_style_text_font(Network_State().ui.pwd_ssid_lbl, ui, 0);
    }
    lv_obj_set_style_text_color(Network_State().ui.pwd_ssid_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(Network_State().ui.pwd_ssid_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(Network_State().ui.pwd_ssid_lbl, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* toggle = lv_obj_create(title_row);
    Network_Strip(toggle);
    lv_obj_set_height(toggle, kKbViewToggleH);
    lv_obj_set_width(toggle, LV_SIZE_CONTENT);
    lv_obj_set_style_flex_grow(toggle, 0, 0);
    lv_obj_set_style_min_width(toggle, kKbViewBtnW * 2 + 1, 0);
    lv_obj_set_style_border_width(toggle, 1, 0);
    lv_obj_set_style_border_color(toggle, lv_color_black(), 0);
    lv_obj_set_style_radius(toggle, 4, 0);
    lv_obj_set_flex_flow(toggle, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toggle, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(toggle, LV_OBJ_FLAG_CLICKABLE);

    auto make_layout_btn = [&](const char* text, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_obj_create(toggle);
        Network_Strip(btn);
        lv_obj_set_size(btn, kKbViewBtnW, kKbViewBtnH);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        if (ui != nullptr) {
            lv_obj_set_style_text_font(lbl, ui, 0);
        }
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_center(lbl);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        return btn;
    };
    Network_State().ui.pwd_kb_qwerty_btn = make_layout_btn("标准", Network_OnKbLayoutQwertyClicked);
    Network_State().ui.pwd_kb_grid_btn = make_layout_btn("宫格", Network_OnKbLayoutGridClicked);
    lv_obj_t* sep = lv_obj_create(toggle);
    Network_Strip(sep);
    lv_obj_set_size(sep, 1, 20);
    lv_obj_set_style_bg_color(sep, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_to_index(sep, 1);
    Network_RefreshKbLayoutToggleUi();

    lv_obj_t* hint = lv_label_create(top);
    lv_label_set_text(hint, Lang::Strings::NETWORK_PWD_HINT);
    if (ui != nullptr) {
        lv_obj_set_style_text_font(hint, ui, 0);
    }

    // 与拨号号码行一致：label + 缓冲，按下即可刷字，无 textarea 光标/布局开销
    lv_obj_t* box = lv_obj_create(top);
    Network_Strip(box);
    Network_State().ui.pwd_box = box;
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_height(box, 56);
    lv_obj_set_style_bg_color(box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_black(), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_pad_hor(box, 10, 0);
    lv_obj_set_style_pad_ver(box, 10, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    Network_State().ui.pwd_lbl = lv_label_create(box);
    lv_obj_set_width(Network_State().ui.pwd_lbl, LV_PCT(100));
    // 墨水屏勿用滚动动画（会连刷）；超长直接裁切
    lv_label_set_long_mode(Network_State().ui.pwd_lbl, LV_LABEL_LONG_CLIP);
    if (ui != nullptr) {
        lv_obj_set_style_text_font(Network_State().ui.pwd_lbl, ui, 0);
    }
    lv_obj_set_style_text_color(Network_State().ui.pwd_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(Network_State().ui.pwd_lbl, LV_OBJ_FLAG_CLICKABLE);
    Network_State().password[0] = '\0';
    Network_RefreshPasswordDisplay();

    lv_obj_t* row = lv_obj_create(top);
    Network_Strip(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 48);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    Network_MakeBtn(row, Lang::Strings::NETWORK_CANCEL, 140, 44, Network_OnPwdCancel);
    Network_MakeBtn(row, Lang::Strings::NETWORK_CONNECT, 140, 44, Network_OnPwdConnect);

    // 键盘区：标准 QWERTY 或原宫格，由顶栏切换
    lv_obj_t* kb = lv_obj_create(page);
    Network_Strip(kb);
    Network_State().ui.pwd_keyboard = kb;
    lv_obj_set_width(kb, LV_PCT(100));
    lv_obj_set_flex_grow(kb, 1);
    lv_obj_set_style_bg_color(kb, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(kb, Network_OnKbAreaSizeChanged, LV_EVENT_SIZE_CHANGED, nullptr);

    Network_State().kb_mode = KbMode::kEn;
    Network_State().kb_layout = KbLayoutStyle::kQwerty;
    Network_State().kb_upper = false;
    Network_RefreshKbLayoutToggleUi();
    Network_RebuildCustomKeyboard();
}

void Network_OpenPasswordPage(const std::string& ssid, wifi_auth_mode_t authmode) {
    if (Network_State().ui.pwd_page == nullptr || Network_State().ui.pwd_ssid_lbl == nullptr) {
        return;
    }
    Network_State().pending_ssid = ssid;
    Network_State().pending_authmode = authmode;
    Network_State().kb_mode = KbMode::kEn;
    Network_State().kb_upper = false;

    // 仅填 SSID；前缀固定在左侧，「连接到：」不会被长名称挤掉
    lv_label_set_text(Network_State().ui.pwd_ssid_lbl, ssid.c_str());
    lv_label_set_long_mode(Network_State().ui.pwd_ssid_lbl, LV_LABEL_LONG_DOT);

    Network_ClearPassword();

    Network_RefreshKbLayoutToggleUi();
    Network_RebuildCustomKeyboard();

    if (Network_State().ui.list_panel != nullptr) {
        lv_obj_add_flag(Network_State().ui.list_panel, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(Network_State().ui.pwd_page, LV_OBJ_FLAG_HIDDEN);
    // 先出布局再刷省略，确保按实际分配宽度裁切
    lv_obj_update_layout(Network_State().ui.pwd_page);
    lv_label_set_long_mode(Network_State().ui.pwd_ssid_lbl, LV_LABEL_LONG_DOT);
    Network_LayoutCustomKeyboardSquares();
}

