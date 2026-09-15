#include "home_screen/home_hero.h"

#include "standby_screen/standby_classic.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include <esp_log.h>
#include <lvgl.h>

#include "fontpack_lvgl.h"
#include "assets/lang_config.h"

LV_FONT_DECLARE(font_misans_regular_160_2);

namespace home_hero {
namespace {

constexpr const char* TAG = "HomeHero";
constexpr lv_coord_t kHeroPadTop = 20;
constexpr lv_coord_t kTimeDateGap = 24;
constexpr lv_coord_t kSidePad = 28;
constexpr lv_coord_t kMetaGap = 16;  // 日期/天气与中间竖线间距
constexpr lv_coord_t kMetaSplitW = 2;
constexpr lv_coord_t kWeatherIcon = 70;
constexpr lv_coord_t kWeatherGap = 12;
constexpr lv_coord_t kCalendarIconW = 62;
constexpr lv_coord_t kCalendarIconH = 59;
constexpr lv_coord_t kCalendarGap = 12;
constexpr uint32_t kClockTickMs = 1000;

// 斜切主题 Hero
constexpr lv_coord_t kSlashSidePad = 28;
constexpr lv_coord_t kSlashTimeDateGap = 20;
constexpr lv_coord_t kSlashDateRuleW = 64;
constexpr lv_coord_t kSlashDateRuleH = 3;
constexpr lv_coord_t kSlashDateGap = 10;
constexpr lv_coord_t kSlashTimeW = 212;    // 「00」adv 约 203px；200 会挤换行
constexpr lv_coord_t kSlashSlashGapH = 44; // 时/分空隙高
constexpr lv_coord_t kSlashLineHalfW = 72; // 斜线半宽（总长约 144，缓于竖切）
constexpr lv_coord_t kSlashLinePadY = 6;
constexpr lv_coord_t kSlashWeatherIcon = 48; // 斜切右侧天气图标（小于经典 70，避免挤宽度）
constexpr lv_coord_t kSlashWeatherGap = 8;
constexpr lv_coord_t kSlashMinuteNudgeX = 20; // 分相对时略右移

const lv_font_t* SlashTimeFont() {
    // 160 字 bitmap 高约 124，原行高 214；收紧后完整显示且不再叠出空白横线
    static lv_font_t font;
    static bool inited = false;
    if (!inited) {
        font = font_misans_regular_160_2;
        font.line_height = 134;
        font.base_line = 10;
        inited = true;
    }
    return &font;
}

enum class HeroKind : uint8_t { Classic = 0, Slash = 1 };

const lv_font_t* WeatherFont() {
    const lv_font_t* f = fontpack_lv_font_get(30, 2);
    return f != nullptr ? f : fontpack_lv_font_ui();
}

const char* WeekdayName(int wday) {
    switch (wday) {
        case 0:
            return Lang::Strings::HOME_WDAY_SUN;
        case 1:
            return Lang::Strings::HOME_WDAY_MON;
        case 2:
            return Lang::Strings::HOME_WDAY_TUE;
        case 3:
            return Lang::Strings::HOME_WDAY_WED;
        case 4:
            return Lang::Strings::HOME_WDAY_THU;
        case 5:
            return Lang::Strings::HOME_WDAY_FRI;
        case 6:
            return Lang::Strings::HOME_WDAY_SAT;
        default:
            return "";
    }
}

struct UiState {
    HeroKind kind = HeroKind::Classic;
    lv_obj_t* root = nullptr;
    lv_obj_t* time_lbl = nullptr;      // classic：HH:MM；slash 不用
    lv_obj_t* hour_lbl = nullptr;      // slash：HH
    lv_obj_t* minute_lbl = nullptr;    // slash：MM
    lv_obj_t* date_lbl = nullptr;
    lv_obj_t* lunar_lbl = nullptr;
    lv_obj_t* weekday_lbl = nullptr;   // slash
    lv_obj_t* meta_split = nullptr;
    lv_obj_t* weather_row = nullptr;
    lv_obj_t* weather_icon = nullptr;
    lv_obj_t* weather_text = nullptr;
    lv_obj_t* weather_temp = nullptr;
    lv_timer_t* timer = nullptr;
    char last_time[8] = {};
    char last_date[48] = {};
    char last_lunar[24] = {};
    char last_weekday[24] = {};
};

UiState s_ui;
bool s_alive = false;

void FormatClassicClock(char* time_buf, size_t time_len, char* date_buf, size_t date_len) {
    time_t now = time(nullptr);
    struct tm tm_info = {};
    // 160 号字体仅含 0-9 与 ':'，无效时间用 00:00 占位，勿写 '-'.
    if (localtime_r(&now, &tm_info) == nullptr || tm_info.tm_year < (2020 - 1900)) {
        std::snprintf(time_buf, time_len, "00:00");
        std::snprintf(date_buf, date_len, Lang::Strings::HOME_DATE_PLACEHOLDER);
        return;
    }
    std::snprintf(time_buf, time_len, "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
    std::snprintf(date_buf, date_len, Lang::Strings::HOME_DATE_FMT, tm_info.tm_mon + 1, tm_info.tm_mday);
}

void FormatSlashClock(char* hour_buf, size_t hour_len, char* min_buf, size_t min_len,
                      char* date_buf, size_t date_len, char* wday_buf, size_t wday_len) {
    time_t now = time(nullptr);
    struct tm tm_info = {};
    if (localtime_r(&now, &tm_info) == nullptr || tm_info.tm_year < (2020 - 1900)) {
        std::snprintf(hour_buf, hour_len, "00");
        std::snprintf(min_buf, min_len, "00");
        std::snprintf(date_buf, date_len, Lang::Strings::HOME_DATE_SLASH_PLACEHOLDER);
        std::snprintf(wday_buf, wday_len, "%s", "");
        return;
    }
    std::snprintf(hour_buf, hour_len, "%02d", tm_info.tm_hour);
    std::snprintf(min_buf, min_len, "%02d", tm_info.tm_min);
    std::snprintf(date_buf, date_len, Lang::Strings::HOME_DATE_SLASH_FMT, tm_info.tm_year + 1900,
                  tm_info.tm_mon + 1, tm_info.tm_mday);
    std::snprintf(wday_buf, wday_len, "%s", WeekdayName(tm_info.tm_wday));
}

void RefreshClock(bool force) {
    if (!s_alive) {
        return;
    }

    if (s_ui.kind == HeroKind::Slash) {
        if (s_ui.hour_lbl == nullptr || s_ui.minute_lbl == nullptr || s_ui.date_lbl == nullptr) {
            return;
        }
        char hour[4];
        char minute[4];
        char date_str[48];
        char wday[24];
        FormatSlashClock(hour, sizeof(hour), minute, sizeof(minute), date_str, sizeof(date_str), wday,
                         sizeof(wday));
        char time_key[8];
        std::snprintf(time_key, sizeof(time_key), "%s%s", hour, minute);
        if (force || std::strcmp(time_key, s_ui.last_time) != 0) {
            lv_label_set_text(s_ui.hour_lbl, hour);
            lv_label_set_text(s_ui.minute_lbl, minute);
            std::snprintf(s_ui.last_time, sizeof(s_ui.last_time), "%s", time_key);
        }
        if (force || std::strcmp(date_str, s_ui.last_date) != 0) {
            lv_label_set_text(s_ui.date_lbl, date_str);
            std::snprintf(s_ui.last_date, sizeof(s_ui.last_date), "%s", date_str);
        }
        if (s_ui.weekday_lbl != nullptr &&
            (force || std::strcmp(wday, s_ui.last_weekday) != 0)) {
            lv_label_set_text(s_ui.weekday_lbl, wday);
            std::snprintf(s_ui.last_weekday, sizeof(s_ui.last_weekday), "%s", wday);
        }
        return;
    }

    if (s_ui.time_lbl == nullptr || s_ui.date_lbl == nullptr) {
        return;
    }

    char time_str[8];
    char date_str[48];
    FormatClassicClock(time_str, sizeof(time_str), date_str, sizeof(date_str));

    if (force || std::strcmp(time_str, s_ui.last_time) != 0) {
        lv_label_set_text(s_ui.time_lbl, time_str);
        std::snprintf(s_ui.last_time, sizeof(s_ui.last_time), "%s", time_str);
    }
    if (force || std::strcmp(date_str, s_ui.last_date) != 0) {
        lv_label_set_text(s_ui.date_lbl, date_str);
        std::snprintf(s_ui.last_date, sizeof(s_ui.last_date), "%s", date_str);
    }
}

void FillWeatherWidgets(const standby_classic::WeatherView& snap) {
    if (s_ui.weather_text != nullptr) {
        lv_label_set_text(s_ui.weather_text, snap.text);
    }
    if (s_ui.weather_temp != nullptr) {
        if (snap.has_temp) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d℃", snap.temp);
            lv_label_set_text(s_ui.weather_temp, buf);
        } else {
            lv_label_set_text(s_ui.weather_temp, "");
        }
    }
    if (s_ui.weather_icon != nullptr) {
        if (snap.icon_code[0] != '\0') {
            char path[48];
            std::snprintf(path, sizeof(path), "A:ic_s_weather_%s.spng", snap.icon_code);
            lv_image_set_src(s_ui.weather_icon, path);
            lv_obj_remove_flag(s_ui.weather_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ui.weather_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ApplyWeatherUi() {
    if (!s_alive) {
        return;
    }

    standby_classic::WeatherView snap;
    const bool weather_ok = standby_classic::CopyWeatherView(&snap) && snap.valid;

    if (s_ui.kind == HeroKind::Slash) {
        if (s_ui.lunar_lbl != nullptr) {
            char lunar[24] = {};
            // 英文等语言农历位会填成星期，与 weekday_lbl 重复；仅中文显示农历
            if (weather_ok && std::strcmp(Lang::CODE, "zh-CN") == 0) {
                standby_classic::FormatLunarOrWeekday(lunar, sizeof(lunar), snap.lunar);
            }
            if (std::strcmp(lunar, s_ui.last_lunar) != 0) {
                lv_label_set_text(s_ui.lunar_lbl, lunar);
                std::snprintf(s_ui.last_lunar, sizeof(s_ui.last_lunar), "%s", lunar);
            }
            if (lunar[0] == '\0') {
                lv_obj_add_flag(s_ui.lunar_lbl, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(s_ui.lunar_lbl, LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (s_ui.weather_row == nullptr) {
            return;
        }
        if (!weather_ok) {
            lv_obj_add_flag(s_ui.weather_row, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        FillWeatherWidgets(snap);
        lv_obj_remove_flag(s_ui.weather_row, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (s_ui.weather_row == nullptr) {
        return;
    }
    if (!weather_ok) {
        lv_obj_add_flag(s_ui.weather_row, LV_OBJ_FLAG_HIDDEN);
        if (s_ui.meta_split != nullptr) {
            lv_obj_add_flag(s_ui.meta_split, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_ui.lunar_lbl != nullptr) {
            lv_label_set_text(s_ui.lunar_lbl, "");
        }
        return;
    }

    if (s_ui.lunar_lbl != nullptr) {
        char sub[24];
        standby_classic::FormatLunarOrWeekday(sub, sizeof(sub), snap.lunar);
        lv_label_set_text(s_ui.lunar_lbl, sub);
    }
    FillWeatherWidgets(snap);
    lv_obj_remove_flag(s_ui.weather_row, LV_OBJ_FLAG_HIDDEN);
    if (s_ui.meta_split != nullptr) {
        lv_obj_remove_flag(s_ui.meta_split, LV_OBJ_FLAG_HIDDEN);
    }
}

void OnWeatherListener() {
    ApplyWeatherUi();
}

void OnClockTimer(lv_timer_t* /*timer*/) {
    RefreshClock(false);
}

void OnSlashGapDraw(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN_END) {
        return;
    }
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    if (obj == nullptr || layer == nullptr) {
        return;
    }
    lv_area_t a;
    lv_obj_get_content_coords(obj, &a);
    // 斜线居中于时宽，长度适中、斜率缓于竖线
    const lv_coord_t mid_x = (a.x1 + a.x2) / 2;
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_black();
    dsc.opa = LV_OPA_COVER;
    dsc.width = 2;
    dsc.round_start = 0;
    dsc.round_end = 0;
    dsc.p1.x = mid_x - kSlashLineHalfW;
    dsc.p1.y = a.y2 - kSlashLinePadY;
    dsc.p2.x = mid_x + kSlashLineHalfW;
    dsc.p2.y = a.y1 + kSlashLinePadY;
    lv_draw_line(layer, &dsc);
}

void StyleSlashTimeLabel(lv_obj_t* lbl) {
    lv_obj_set_style_text_font(lbl, SlashTimeFont(), 0);
    lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(lbl, kSlashTimeW);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP); // 防宽度不足时第二位换行
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
}

void BuildClassicUi(lv_obj_t* parent, lv_coord_t y_offset) {
    lv_obj_t* box = lv_obj_create(parent);
    s_ui.root = box;
    lv_obj_remove_style_all(box);
    lv_obj_set_width(box, LV_HOR_RES - 2 * kSidePad);
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, y_offset + kHeroPadTop);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_row(box, kTimeDateGap, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);

    s_ui.time_lbl = lv_label_create(box);
    lv_label_set_text(s_ui.time_lbl, "00:00");
    lv_obj_set_style_text_font(s_ui.time_lbl, &font_misans_regular_160_2, 0);
    lv_obj_set_style_text_color(s_ui.time_lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(s_ui.time_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(s_ui.time_lbl, LV_OBJ_FLAG_CLICKABLE);

    // 居中成组：日期 | 竖线 | 天气，用 pad_column 控与竖线边距（勿 SPACE_BETWEEN 拉满宽）
    lv_obj_t* meta_row = lv_obj_create(box);
    lv_obj_remove_style_all(meta_row);
    lv_obj_set_width(meta_row, LV_SIZE_CONTENT);
    lv_obj_set_height(meta_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(meta_row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(meta_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(meta_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(meta_row, kMetaGap, 0);
    lv_obj_clear_flag(meta_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(meta_row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* date_row = lv_obj_create(meta_row);
    lv_obj_remove_style_all(date_row);
    lv_obj_set_size(date_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(date_row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(date_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(date_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(date_row, kCalendarGap, 0);
    lv_obj_clear_flag(date_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(date_row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* cal_icon = lv_image_create(date_row);
    lv_image_set_src(cal_icon, "A:ic_s_standby_calendar.spng");
    lv_obj_set_size(cal_icon, kCalendarIconW, kCalendarIconH);
    lv_obj_clear_flag(cal_icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* date_col = lv_obj_create(date_row);
    lv_obj_remove_style_all(date_col);
    lv_obj_set_size(date_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(date_col, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(date_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(date_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(date_col, 4, 0);
    lv_obj_clear_flag(date_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(date_col, LV_OBJ_FLAG_CLICKABLE);

    const lv_font_t* wfont = WeatherFont();
    s_ui.date_lbl = lv_label_create(date_col);
    lv_label_set_text(s_ui.date_lbl, Lang::Strings::HOME_DATE_PLACEHOLDER);
    lv_obj_set_style_text_font(s_ui.date_lbl, wfont, 0);
    lv_obj_set_style_text_color(s_ui.date_lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(s_ui.date_lbl, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_clear_flag(s_ui.date_lbl, LV_OBJ_FLAG_CLICKABLE);

    s_ui.lunar_lbl = lv_label_create(date_col);
    lv_label_set_text(s_ui.lunar_lbl, "");
    lv_obj_set_style_text_font(s_ui.lunar_lbl, wfont, 0);
    lv_obj_set_style_text_color(s_ui.lunar_lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(s_ui.lunar_lbl, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_clear_flag(s_ui.lunar_lbl, LV_OBJ_FLAG_CLICKABLE);

    s_ui.meta_split = lv_obj_create(meta_row);
    lv_obj_remove_style_all(s_ui.meta_split);
    lv_obj_set_size(s_ui.meta_split, kMetaSplitW, kWeatherIcon);
    lv_obj_set_style_bg_color(s_ui.meta_split, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.meta_split, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_ui.meta_split, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.meta_split, LV_OBJ_FLAG_HIDDEN);

    s_ui.weather_row = lv_obj_create(meta_row);
    lv_obj_remove_style_all(s_ui.weather_row);
    lv_obj_set_size(s_ui.weather_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_ui.weather_row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(s_ui.weather_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_ui.weather_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_ui.weather_row, kWeatherGap, 0);
    lv_obj_clear_flag(s_ui.weather_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_ui.weather_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.weather_row, LV_OBJ_FLAG_HIDDEN);

    s_ui.weather_icon = lv_image_create(s_ui.weather_row);
    lv_obj_set_size(s_ui.weather_icon, kWeatherIcon, kWeatherIcon);
    lv_obj_clear_flag(s_ui.weather_icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* meta = lv_obj_create(s_ui.weather_row);
    lv_obj_remove_style_all(meta);
    lv_obj_set_size(meta, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(meta, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(meta, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(meta, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(meta, 4, 0);
    lv_obj_clear_flag(meta, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(meta, LV_OBJ_FLAG_CLICKABLE);

    s_ui.weather_text = lv_label_create(meta);
    lv_label_set_text(s_ui.weather_text, "");
    lv_obj_set_style_text_font(s_ui.weather_text, wfont, 0);
    lv_obj_set_style_text_color(s_ui.weather_text, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.weather_text, LV_OBJ_FLAG_CLICKABLE);

    s_ui.weather_temp = lv_label_create(meta);
    lv_label_set_text(s_ui.weather_temp, "");
    lv_obj_set_style_text_font(s_ui.weather_temp, wfont, 0);
    lv_obj_set_style_text_color(s_ui.weather_temp, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.weather_temp, LV_OBJ_FLAG_CLICKABLE);
}

void BuildSlashUi(lv_obj_t* parent, lv_coord_t y_offset) {
    lv_obj_t* box = lv_obj_create(parent);
    s_ui.root = box;
    lv_obj_remove_style_all(box);
    lv_obj_set_width(box, LV_HOR_RES - 2 * kSlashSidePad);
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, y_offset + kHeroPadTop);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    // 底对齐：日期块落在分针一带，星期一行才看得见
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(box, kSlashTimeDateGap, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t* time_col = lv_obj_create(box);
    lv_obj_remove_style_all(time_col);
    lv_obj_set_size(time_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(time_col, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(time_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(time_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(time_col, 0, 0);
    lv_obj_clear_flag(time_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(time_col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(time_col, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    s_ui.hour_lbl = lv_label_create(time_col);
    lv_label_set_text(s_ui.hour_lbl, "00");
    StyleSlashTimeLabel(s_ui.hour_lbl);

    lv_obj_t* slash_gap = lv_obj_create(time_col);
    lv_obj_remove_style_all(slash_gap);
    lv_obj_set_size(slash_gap, kSlashTimeW, kSlashSlashGapH);
    lv_obj_set_style_bg_opa(slash_gap, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(slash_gap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(slash_gap, OnSlashGapDraw, LV_EVENT_DRAW_MAIN_END, nullptr);

    s_ui.minute_lbl = lv_label_create(time_col);
    lv_label_set_text(s_ui.minute_lbl, "00");
    StyleSlashTimeLabel(s_ui.minute_lbl);
    lv_obj_set_style_translate_x(s_ui.minute_lbl, kSlashMinuteNudgeX, 0);

    lv_obj_t* date_col = lv_obj_create(box);
    lv_obj_remove_style_all(date_col);
    lv_obj_set_size(date_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(date_col, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(date_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(date_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(date_col, kSlashDateGap, 0);
    lv_obj_clear_flag(date_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(date_col, LV_OBJ_FLAG_CLICKABLE);

    // 公历/星期常规；农历 30@4 半粗
    const lv_font_t* date_font = fontpack_lv_font_get(30, 2);
    if (date_font == nullptr) {
        date_font = fontpack_lv_font_ui();
    }
    const lv_font_t* lunar_font = fontpack_lv_font_get(30, 4);
    if (lunar_font == nullptr) {
        lunar_font = date_font;
    }

    // 天气在日期横线上方
    s_ui.weather_row = lv_obj_create(date_col);
    lv_obj_remove_style_all(s_ui.weather_row);
    lv_obj_set_size(s_ui.weather_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_ui.weather_row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(s_ui.weather_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_ui.weather_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_ui.weather_row, kSlashWeatherGap, 0);
    lv_obj_clear_flag(s_ui.weather_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_ui.weather_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.weather_row, LV_OBJ_FLAG_HIDDEN);

    s_ui.weather_icon = lv_image_create(s_ui.weather_row);
    lv_obj_set_size(s_ui.weather_icon, kSlashWeatherIcon, kSlashWeatherIcon);
    lv_obj_clear_flag(s_ui.weather_icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* weather_meta = lv_obj_create(s_ui.weather_row);
    lv_obj_remove_style_all(weather_meta);
    lv_obj_set_size(weather_meta, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(weather_meta, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(weather_meta, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(weather_meta, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(weather_meta, 2, 0);
    lv_obj_clear_flag(weather_meta, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(weather_meta, LV_OBJ_FLAG_CLICKABLE);

    s_ui.weather_text = lv_label_create(weather_meta);
    lv_label_set_text(s_ui.weather_text, "");
    // 天气描述半粗；温度保持常规
    const lv_font_t* weather_text_font = fontpack_lv_font_get(30, 4);
    if (weather_text_font == nullptr) {
        weather_text_font = date_font;
    }
    lv_obj_set_style_text_font(s_ui.weather_text, weather_text_font, 0);
    lv_obj_set_style_text_color(s_ui.weather_text, lv_color_black(), 0);
    lv_obj_set_style_text_align(s_ui.weather_text, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_clear_flag(s_ui.weather_text, LV_OBJ_FLAG_CLICKABLE);

    s_ui.weather_temp = lv_label_create(weather_meta);
    lv_label_set_text(s_ui.weather_temp, "");
    lv_obj_set_style_text_font(s_ui.weather_temp, date_font, 0);
    lv_obj_set_style_text_color(s_ui.weather_temp, lv_color_black(), 0);
    lv_obj_set_style_text_align(s_ui.weather_temp, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_clear_flag(s_ui.weather_temp, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* rule = lv_obj_create(date_col);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, kSlashDateRuleW, kSlashDateRuleH);
    lv_obj_set_style_bg_color(rule, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_clear_flag(rule, LV_OBJ_FLAG_CLICKABLE);

    s_ui.date_lbl = lv_label_create(date_col);
    lv_label_set_text(s_ui.date_lbl, Lang::Strings::HOME_DATE_SLASH_PLACEHOLDER);
    lv_obj_set_style_text_font(s_ui.date_lbl, date_font, 0);
    lv_obj_set_style_text_color(s_ui.date_lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(s_ui.date_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_clear_flag(s_ui.date_lbl, LV_OBJ_FLAG_CLICKABLE);

    s_ui.lunar_lbl = lv_label_create(date_col);
    lv_label_set_text(s_ui.lunar_lbl, "");
    lv_obj_add_flag(s_ui.lunar_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(s_ui.lunar_lbl, lunar_font, 0);
    lv_obj_set_style_text_color(s_ui.lunar_lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(s_ui.lunar_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_clear_flag(s_ui.lunar_lbl, LV_OBJ_FLAG_CLICKABLE);

    s_ui.weekday_lbl = lv_label_create(date_col);
    lv_label_set_text(s_ui.weekday_lbl, "");
    lv_obj_set_style_text_font(s_ui.weekday_lbl, date_font, 0);
    lv_obj_set_style_text_color(s_ui.weekday_lbl, lv_color_black(), 0);
    lv_obj_set_style_text_align(s_ui.weekday_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_clear_flag(s_ui.weekday_lbl, LV_OBJ_FLAG_CLICKABLE);
}

}  // namespace

lv_obj_t* Mount(lv_obj_t* parent, lv_coord_t y_offset) {
    if (parent == nullptr) {
        return nullptr;
    }
    Teardown();
    s_ui = UiState{};
    s_ui.kind = HeroKind::Classic;
    s_alive = true;
    BuildClassicUi(parent, y_offset);
    ESP_LOGI(TAG, "mounted classic y_offset=%d", static_cast<int>(y_offset));
    return s_ui.root;
}

lv_obj_t* MountSlash(lv_obj_t* parent, lv_coord_t y_offset) {
    if (parent == nullptr) {
        return nullptr;
    }
    Teardown();
    s_ui = UiState{};
    s_ui.kind = HeroKind::Slash;
    s_alive = true;
    BuildSlashUi(parent, y_offset);
    ESP_LOGI(TAG, "mounted slash y_offset=%d", static_cast<int>(y_offset));
    return s_ui.root;
}

void Start() {
    if (!s_alive) {
        return;
    }
    RefreshClock(true);
    ApplyWeatherUi();
    standby_classic::AddWeatherUiListener(OnWeatherListener);
    if (s_ui.timer != nullptr) {
        lv_timer_delete(s_ui.timer);
        s_ui.timer = nullptr;
    }
    s_ui.timer = lv_timer_create(OnClockTimer, kClockTickMs, nullptr);
    standby_classic::RequestWeatherEnsure();
}

void Stop() {
    standby_classic::RemoveWeatherUiListener(OnWeatherListener);
    if (s_ui.timer != nullptr) {
        lv_timer_delete(s_ui.timer);
        s_ui.timer = nullptr;
    }
}

void Teardown() {
    Stop();
    s_alive = false;
    // 控件随 parent/screen 删除；只清指针避免野引用
    s_ui = UiState{};
}

}  // namespace home_hero
