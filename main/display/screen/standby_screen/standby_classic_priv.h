#pragma once

#include "standby_screen/standby_classic.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

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
constexpr lv_coord_t kTodoRowPad = 10;
constexpr lv_coord_t kTodoGap = 8;
constexpr lv_coord_t kTodoLineGap = 6;
constexpr lv_coord_t kSectionGap = 48;
constexpr lv_coord_t kHeroSectionGap = 20;
constexpr lv_coord_t kHeroInnerGap = 4;
constexpr lv_coord_t kTopPad = 12;
constexpr lv_coord_t kCatW = 424;
constexpr lv_coord_t kCatH = 202;
constexpr lv_coord_t kBubbleW = 316;
constexpr lv_coord_t kBubbleH = 94;
constexpr lv_coord_t kMenuIcon = 30;
constexpr lv_coord_t kBorderW = 2;
constexpr int kMaxTodos = 3;
constexpr int kHttpTimeoutMs = 15000;
constexpr int kWorkerStack = 10 * 1024;
constexpr const char* kWeatherNs = "weather";
constexpr int kMaxWeatherListeners = 2;

struct WeatherPhenom {
    const char* code;
    const char* zh;
    const char* en_day;
    const char* en_night;
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

struct StandbyClassicUiState {
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

extern const WeatherPhenom kPhenoms[];
/** @brief 取经典待机 UI 状态 */
StandbyClassicUiState& StandbyClassic_State();
extern bool StandbyClassic_alive; // 经典待机是否存活
extern bool StandbyClassic_as_overlay; // 是否 Overlay 模式
extern WeatherSnap StandbyClassic_weather; // RAM 天气快照
extern char StandbyClassic_weather_day[9]; // 天气对应日期 YYYYMMDD
extern std::mutex StandbyClassic_weather_mu; // 天气锁
extern TaskHandle_t StandbyClassic_fetch_task; // 拉取任务
extern std::atomic<bool> StandbyClassic_busy; // 拉取忙
extern std::vector<TodoItem> StandbyClassic_todos; // 待办缓存
extern StandbyClassicWeatherUiListener StandbyClassic_weather_listeners[kMaxWeatherListeners]; // 监听表

/** @brief 天气文案字体 */
const lv_font_t* StandbyClassic_WeatherFont();
/** @brief 待办区字体 */
const lv_font_t* StandbyClassic_TodoFont();
/** @brief 现象码是否已知 */
bool StandbyClassic_CodeKnown(const char* code);
/** @brief 天气文本映射为现象码 */
const char* StandbyClassic_MapTextToCode(const char* text);
/** @brief 本地化天气现象文案 */
const char* StandbyClassic_LocalizedPhenomText(const WeatherSnap& snap);
/** @brief 星期短名 */
const char* StandbyClassic_WeekdayShortName(int wday);
/** @brief 格式化现象码显示 */
void StandbyClassic_FormatPhenomCode(char* out, size_t out_len, int n);
/** @brief 格式化今日日期串 */
bool StandbyClassic_FormatToday(char* out, size_t out_len);
/** @brief RAM 天气是否为今日 */
bool StandbyClassic_WeatherRamIsToday();
/** @brief 拷贝 RAM 天气快照 */
WeatherSnap StandbyClassic_WeatherRamCopy();
/** @brief 写入 RAM 天气快照 */
void StandbyClassic_WeatherRamStore(const WeatherSnap& snap, const char* day);
/** @brief 从 NVS 加载天气 */
bool StandbyClassic_LoadWeatherFromNvs(WeatherSnap& out, char* day_out, size_t day_len);
/** @brief 天气写入 NVS */
void StandbyClassic_SaveWeatherToNvs(const WeatherSnap& snap, const char* day);
/** @brief 将天气应用到 UI */
void StandbyClassic_ApplyWeatherUi();
/** @brief 从 checklist_cache 填充待办 */
void StandbyClassic_ApplyTodosFromCache();
/** @brief 调度天气拉取 */
void StandbyClassic_ScheduleWeatherEnsure();
