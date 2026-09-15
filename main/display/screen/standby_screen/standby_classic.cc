#include "standby_screen/standby_classic.h"

#include "standby_screen/standby_screen.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include <cJSON.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

#include "api_endpoints.h"
#include "api_http.h"
#include "board.h"
#include "checklist_cache.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "screen_common.h"
#include "settings.h"
#include "task_screen/task_screen.h"
#include "wifi_station.h"
#include "assets/lang_config.h"

namespace {

constexpr const char* TAG = "StandbyClassic";
constexpr lv_coord_t kSidePad = 28;
constexpr lv_coord_t kMetaGap = 16;
constexpr lv_coord_t kMetaSplitW = 2;
constexpr lv_coord_t kWeatherIcon = 70;
constexpr lv_coord_t kWeatherGap = 12;
constexpr lv_coord_t kCalendarIconW = 62;
constexpr lv_coord_t kCalendarIconH = 59;
constexpr lv_coord_t kCalendarGap = 12;
constexpr lv_coord_t kHomeBtnH = 60;
constexpr lv_coord_t kTodoRowPad = 10;   // 与 TaskScreen kRowPad 一致
constexpr lv_coord_t kTodoGap = 8;       // 与 TaskScreen kRowGap 一致
constexpr lv_coord_t kTodoLineGap = 6;   // 与 TaskScreen kRowLineGap 一致
constexpr lv_coord_t kSectionGap = 48;
constexpr lv_coord_t kHeroSectionGap = 20;  // 回首页与猫咪区间距
constexpr lv_coord_t kHeroInnerGap = 4;     // 气泡与猫咪间距
constexpr lv_coord_t kTopPad = 12;
// 猫咪贴底：无底边距
constexpr lv_coord_t kCatW = 424;
constexpr lv_coord_t kCatH = 202;
constexpr lv_coord_t kBubbleW = 316;  // 原 372 再缩 15%
constexpr lv_coord_t kBubbleH = 94;
constexpr lv_coord_t kMenuIcon = 30;
constexpr lv_coord_t kBorderW = 2;
constexpr int kMaxTodos = 3;  // 下半有猫咪，条数略减以免撑出屏
constexpr int kHttpTimeoutMs = 15000;
constexpr int kWorkerStack = 10 * 1024;
constexpr const char* kWeatherNs = "weather";

const lv_font_t* WeatherFont() {
    const lv_font_t* f = fontpack_lv_font_get(30, 2);
    return f != nullptr ? f : fontpack_lv_font_ui();
}

/** 清单行：与 TaskScreen TaskFont 一致（fontpack UI / 25@2） */
const lv_font_t* TodoFont() {
    return fontpack_lv_font_ui();
}

// 百度地图天气现象编码（对照表 0410）→ ic_s_weather_<code>.spng
struct WeatherPhenom {
    const char* code;
    const char* zh;
    const char* en_day;
    const char* en_night;
};

constexpr WeatherPhenom kPhenoms[] = {
    {"00", "晴", "Sunny", "Clear"},
    {"01", "多云", "Cloudy", "Cloudy"},
    {"02", "阴", "Overcast", "Overcast"},
    {"03", "阵雨", "Shower", "Shower"},
    {"04", "雷阵雨", "Thundershower", "Thundershower"},
    {"05", "雷阵雨伴有冰雹", "Thundershower with hail", "Thundershower with hail"},
    {"06", "雨夹雪", "Sleet", "Sleet"},
    {"07", "小雨", "Light rain", "Light rain"},
    {"08", "中雨", "Moderate rain", "Moderate rain"},
    {"09", "大雨", "Heavy rain", "Heavy rain"},
    {"10", "暴雨", "Storm", "Storm"},
    {"11", "大暴雨", "Heavy storm", "Heavy storm"},
    {"12", "特大暴雨", "Severe storm", "Severe storm"},
    {"13", "阵雪", "Snow flurry", "Snow flurry"},
    {"14", "小雪", "Light snow", "Light snow"},
    {"15", "中雪", "Moderate snow", "Moderate snow"},
    {"16", "大雪", "Heavy snow", "Heavy snow"},
    {"17", "暴雪", "Snowstorm", "Snowstorm"},
    {"18", "雾", "Fog", "Fog"},
    {"19", "冻雨", "Ice rain", "Ice rain"},
    {"20", "沙尘暴", "Duststorm", "Duststorm"},
    {"21", "小到中雨", "Light to moderate rain", "Light to moderate rain"},
    {"22", "中到大雨", "Moderate to heavy rain", "Moderate to heavy rain"},
    {"23", "大到暴雨", "Heavy rain to storm", "Heavy rain to storm"},
    {"24", "暴雨到大暴雨", "Storm to heavy storm", "Storm to heavy storm"},
    {"25", "大暴雨到特大暴雨", "Heavy to severe storm", "Heavy to severe storm"},
    {"26", "小到中雪", "Light to moderate snow", "Light to moderate snow"},
    {"27", "中到大雪", "Moderate to heavy snow", "Moderate to heavy snow"},
    {"28", "大到暴雪", "Heavy snow to snowstorm", "Heavy snow to snowstorm"},
    {"29", "浮尘", "Dust", "Dust"},
    {"30", "扬沙", "Sand", "Sand"},
    {"31", "强沙尘暴", "Sandstorm", "Sandstorm"},
    {"32", "浓雾", "Dense fog", "Dense fog"},
    {"33", "龙卷风", "Tornado", "Tornado"},
    {"34", "弱高吹雪", "Weak high blow snow", "Weak high blow snow"},
    {"35", "轻雾", "Mist", "Mist"},
    {"49", "强浓雾", "Heavy dense fog", "Heavy dense fog"},
    {"53", "霾", "Haze", "Haze"},
    {"54", "中度霾", "Moderate haze", "Moderate haze"},
    {"55", "重度霾", "Severe haze", "Severe haze"},
    {"56", "严重霾", "Hazardous haze", "Hazardous haze"},
    {"57", "大雾", "Heavy fog", "Heavy fog"},
    {"58", "特强浓雾", "Extra-heavy dense fog", "Extra-heavy dense fog"},
    {"301", "雨", "Rain", "Rain"},
    {"302", "雪", "Snow", "Snow"},
};

struct WeatherSnap {
    bool valid = false;
    bool has_temp = false;
    int temp = 0;
    char text[32] = {};
    char icon_code[8] = {};
    char lunar[24] = {};
};

struct TodoItem {
    char title[96] = {};
    char status[16] = {};
    char plan_date[16] = {};
    char plan_time[16] = {};
};

struct UiState {
    lv_obj_t* date_lbl = nullptr;
    lv_obj_t* lunar_lbl = nullptr;
    lv_obj_t* meta_split = nullptr;
    lv_obj_t* weather_row = nullptr;
    lv_obj_t* weather_icon = nullptr;
    lv_obj_t* weather_text = nullptr;
    lv_obj_t* weather_temp = nullptr;
    lv_obj_t* todo_host = nullptr;
    lv_obj_t* home_btn = nullptr;
};

UiState s_ui;
bool s_classic_alive = false;
bool s_as_overlay = false;
WeatherSnap s_weather;
char s_weather_day[9] = {};
std::mutex s_weather_mu;
TaskHandle_t s_fetch_task = nullptr;
std::atomic<bool> s_busy{false};
std::vector<TodoItem> s_todos;

constexpr int kMaxWeatherListeners = 2;
standby_classic::WeatherUiListener s_weather_listeners[kMaxWeatherListeners] = {};

void OpenTaskScreenAsync(void* /*arg*/);
void OnTodoItemClicked(lv_event_t* e);

bool CodeKnown(const char* code) {
    if (code == nullptr || code[0] == '\0') {
        return false;
    }
    for (const auto& p : kPhenoms) {
        if (std::strcmp(p.code, code) == 0) {
            return true;
        }
    }
    return false;
}

const char* MapTextToCode(const char* text) {
    if (text == nullptr || text[0] == '\0') {
        return nullptr;
    }
    for (const auto& p : kPhenoms) {
        if (std::strcmp(text, p.zh) == 0 || std::strcmp(text, p.en_day) == 0 ||
            std::strcmp(text, p.en_night) == 0) {
            return p.code;
        }
    }
    return nullptr;
}

const WeatherPhenom* FindPhenomByCode(const char* code) {
    if (code == nullptr || code[0] == '\0') {
        return nullptr;
    }
    for (const auto& p : kPhenoms) {
        if (std::strcmp(p.code, code) == 0) {
            return &p;
        }
    }
    return nullptr;
}

/** 按当前 UI 语言显示天气现象名（表内已有中英，勿再塞进 language.json） */
const char* LocalizedPhenomText(const WeatherSnap& snap) {
    const WeatherPhenom* p = FindPhenomByCode(snap.icon_code);
    if (p == nullptr && snap.text[0] != '\0') {
        if (const char* code = MapTextToCode(snap.text)) {
            p = FindPhenomByCode(code);
        }
    }
    if (p == nullptr) {
        return snap.text;
    }
    return (std::strcmp(Lang::CODE, "zh-CN") == 0) ? p->zh : p->en_day;
}

const char* WeekdayShortName(int wday) {
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

void FormatPhenomCode(char* out, size_t out_len, int n) {
    if (out == nullptr || out_len == 0) {
        return;
    }
    const unsigned v = static_cast<unsigned>(n < 0 ? 0 : n) % 1000u;
    if (v >= 100u) {
        std::snprintf(out, out_len, "%u", v);
    } else {
        std::snprintf(out, out_len, "%02u", v);
    }
}

bool FormatToday(char* out, size_t out_len) {
    if (out == nullptr || out_len < 9) {
        return false;
    }
    time_t now = time(nullptr);
    struct tm tm_info = {};
    if (localtime_r(&now, &tm_info) == nullptr || tm_info.tm_year < (2020 - 1900)) {
        out[0] = '\0';
        return false;
    }
    const unsigned year = static_cast<unsigned>(tm_info.tm_year + 1900) % 10000u;
    const unsigned month = static_cast<unsigned>(tm_info.tm_mon + 1) % 100u;
    const unsigned day = static_cast<unsigned>(tm_info.tm_mday) % 100u;
    std::snprintf(out, out_len, "%04u%02u%02u", year, month, day);
    return true;
}

bool WeatherRamIsToday() {
    char today[9];
    if (!FormatToday(today, sizeof(today))) {
        return false;
    }
    std::lock_guard<std::mutex> lock(s_weather_mu);
    return s_weather.valid && std::strcmp(s_weather_day, today) == 0;
}

WeatherSnap WeatherRamCopy() {
    std::lock_guard<std::mutex> lock(s_weather_mu);
    return s_weather;
}

void WeatherRamStore(const WeatherSnap& snap, const char* day) {
    std::lock_guard<std::mutex> lock(s_weather_mu);
    s_weather = snap;
    strlcpy(s_weather_day, day != nullptr ? day : "", sizeof(s_weather_day));
}

bool LoadWeatherFromNvs(WeatherSnap& out, char* day_out, size_t day_len) {
    out = WeatherSnap{};
    if (day_out != nullptr && day_len > 0) {
        day_out[0] = '\0';
    }
    Settings settings(kWeatherNs, false);
    const std::string day = settings.GetString("day");
    if (day.size() != 8) {
        return false;
    }
    if (day_out != nullptr && day_len > 0) {
        strlcpy(day_out, day.c_str(), day_len);
    }
    const std::string text = settings.GetString("text");
    const std::string icon = settings.GetString("icon");
    const std::string lunar = settings.GetString("lunar");
    strlcpy(out.text, text.c_str(), sizeof(out.text));
    strlcpy(out.icon_code, icon.c_str(), sizeof(out.icon_code));
    strlcpy(out.lunar, lunar.c_str(), sizeof(out.lunar));
    out.has_temp = settings.GetBool("ht", false);
    out.temp = settings.GetInt("temp", 0);
    out.valid = (out.text[0] != '\0') || out.has_temp;
    // 升级前缓存无农历字段，强制重拉一次
    if (out.valid && !settings.GetBool("lunar_ok", false)) {
        return false;
    }
    return out.valid;
}

void SaveWeatherToNvs(const WeatherSnap& snap, const char* day) {
    if (day == nullptr || day[0] == '\0' || !snap.valid) {
        return;
    }
    Settings settings(kWeatherNs, true);
    settings.SetString("day", day);
    settings.SetString("text", snap.text);
    settings.SetString("icon", snap.icon_code);
    settings.SetString("lunar", snap.lunar);
    settings.SetInt("temp", snap.temp);
    settings.SetBool("ht", snap.has_temp);
    settings.SetBool("lunar_ok", true);
    ESP_LOGI(TAG, "weather nvs saved day=%s text=%s temp=%d icon=%s lunar=%s", day, snap.text,
             snap.temp, snap.icon_code, snap.lunar);
}

void RefreshDateOnce() {
    if (s_ui.date_lbl == nullptr) {
        return;
    }
    time_t now = time(nullptr);
    struct tm tm_info = {};
    if (localtime_r(&now, &tm_info) == nullptr || tm_info.tm_year < (2020 - 1900)) {
        lv_label_set_text(s_ui.date_lbl, Lang::Strings::STANDBY_DATE_PLACEHOLDER);
        return;
    }
    char date_str[48];
    std::snprintf(date_str, sizeof(date_str), Lang::Strings::STANDBY_DATE_FMT, tm_info.tm_mon + 1, tm_info.tm_mday);
    lv_label_set_text(s_ui.date_lbl, date_str);
}

void ApplyWeatherUi() {
    if (!s_classic_alive || s_ui.weather_row == nullptr) {
        return;
    }
    const WeatherSnap snap = WeatherRamCopy();
    if (!snap.valid) {
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
    if (s_ui.weather_text != nullptr) {
        lv_label_set_text(s_ui.weather_text, LocalizedPhenomText(snap));
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
    lv_obj_remove_flag(s_ui.weather_row, LV_OBJ_FLAG_HIDDEN);
    if (s_ui.meta_split != nullptr) {
        lv_obj_remove_flag(s_ui.meta_split, LV_OBJ_FLAG_HIDDEN);
    }
}

void NotifyWeatherListeners() {
    for (int i = 0; i < kMaxWeatherListeners; ++i) {
        if (s_weather_listeners[i] != nullptr) {
            s_weather_listeners[i]();
        }
    }
}

void AsyncApplyWeatherUi(void* /*p*/) {
    ApplyWeatherUi();
    NotifyWeatherListeners();
}

void PostWeatherUi() {
    // classic 未挂载时仍需通知首页 Hero 等 listener
    if (lv_async_call(AsyncApplyWeatherUi, nullptr) != LV_RESULT_OK) {
        ESP_LOGW(TAG, "lv_async_call weather ui failed");
    }
}

void FormatTodoPlanMeta(char* out, size_t out_sz, const TodoItem& item) {
    if (out == nullptr || out_sz == 0) {
        return;
    }
    char time_buf[8] = {};
    if (item.plan_time[0] != '\0') {
        std::snprintf(time_buf, sizeof(time_buf), "%.5s", item.plan_time);
    }
    if (item.plan_date[0] != '\0' && time_buf[0] != '\0') {
        std::snprintf(out, out_sz, "%s %s", item.plan_date, time_buf);
    } else if (item.plan_date[0] != '\0') {
        std::snprintf(out, out_sz, "%s", item.plan_date);
    } else if (time_buf[0] != '\0') {
        std::snprintf(out, out_sz, "%s", time_buf);
    } else {
        out[0] = '\0';
    }
}

void RebuildTodoList() {
    if (!s_classic_alive || s_ui.todo_host == nullptr) {
        return;
    }
    lv_obj_clean(s_ui.todo_host);

    const lv_font_t* font = TodoFont();
    // 与 TaskScreen 普通列表行一致：白底黑框圆角 + 标题/计划时间，无行首勾选图
    const lv_coord_t text_w =
        LV_HOR_RES - 2 * kSidePad - 2 * kTodoRowPad - 2 * kBorderW;

    if (s_todos.empty()) {
        lv_obj_t* empty = lv_label_create(s_ui.todo_host);
        lv_label_set_text(empty, Lang::Strings::STANDBY_EMPTY_TODO);
        lv_obj_set_style_text_font(empty, font, 0);
        lv_obj_set_style_text_color(empty, lv_color_black(), 0);
        lv_obj_set_width(empty, text_w);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_clear_flag(empty, LV_OBJ_FLAG_CLICKABLE);
        return;
    }

    const int n = static_cast<int>(s_todos.size()) < kMaxTodos ? static_cast<int>(s_todos.size())
                                                               : kMaxTodos;
    for (int i = 0; i < n; ++i) {
        const TodoItem& item = s_todos[static_cast<size_t>(i)];
        lv_obj_t* row = lv_obj_create(s_ui.todo_host);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_black(), 0);
        lv_obj_set_style_border_width(row, kBorderW, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_all(row, kTodoRowPad, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        if (!s_as_overlay) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            HapticAttachClick(row);
            lv_obj_add_event_cb(row, OnTodoItemClicked, LV_EVENT_CLICKED, nullptr);
        } else {
            lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        }

        lv_obj_t* text_col = lv_obj_create(row);
        lv_obj_remove_style_all(text_col);
        lv_obj_set_flex_grow(text_col, 1);
        lv_obj_set_height(text_col, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(text_col, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(text_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(text_col, kTodoLineGap, 0);
        lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(text_col, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* name = lv_label_create(text_col);
        lv_label_set_text(name, item.title);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name, text_w);
        lv_obj_set_style_text_font(name, font, 0);
        lv_obj_set_style_text_color(name, lv_color_black(), 0);
        lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);

        char meta[72];
        FormatTodoPlanMeta(meta, sizeof(meta), item);
        if (meta[0] != '\0') {
            lv_obj_t* hint = lv_label_create(text_col);
            lv_label_set_text(hint, meta);
            lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
            lv_obj_set_width(hint, text_w);
            lv_obj_set_style_text_font(hint, font, 0);
            lv_obj_set_style_text_color(hint, lv_color_black(), 0);
            lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

void SetTodoHint(const char* text) {
    if (!s_classic_alive || s_ui.todo_host == nullptr) {
        return;
    }
    lv_obj_clean(s_ui.todo_host);
    lv_obj_t* hint = lv_label_create(s_ui.todo_host);
    lv_label_set_text(hint, text != nullptr ? text : "");
    lv_obj_set_style_text_font(hint, TodoFont(), 0);
    lv_obj_set_style_text_color(hint, lv_color_black(), 0);
    lv_obj_set_width(hint, LV_HOR_RES - 2 * kSidePad - 2 * kTodoRowPad - 2 * kBorderW);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
}

/** 待机页只读 checklist_cache（开机联网 / 百问进页写入）；禁止 HTTP */
void ApplyTodosFromCache() {
    s_todos.clear();
    if (!checklist_cache_ready()) {
        SetTodoHint(Lang::Strings::STANDBY_NO_TODO_CACHE);
        return;
    }
    checklist_cache_item_t buf[kMaxTodos];
    const size_t n = checklist_cache_copy_pending(buf, kMaxTodos);
    for (size_t i = 0; i < n; ++i) {
        TodoItem item;
        strlcpy(item.title, buf[i].title, sizeof(item.title));
        strlcpy(item.status, buf[i].status, sizeof(item.status));
        strlcpy(item.plan_date, buf[i].plan_date, sizeof(item.plan_date));
        strlcpy(item.plan_time, buf[i].plan_time, sizeof(item.plan_time));
        s_todos.push_back(item);
    }
    RebuildTodoList();
}

void OnHomeBtnClicked(lv_event_t* /*e*/) {
    ESP_LOGI(TAG, "home btn -> home");
    lv_async_call(
        [](void*) {
            if (s_classic_alive && s_as_overlay) {
                StandbyScreen::Dismiss();
                return;
            }
            ESP_LOGI(TAG, "return home");
            ScreenGoHome();
        },
        nullptr);
}

void OpenTaskScreenAsync(void* /*arg*/) {
    ScreenNavigateTo(TaskScreen::Create);
}

void OnTodoItemClicked(lv_event_t* /*e*/) {
    ESP_LOGI(TAG, "todo item -> task screen");
    lv_async_call(OpenTaskScreenAsync, nullptr);
}

bool ParseWeatherJson(const std::string& body, WeatherSnap& out, std::string& err) {
    out = WeatherSnap{};
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        err = Lang::Strings::STANDBY_PARSE_FAIL;
        return false;
    }
    cJSON* code = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(code) || code->valueint != 0) {
        cJSON* msg = cJSON_GetObjectItemCaseSensitive(root, "msg");
        err = (cJSON_IsString(msg) && msg->valuestring != nullptr) ? msg->valuestring : Lang::Strings::STANDBY_WEATHER_FAIL;
        cJSON_Delete(root);
        return false;
    }
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(data)) {
        err = Lang::Strings::STANDBY_NO_DATA;
        cJSON_Delete(root);
        return false;
    }

    cJSON* text = cJSON_GetObjectItemCaseSensitive(data, "text");
    if (cJSON_IsString(text) && text->valuestring != nullptr) {
        strlcpy(out.text, text->valuestring, sizeof(out.text));
    }

    cJSON* lunar = cJSON_GetObjectItemCaseSensitive(data, "lunar");
    if (cJSON_IsString(lunar) && lunar->valuestring != nullptr) {
        strlcpy(out.lunar, lunar->valuestring, sizeof(out.lunar));
    }

    cJSON* temp = cJSON_GetObjectItemCaseSensitive(data, "temp");
    if (cJSON_IsNumber(temp)) {
        out.temp = temp->valueint;
        out.has_temp = true;
    }

    char num_buf[8] = {};
    const char* api_code = nullptr;
    cJSON* phen = cJSON_GetObjectItemCaseSensitive(data, "phenomena");
    if (phen == nullptr) {
        phen = cJSON_GetObjectItemCaseSensitive(data, "phenomenon");
    }
    if (cJSON_IsString(phen) && phen->valuestring != nullptr) {
        api_code = phen->valuestring;
    } else if (cJSON_IsNumber(phen)) {
        FormatPhenomCode(num_buf, sizeof(num_buf), phen->valueint);
        api_code = num_buf;
    }
    if (CodeKnown(api_code)) {
        strlcpy(out.icon_code, api_code, sizeof(out.icon_code));
    } else if (const char* mapped = MapTextToCode(out.text)) {
        strlcpy(out.icon_code, mapped, sizeof(out.icon_code));
    }

    out.valid = (out.text[0] != '\0') || out.has_temp;
    cJSON_Delete(root);
    if (!out.valid) {
        err = Lang::Strings::STANDBY_NO_WEATHER;
        return false;
    }
    return true;
}

bool HttpGetWeather(std::string& body_out, std::string& err_out) {
    body_out.clear();
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        err_out = Lang::Strings::STANDBY_NO_NETWORK;
        return false;
    }
    auto http = network->CreateHttp(0);
    if (http == nullptr) {
        err_out = Lang::Strings::STANDBY_CONN_FAIL;
        return false;
    }
    const std::string url = api::WeatherLatestUrl();
    if (url.empty()) {
        err_out = "weather api not configured";
        ESP_LOGI(TAG, "skip weather: cloud endpoints blank");
        return false;
    }
    http->SetTimeout(kHttpTimeoutMs);
    api::ApplyCommonHeaders(http);
    api::LogHttpRequest(TAG, "GET", url);
    if (!http->Open("GET", url)) {
        err_out = Lang::Strings::STANDBY_REQUEST_FAIL;
        api::LogHttpResponse(TAG, -1, err_out);
        return false;
    }
    const int status = http->GetStatusCode();
    body_out = http->ReadAll();
    http->Close();
    api::LogHttpResponse(TAG, status, api::RedactClawUrlsForLog(body_out));
    if (status < 200 || status >= 300) {
        err_out = Lang::Strings::STANDBY_REQUEST_FAIL;
        return false;
    }
    return true;
}

void EnsureWeatherCachedLocked() {
    char today[9];
    if (!FormatToday(today, sizeof(today))) {
        ESP_LOGW(TAG, "weather skip: time not ready");
        return;
    }
    if (WeatherRamIsToday()) {
        ESP_LOGI(TAG, "weather ram hit day=%s", today);
        return;
    }

    WeatherSnap snap;
    char nvs_day[9] = {};
    if (LoadWeatherFromNvs(snap, nvs_day, sizeof(nvs_day)) && std::strcmp(nvs_day, today) == 0) {
        WeatherRamStore(snap, today);
        ESP_LOGI(TAG, "weather nvs hit day=%s text=%s temp=%d", today, snap.text, snap.temp);
        return;
    }

    if (s_busy.exchange(true)) {
        ESP_LOGI(TAG, "weather http already in flight");
        return;
    }

    // 持锁段单出口，避免 GCC cfgcleanup ICE（try_forward_edges）
    bool done = false;
    if (WeatherRamIsToday()) {
        done = true;
    } else if (LoadWeatherFromNvs(snap, nvs_day, sizeof(nvs_day)) &&
               std::strcmp(nvs_day, today) == 0) {
        WeatherRamStore(snap, today);
        ESP_LOGI(TAG, "weather nvs hit day=%s text=%s temp=%d", today, snap.text, snap.temp);
        done = true;
    } else if (!WifiStation::GetInstance().IsConnected()) {
        // 待机/关 WiFi 场景禁止联网拉天气
        ESP_LOGI(TAG, "weather skip http (wifi down)");
        done = true;
    }

    if (!done) {
        std::string body;
        std::string err;
        if (!HttpGetWeather(body, err)) {
            ESP_LOGW(TAG, "weather http fail: %s", err.c_str());
        } else if (!ParseWeatherJson(body, snap, err)) {
            ESP_LOGW(TAG, "weather parse fail: %s", err.c_str());
        } else {
            SaveWeatherToNvs(snap, today);
            WeatherRamStore(snap, today);
        }
    }
    s_busy.store(false);
}

void EnsureWeatherTask(void* /*arg*/) {
    EnsureWeatherCachedLocked();
    s_fetch_task = nullptr;
    PostWeatherUi();
    // NVS 读写：栈须 INTERNAL
    vTaskDelete(nullptr);
}

void ScheduleWeatherEnsure() {
    if (WeatherRamIsToday()) {
        return;
    }
    if (s_fetch_task != nullptr || s_busy.load()) {
        return;
    }
    if (xTaskCreatePinnedToCore(EnsureWeatherTask, "standby_wx", kWorkerStack, nullptr,
                                tskIDLE_PRIORITY + 2, &s_fetch_task, 0) != pdPASS) {
        s_fetch_task = nullptr;
        ESP_LOGW(TAG, "weather task create fail");
    }
}

/** 在 root 上构建待机 UI；调用方负责日期 / 天气 / 清单刷新 */
void BuildUi(lv_obj_t* root)
{
    lv_obj_set_style_bg_color(root, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    s_todos.clear();

    const lv_coord_t content_w = LV_HOR_RES - 2 * kSidePad;
    const lv_coord_t content_h = LV_VER_RES - kTopPad;

    // 底：气泡+猫咪固定贴底；上：spacer 顶开，天气/清单随条数沉到猫咪上方
    lv_obj_t* center = lv_obj_create(root);
    lv_obj_remove_style_all(center);
    lv_obj_set_size(center, content_w, content_h);
    lv_obj_align(center, LV_ALIGN_TOP_MID, 0, kTopPad);
    lv_obj_set_style_bg_opa(center, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(center, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(center, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(center, 0, 0);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* spacer = lv_obj_create(center);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_width(spacer, lv_pct(100));
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* meta_row = lv_obj_create(center);
    lv_obj_remove_style_all(meta_row);
    lv_obj_set_width(meta_row, lv_pct(100));
    lv_obj_set_height(meta_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(meta_row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(meta_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(meta_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(meta_row, kMetaGap, 0);
    lv_obj_set_style_margin_bottom(meta_row, kSectionGap, 0);
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
    lv_label_set_text(s_ui.date_lbl, Lang::Strings::STANDBY_DATE_PLACEHOLDER);
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

    s_ui.todo_host = lv_obj_create(center);
    lv_obj_remove_style_all(s_ui.todo_host);
    lv_obj_set_width(s_ui.todo_host, lv_pct(100));
    lv_obj_set_height(s_ui.todo_host, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_ui.todo_host, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(s_ui.todo_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_ui.todo_host, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_ui.todo_host, kTodoGap, 0);
    lv_obj_set_style_margin_bottom(s_ui.todo_host, kSectionGap, 0);
    lv_obj_clear_flag(s_ui.todo_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_ui.todo_host, LV_OBJ_FLAG_CLICKABLE);

    s_ui.home_btn = lv_obj_create(center);
    lv_obj_remove_style_all(s_ui.home_btn);
    lv_obj_set_size(s_ui.home_btn, content_w, kHomeBtnH);
    lv_obj_set_style_bg_color(s_ui.home_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_ui.home_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_ui.home_btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_ui.home_btn, kBorderW, 0);
    lv_obj_set_style_radius(s_ui.home_btn, 8, 0);
    lv_obj_set_style_margin_bottom(s_ui.home_btn, kHeroSectionGap, 0);
    lv_obj_clear_flag(s_ui.home_btn, LV_OBJ_FLAG_SCROLLABLE);
    if (!s_as_overlay) {
        lv_obj_add_flag(s_ui.home_btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(s_ui.home_btn);
        lv_obj_add_event_cb(s_ui.home_btn, OnHomeBtnClicked, LV_EVENT_CLICKED, nullptr);
    } else {
        lv_obj_add_flag(s_ui.home_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_ui.home_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_margin_bottom(s_ui.home_btn, 0, 0);
    }

    lv_obj_t* menu_icon = lv_image_create(s_ui.home_btn);
    lv_image_set_src(menu_icon, "A:ic_s_standby_menu.spng");
    lv_obj_set_size(menu_icon, kMenuIcon, kMenuIcon);
    lv_obj_center(menu_icon);
    lv_obj_clear_flag(menu_icon, LV_OBJ_FLAG_CLICKABLE);

    // 气泡+猫咪固定贴底（位置不随清单变化）
    lv_obj_t* hero = lv_obj_create(center);
    lv_obj_remove_style_all(hero);
    lv_obj_set_width(hero, content_w);
    lv_obj_set_height(hero, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(hero, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(hero, kHeroInnerGap, 0);
    lv_obj_clear_flag(hero, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(hero, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* bubble = lv_image_create(hero);
    lv_image_set_src(bubble, "A:ic_s_standby_bubble.spng");
    lv_obj_set_size(bubble, kBubbleW, kBubbleH);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* cat = lv_image_create(hero);
    lv_image_set_src(cat, "A:ic_s_standby_cat.spng");
    lv_obj_set_size(cat, kCatW, kCatH);
    lv_obj_clear_flag(cat, LV_OBJ_FLAG_CLICKABLE);
}

void StartOverlayRuntime()
{
    RefreshDateOnce();
    ApplyWeatherUi();
    ApplyTodosFromCache();
    ScheduleWeatherEnsure();
}

}  // namespace


namespace standby_classic {

void Build(lv_obj_t* root, bool as_overlay)
{
    if (root == nullptr) {
        return;
    }
    s_as_overlay = as_overlay;
    s_classic_alive = true;
    s_ui = UiState{};
    BuildUi(root);
}

void StartRuntime()
{
    if (!s_classic_alive) {
        return;
    }
    StartOverlayRuntime();
}

void StopRuntime()
{
}

void Teardown()
{
    StopRuntime();
    s_classic_alive = false;
    s_as_overlay = false;
    s_todos.clear();
    s_ui = UiState{};
}

void EnsureWeatherCached()
{
    EnsureWeatherCachedLocked();
    PostWeatherUi();
}

bool CopyWeatherView(WeatherView* out)
{
    if (out == nullptr) {
        return false;
    }
    *out = WeatherView{};
    const WeatherSnap snap = WeatherRamCopy();
    if (!snap.valid) {
        return false;
    }
    out->valid = true;
    out->has_temp = snap.has_temp;
    out->temp = snap.temp;
    // 按当前 UI 语言填现象名（首页 Hero 等只读此字段）
    strlcpy(out->text, LocalizedPhenomText(snap), sizeof(out->text));
    strlcpy(out->icon_code, snap.icon_code, sizeof(out->icon_code));
    strlcpy(out->lunar, snap.lunar, sizeof(out->lunar));
    return true;
}

void FormatLunarOrWeekday(char* out, size_t out_len, const char* lunar_zh)
{
    if (out == nullptr || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (std::strcmp(Lang::CODE, "zh-CN") == 0) {
        if (lunar_zh != nullptr) {
            strlcpy(out, lunar_zh, out_len);
        }
        return;
    }
    time_t now = time(nullptr);
    struct tm tm_info = {};
    if (localtime_r(&now, &tm_info) == nullptr) {
        return;
    }
    strlcpy(out, WeekdayShortName(tm_info.tm_wday), out_len);
}

void AddWeatherUiListener(WeatherUiListener cb)
{
    if (cb == nullptr) {
        return;
    }
    for (int i = 0; i < kMaxWeatherListeners; ++i) {
        if (s_weather_listeners[i] == cb) {
            return;
        }
    }
    for (int i = 0; i < kMaxWeatherListeners; ++i) {
        if (s_weather_listeners[i] == nullptr) {
            s_weather_listeners[i] = cb;
            return;
        }
    }
    ESP_LOGW(TAG, "weather listener slots full");
}

void RemoveWeatherUiListener(WeatherUiListener cb)
{
    if (cb == nullptr) {
        return;
    }
    for (int i = 0; i < kMaxWeatherListeners; ++i) {
        if (s_weather_listeners[i] == cb) {
            s_weather_listeners[i] = nullptr;
        }
    }
}

void RequestWeatherEnsure()
{
    ScheduleWeatherEnsure();
}

}  // namespace standby_classic
