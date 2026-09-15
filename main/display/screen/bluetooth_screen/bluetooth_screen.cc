#include "bluetooth_screen.h"

#include "board.h"
#include "power_policy.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "settings_common.h"

#include "SimpleUart.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "assets/lang_config.h"

namespace {

constexpr const char* TAG = "BtScreen";

constexpr int kAddrHexLen = 12;

// e-ink-4 扩展器无 BT_POWER 引脚，电源复位不可用。
constexpr bool kBtPowerResetSupported = false;

std::atomic<bool> s_mode_cmd_busy{false};

enum class BtMode : uint8_t {
    kNone = 0,
    kMode1,
    kMode2,
    kMode3,
};

enum class ConnState : uint8_t {
    kIdle,
    kScanning,
    kConnecting,
    kConnected,
};

struct BtDevice {
    char address[kAddrHexLen + 1];
    char name[64];
};

struct UiState {
    lv_obj_t* root = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* mode_btns[3] = {};
    lv_obj_t* mode1_panel = nullptr;
    lv_obj_t* mode2_panel = nullptr;
    lv_obj_t* scan_btn = nullptr;
    lv_obj_t* device_list = nullptr;
    lv_obj_t* music_btn = nullptr;
    lv_obj_t* call_btn = nullptr;
};

UiState s_ui;
BtMode s_active_mode = BtMode::kNone;
ConnState s_conn_state = ConnState::kIdle;
std::string s_rx_buffer;
std::vector<BtDevice> s_devices;
bool s_screen_active = false;

void strip_container(lv_obj_t* obj) {
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void update_status_label(const char* text) {
    if (s_ui.status_label == nullptr) {
        return;
    }
    lv_label_set_text(s_ui.status_label, text);
}

struct AsyncStatusMsg {
    char text[128];
};

void async_update_status(void* user_data) {
    auto* msg = static_cast<AsyncStatusMsg*>(user_data);
    update_status_label(msg->text);
    delete msg;
}

void post_status(const char* text) {
    if (!s_screen_active) {
        return;
    }
    auto* msg = new AsyncStatusMsg{};
    snprintf(msg->text, sizeof(msg->text), "%s", text);
    lv_async_call(async_update_status, msg);
}

void refresh_mode_buttons() {
    for (int i = 0; i < 3; ++i) {
        if (s_ui.mode_btns[i] == nullptr) {
            continue;
        }
        const bool active = (static_cast<int>(s_active_mode) == i + 1);
        SettingsStyleSelectable(s_ui.mode_btns[i], lv_obj_get_child(s_ui.mode_btns[i], 0), active);
    }
}

void show_mode2_panel(bool show) {
    if (s_ui.mode2_panel == nullptr) {
        return;
    }
    if (show) {
        lv_obj_clear_flag(s_ui.mode2_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ui.mode2_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

void show_mode1_panel(bool show) {
    if (s_ui.mode1_panel == nullptr) {
        return;
    }
    if (show) {
        lv_obj_clear_flag(s_ui.mode1_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ui.mode1_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

void clear_device_list_ui() {
    if (s_ui.device_list == nullptr) {
        return;
    }
    lv_obj_clean(s_ui.device_list);
}

struct AsyncAddDeviceMsg {
    char address[kAddrHexLen + 1];
    char name[64];
};

void async_add_device_item(void* user_data) {
    auto* msg = static_cast<AsyncAddDeviceMsg*>(user_data);
    if (s_ui.device_list == nullptr) {
        delete msg;
        return;
    }

    lv_obj_t* item = lv_obj_create(s_ui.device_list);
    lv_obj_remove_style_all(item);
    lv_obj_set_width(item, lv_pct(100));
    lv_obj_set_height(item, kSettingsOptionH);
    lv_obj_set_style_pad_hor(item, 10, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(item);

    char* addr_copy = static_cast<char*>(lv_malloc(kAddrHexLen + 1));
    if (addr_copy != nullptr) {
        memcpy(addr_copy, msg->address, kAddrHexLen + 1);
        lv_obj_add_event_cb(
            item,
            [](lv_event_t* e) {
                const char* addr = static_cast<const char*>(lv_event_get_user_data(e));
                if (addr == nullptr) {
                    return;
                }
                char cmd[48];
                snprintf(cmd, sizeof(cmd), "AT+CONNECT=%s\r\n", addr);
                SimpleUart::getInstance().sendString(cmd);
                ESP_LOGI(TAG, "TX: AT+CONNECT=%s", addr);
                s_conn_state = ConnState::kConnecting;
                char status[64];
                snprintf(status, sizeof(status), Lang::Strings::BT_CONNECTING_FMT, addr);
                post_status(status);
            },
            LV_EVENT_CLICKED, addr_copy);
        lv_obj_add_event_cb(
            item,
            [](lv_event_t* e) {
                char* addr = static_cast<char*>(lv_event_get_user_data(e));
                lv_free(addr);
            },
            LV_EVENT_DELETE, addr_copy);
    }

    lv_obj_t* lbl = lv_label_create(item);
    char display[96];
    if (msg->name[0] != '\0') {
        snprintf(display, sizeof(display), "%s\n%s", msg->name, msg->address);
    } else {
        snprintf(display, sizeof(display), "%s", msg->address);
    }
    lv_label_set_text(lbl, display);
    lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    SettingsStyleSelectable(item, lbl, false);

    delete msg;
}

void add_device_to_list(const char* address, const char* name) {
    if (!s_screen_active) {
        return;
    }
    auto* msg = new AsyncAddDeviceMsg{};
    snprintf(msg->address, sizeof(msg->address), "%s", address);
    snprintf(msg->name, sizeof(msg->name), "%s", name);
    lv_async_call(async_add_device_item, msg);
}

void async_clear_list(void* /*user_data*/) {
    clear_device_list_ui();
}

void post_clear_list() {
    if (!s_screen_active) {
        return;
    }
    lv_async_call(async_clear_list, nullptr);
}

static void async_on_mode1_set(void* /*user_data*/) {
    refresh_mode_buttons();
    show_mode2_panel(false);
    show_mode1_panel(true);
}

static void async_on_mode2_set(void* /*user_data*/) {
    refresh_mode_buttons();
    show_mode1_panel(false);
    show_mode2_panel(true);
}

static void async_on_mode3_set(void* /*user_data*/) {
    refresh_mode_buttons();
    show_mode1_panel(false);
    show_mode2_panel(false);
}

static bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static void trim_line(std::string& line) {
    while (!line.empty() &&
           (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
        line.pop_back();
    }
    size_t start = 0;
    while (start < line.size() && line[start] == ' ') {
        ++start;
    }
    if (start > 0) {
        line = line.substr(start);
    }
}

static bool parse_bt_device_line(const std::string& line, char* address, size_t addr_sz,
                                 char* name, size_t name_sz) {
    constexpr const char* kPrefix = "AT+BT:";
    if (line.rfind(kPrefix, 0) != 0) {
        return false;
    }
    const std::string payload = line.substr(strlen(kPrefix));
    if (payload.size() < static_cast<size_t>(kAddrHexLen)) {
        return false;
    }
    for (int i = 0; i < kAddrHexLen; ++i) {
        if (!is_hex_char(payload[i])) {
            return false;
        }
    }
    snprintf(address, addr_sz, "%.*s", kAddrHexLen, payload.c_str());
    snprintf(name, name_sz, "%s", payload.c_str() + kAddrHexLen);
    return true;
}

static void handle_response_line(const std::string& raw_line) {
    std::string line = raw_line;
    trim_line(line);
    if (line.empty()) {
        return;
    }

    ESP_LOGI(TAG, "RX: %s", line.c_str());

    if (line.find("SET MODE 1") != std::string::npos) {
        s_active_mode = BtMode::kMode1;
        s_conn_state = ConnState::kIdle;
        post_status(Lang::Strings::BT_MODE1_SET);
        lv_async_call(async_on_mode1_set, nullptr);
        return;
    }
    if (line.find("SET MODE 2") != std::string::npos) {
        s_active_mode = BtMode::kMode2;
        s_conn_state = ConnState::kIdle;
        post_status(Lang::Strings::BT_MODE2_SET);
        lv_async_call(async_on_mode2_set, nullptr);
        return;
    }
    if (line.find("SET MODE 3") != std::string::npos) {
        s_active_mode = BtMode::kMode3;
        s_conn_state = ConnState::kIdle;
        post_status(Lang::Strings::BT_MODE3_SET);
        lv_async_call(async_on_mode3_set, nullptr);
        return;
    }

    if (line.find("RECONNECT") != std::string::npos) {
        post_status(line.c_str());
        return;
    }

    if (line.find("INQUIRING START") != std::string::npos) {
        s_conn_state = ConnState::kScanning;
        s_devices.clear();
        post_clear_list();
        post_status(Lang::Strings::BT_SCANNING);
        return;
    }

    char address[kAddrHexLen + 1];
    char name[64];
    if (parse_bt_device_line(line, address, sizeof(address), name, sizeof(name))) {
        BtDevice dev{};
        snprintf(dev.address, sizeof(dev.address), "%s", address);
        snprintf(dev.name, sizeof(dev.name), "%s", name);
        s_devices.push_back(dev);
        add_device_to_list(address, name);
        char status[96];
        snprintf(status, sizeof(status), Lang::Strings::BT_FOUND_FMT, name[0] ? name : address);
        post_status(status);
        return;
    }

    if (line.find("INQ COMPLETE") != std::string::npos) {
        s_conn_state = ConnState::kIdle;
        char status[64];
        snprintf(status, sizeof(status), Lang::Strings::BT_SCAN_DONE_FMT,
                 static_cast<int>(s_devices.size()));
        post_status(status);
        return;
    }

    if (line.find("CONNECTING") != std::string::npos) {
        s_conn_state = ConnState::kConnecting;
        post_status(Lang::Strings::BT_CONNECTING);
        return;
    }

    if (line.find("CONNECT SUCCESS") != std::string::npos) {
        s_conn_state = ConnState::kConnected;
        post_status(Lang::Strings::BT_CONNECT_OK);
        return;
    }

    if (line.find("CONNECT TIMEOUT") != std::string::npos) {
        s_conn_state = ConnState::kIdle;
        post_status(Lang::Strings::BT_CONNECT_TIMEOUT);
        return;
    }

    if (line.find("SETUP SCO") != std::string::npos) {
        post_status(Lang::Strings::BT_CALL_MODE_SCO);
        return;
    }

    if (line.find("DISC SCO") != std::string::npos) {
        post_status(Lang::Strings::BT_MUSIC_MODE_SCO);
        return;
    }

    post_status(line.c_str());
}

static void on_uart_data(const std::vector<uint8_t>& data) {
    s_rx_buffer.append(data.begin(), data.end());

    size_t pos = 0;
    while (true) {
        size_t nl = s_rx_buffer.find('\n', pos);
        if (nl == std::string::npos) {
            break;
        }
        std::string line = s_rx_buffer.substr(pos, nl - pos);
        handle_response_line(line);
        pos = nl + 1;
    }
    if (pos > 0) {
        s_rx_buffer.erase(0, pos);
    }

    if (s_rx_buffer.size() > 2048) {
        ESP_LOGW(TAG, "RX buffer overflow, clearing");
        s_rx_buffer.clear();
    }
}

struct ModeCmdArgs {
    BtMode mode;
};

static void mode_cmd_task(void* param) {
    auto* args = static_cast<ModeCmdArgs*>(param);
    SimpleUart& uart = SimpleUart::getInstance();

    switch (args->mode) {
        case BtMode::kMode1:
            post_status(Lang::Strings::BT_SWITCH_MODE1);
            uart.sendString("AT+RX=2\r\n");
            ESP_LOGI(TAG, "TX: AT+RX=2");
            vTaskDelay(pdMS_TO_TICKS(700));
            uart.sendString("AT+MODE=1\r\n");
            ESP_LOGI(TAG, "TX: AT+MODE=1");
            break;
        case BtMode::kMode2:
            post_status(Lang::Strings::BT_SWITCH_MODE2);
            uart.sendString("AT+TX=1\r\n");
            ESP_LOGI(TAG, "TX: AT+TX=1");
            vTaskDelay(pdMS_TO_TICKS(700));
            uart.sendString("AT+MODE=2\r\n");
            ESP_LOGI(TAG, "TX: AT+MODE=2");
            break;
        case BtMode::kMode3:
            post_status(Lang::Strings::BT_SWITCH_MODE3);
            uart.sendString("AT+RX=1\r\n");
            ESP_LOGI(TAG, "TX: AT+RX=1");
            vTaskDelay(pdMS_TO_TICKS(700));
            uart.sendString("AT+MODE=3\r\n");
            ESP_LOGI(TAG, "TX: AT+MODE=3");
            break;
        default:
            break;
    }

    delete args;
    s_mode_cmd_busy.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

static void send_mode_command(BtMode mode) {
    if (!SimpleUart::getInstance().isInitialized()) {
        post_status(Lang::Strings::BT_UART_NOT_INIT);
        ESP_LOGE(TAG, "SimpleUart not initialized");
        return;
    }
    bool expected = false;
    if (!s_mode_cmd_busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        ESP_LOGW(TAG, "mode cmd busy, skip mode=%u", static_cast<unsigned>(mode));
        return;
    }
    auto* args = new (std::nothrow) ModeCmdArgs{mode};
    if (args == nullptr) {
        s_mode_cmd_busy.store(false, std::memory_order_release);
        ESP_LOGE(TAG, "mode cmd alloc failed");
        return;
    }
    if (xTaskCreate(mode_cmd_task, "bt_mode_cmd", 4096, args, 5, nullptr) != pdPASS) {
        delete args;
        s_mode_cmd_busy.store(false, std::memory_order_release);
        ESP_LOGE(TAG, "mode cmd task create failed");
    }
}

static void call_mode_task(void* /*param*/) {
    auto& board = Board::GetInstance();
    PowerPolicy::GetInstance().Release(PowerNeed::BtAudio);
    PowerPolicy::GetInstance().Acquire(PowerNeed::BtAudio);
    SimpleUart& uart = SimpleUart::getInstance();
    post_status(Lang::Strings::BT_SWITCH_CALL);
    uart.sendString("AT+PP=1\r\n");
    ESP_LOGI(TAG, "TX: AT+PP=1");
    vTaskDelay(pdMS_TO_TICKS(200));
    uart.sendString("AT+BTSCO=1\r\n");
    ESP_LOGI(TAG, "TX: AT+BTSCO=1");
    vTaskDelete(nullptr);
}

static void music_mode_task(void* /*param*/) {
    PowerPolicy::GetInstance().Release(PowerNeed::BtAudio);
    SimpleUart& uart = SimpleUart::getInstance();
    post_status(Lang::Strings::BT_SWITCH_MUSIC);
    uart.sendString("AT+BTSCO=0\r\n");
    ESP_LOGI(TAG, "TX: AT+BTSCO=0");
    vTaskDelay(pdMS_TO_TICKS(200));
    uart.sendString("AT+PP=1\r\n");
    ESP_LOGI(TAG, "TX: AT+PP=1");
    vTaskDelete(nullptr);
}

void on_mode_btn_clicked(lv_event_t* e) {
    const int idx =
        static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    send_mode_command(static_cast<BtMode>(idx + 1));
}

static void async_after_bt_reset(void* /*user_data*/) {
    refresh_mode_buttons();
    show_mode1_panel(false);
    show_mode2_panel(false);
}

static void bt_reset_task(void* /*param*/) {
    if (!kBtPowerResetSupported) {
        post_status(Lang::Strings::BT_PWR_RESET_UNSUP);
        vTaskDelete(nullptr);
        return;
    }
    s_active_mode = BtMode::kNone;
    s_conn_state = ConnState::kIdle;
    lv_async_call(async_after_bt_reset, nullptr);
    post_status(Lang::Strings::BT_PWR_RESET_OK);
    vTaskDelete(nullptr);
}

void on_reset_bt_clicked(lv_event_t* /*e*/) {
    xTaskCreate(bt_reset_task, "bt_reset", 4096, nullptr, 5, nullptr);
}

void on_scan_clicked(lv_event_t* /*e*/) {
    if (s_active_mode != BtMode::kMode2) {
        post_status(Lang::Strings::BT_NEED_MODE2);
        return;
    }
    if (!SimpleUart::getInstance().isInitialized()) {
        post_status(Lang::Strings::BT_UART_NOT_INIT);
        return;
    }
    SimpleUart::getInstance().sendString("AT+INQUIRING\r\n");
    ESP_LOGI(TAG, "TX: AT+INQUIRING");
    s_devices.clear();
    post_clear_list();
    post_status(Lang::Strings::BT_SCAN_START);
}

void on_call_mode_clicked(lv_event_t* /*e*/) {
    if (s_conn_state != ConnState::kConnected) {
        post_status(Lang::Strings::BT_NEED_CONNECT);
        return;
    }
    xTaskCreate(call_mode_task, "bt_call_mode", 4096, nullptr, 5, nullptr);
}

void on_music_mode_clicked(lv_event_t* /*e*/) {
    if (s_conn_state != ConnState::kConnected) {
        post_status(Lang::Strings::BT_NEED_CONNECT);
        return;
    }
    xTaskCreate(music_mode_task, "bt_music_mode", 4096, nullptr, 5, nullptr);
}

void restore_mode_ui() {
    refresh_mode_buttons();
    switch (s_active_mode) {
        case BtMode::kMode1:
            show_mode1_panel(true);
            show_mode2_panel(false);
            update_status_label(Lang::Strings::BT_MODE1_SET);
            break;
        case BtMode::kMode2:
            show_mode1_panel(false);
            show_mode2_panel(true);
            update_status_label(Lang::Strings::BT_MODE2_SET);
            break;
        case BtMode::kMode3:
            show_mode1_panel(false);
            show_mode2_panel(false);
            update_status_label(Lang::Strings::BT_MODE3_SET);
            break;
        default:
            show_mode1_panel(false);
            show_mode2_panel(false);
            break;
    }
}

void reset_ui_state() {
    s_ui = {};
    s_rx_buffer.clear();
    s_devices.clear();
    s_conn_state = ConnState::kIdle;
}

lv_obj_t* make_action_button(lv_obj_t* parent, const char* label, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_height(btn, kSettingsOptionH);
    lv_obj_set_style_pad_hor(btn, 12, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(btn);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    SettingsStyleSelectable(btn, lbl, false);
    return btn;
}

lv_obj_t* make_mode_button(lv_obj_t* parent, const char* label, int idx) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_height(btn, kSettingsOptionH);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(btn);
    lv_obj_add_event_cb(btn, on_mode_btn_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(idx)));
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    SettingsStyleSelectable(btn, lbl, false);
    return btn;
}

}  // namespace

void BluetoothScreen::BuildInto(lv_obj_t* parent) {
    s_conn_state = ConnState::kIdle;
    s_rx_buffer.clear();
    s_devices.clear();
    s_ui.root = parent;

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, Lang::Strings::BT_TITLE);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* desc = lv_label_create(parent);
    lv_label_set_text(desc, Lang::Strings::BT_DESC);
    lv_obj_set_width(desc, lv_pct(100));
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(desc, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(desc, lv_color_black(), 0);
    lv_obj_clear_flag(desc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* reset_row = lv_obj_create(parent);
    strip_container(reset_row);
    lv_obj_set_width(reset_row, lv_pct(100));
    lv_obj_set_height(reset_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(reset_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(reset_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* reset_hint = lv_label_create(reset_row);
    lv_label_set_text(reset_hint, Lang::Strings::BT_RESET_HINT);
    lv_obj_set_style_text_font(reset_hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(reset_hint, lv_color_black(), 0);
    lv_obj_set_flex_grow(reset_hint, 1);
    lv_obj_clear_flag(reset_hint, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* reset = make_action_button(reset_row, Lang::Strings::BT_RESET_BTN, on_reset_bt_clicked);
    lv_obj_set_width(reset, 120);

    lv_obj_t* mode_row = lv_obj_create(parent);
    strip_container(mode_row);
    lv_obj_set_width(mode_row, lv_pct(100));
    lv_obj_set_height(mode_row, kSettingsOptionH);
    lv_obj_set_flex_flow(mode_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mode_row, kSettingsOptionGap, 0);

    const char* mode_labels[] = {Lang::Strings::BT_MODE1, Lang::Strings::BT_MODE2, Lang::Strings::BT_MODE3};
    for (int i = 0; i < 3; ++i) {
        s_ui.mode_btns[i] = make_mode_button(mode_row, mode_labels[i], i);
    }

    lv_obj_t* status = lv_label_create(parent);
    s_ui.status_label = status;
    lv_label_set_text(status, Lang::Strings::BT_SELECT_MODE);
    lv_obj_set_style_text_font(status, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(status, lv_color_black(), 0);
    lv_obj_set_width(status, lv_pct(100));
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_clear_flag(status, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* m1_panel = lv_obj_create(parent);
    s_ui.mode1_panel = m1_panel;
    strip_container(m1_panel);
    lv_obj_set_width(m1_panel, lv_pct(100));
    lv_obj_set_height(m1_panel, 80);
    lv_obj_set_style_border_width(m1_panel, kSettingsBorderW, 0);
    lv_obj_set_style_border_color(m1_panel, lv_color_black(), 0);
    lv_obj_set_style_radius(m1_panel, 8, 0);
    lv_obj_add_flag(m1_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* m1_hint = lv_label_create(m1_panel);
    lv_label_set_text(m1_hint, Lang::Strings::BT_MODE1_ACTIVE);
    lv_obj_set_style_text_font(m1_hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(m1_hint, lv_color_black(), 0);
    lv_obj_center(m1_hint);
    lv_obj_clear_flag(m1_hint, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* panel = lv_obj_create(parent);
    s_ui.mode2_panel = panel;
    strip_container(panel);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_height(panel, 280);
    lv_obj_set_style_border_width(panel, kSettingsBorderW, 0);
    lv_obj_set_style_border_color(panel, lv_color_black(), 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* scan = make_action_button(panel, Lang::Strings::BT_SCAN_BTN, on_scan_clicked);
    s_ui.scan_btn = scan;
    lv_obj_set_width(scan, lv_pct(100));
    lv_obj_align(scan, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* list = lv_obj_create(panel);
    s_ui.device_list = list;
    strip_container(list);
    lv_obj_set_size(list, lv_pct(100), 140);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, kSettingsOptionH + 8);
    lv_obj_set_style_border_width(list, kSettingsBorderW, 0);
    lv_obj_set_style_border_color(list, lv_color_black(), 0);
    lv_obj_set_style_radius(list, 8, 0);
    lv_obj_set_style_pad_all(list, 6, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    lv_obj_t* btn_row = lv_obj_create(panel);
    strip_container(btn_row);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, kSettingsOptionH);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, kSettingsOptionGap, 0);

    lv_obj_t* music = make_action_button(btn_row, Lang::Strings::BT_MUSIC_BTN, on_music_mode_clicked);
    s_ui.music_btn = music;
    lv_obj_set_flex_grow(music, 1);

    lv_obj_t* call = make_action_button(btn_row, Lang::Strings::BT_CALL_BTN, on_call_mode_clicked);
    s_ui.call_btn = call;
    lv_obj_set_flex_grow(call, 1);

    restore_mode_ui();
}

void BluetoothScreen::ResetUi() {
    reset_ui_state();
}

void BluetoothScreen::ApplyDefaultMode() {
    s_active_mode = BtMode::kMode1;
    SimpleUart& uart = SimpleUart::getInstance();
    if (!uart.isInitialized()) {
        ESP_LOGE(TAG, "ApplyDefaultMode: UART not initialized");
        return;
    }
    uart.sendString("AT+RX=2\r\n");
    ESP_LOGI(TAG, "TX: AT+RX=2");
    vTaskDelay(pdMS_TO_TICKS(700));
    uart.sendString("AT+MODE=1\r\n");
    ESP_LOGI(TAG, "TX: AT+MODE=1");
    // 勿在此拉 PA：MAIN_PWR 恢复也会走本函数，进百问会 PA on→off。
    // PA 仅由 PowerPolicy（Speaking / BtAudio / 电话）按需开关。
}

void BluetoothScreen::OnActivated() {
    ESP_LOGI(TAG, "activate: bluetooth tab");
    s_screen_active = true;
    s_rx_buffer.clear();
    SimpleUart::getInstance().registerCallback(on_uart_data);
}

void BluetoothScreen::OnDeactivated() {
    ESP_LOGI(TAG, "deactivate: bluetooth tab");
    PowerPolicy::GetInstance().Release(PowerNeed::BtAudio);
    SimpleUart::getInstance().registerCallback(
        std::function<void(const std::vector<uint8_t>&)>());
    s_screen_active = false;
}
