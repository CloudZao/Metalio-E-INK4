#include "settings_about_tab.h"

#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "display.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "ota.h"
#include "ota_confirm_dialog/ota_confirm_dialog.h"
#include "reader/file_size.h"
#include "settings_common.h"
#include "system_info.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

#include <esp_app_desc.h>
#include <esp_chip_info.h>
#include <esp_log.h>
#include <esp_lv_adapter.h>
#include <esp_psram.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

namespace {

constexpr const char* TAG = "SettingsAbout";
constexpr lv_coord_t kInfoRowH = 72;
constexpr lv_coord_t kInfoRowGap = 8;
constexpr lv_coord_t kInfoRowPadH = 12;
constexpr lv_coord_t kInfoRowPadV = 8;

std::atomic<bool> s_tab_alive{false};
std::atomic<uint32_t> s_check_gen{0};
std::atomic<bool> s_check_busy{false};

struct InfoItem {
    const char* label;
    char value[96];
    bool firmware_row = false;
};

enum class CheckResultKind : uint8_t {
    Failed = 0,
    Latest = 1,
    NewVersion = 2,
};

struct CheckResultMsg {
    uint32_t gen = 0;
    CheckResultKind kind = CheckResultKind::Failed;
    char current[48] = {};
    char next[48] = {};
    char url[256] = {};
};

void CollectInfoItems(InfoItem* items, int* count) {
    const auto* app_desc = esp_app_get_description();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    int idx = 0;

    items[idx].label = Lang::Strings::SETTINGS_ABOUT_MODEL;
    std::snprintf(items[idx].value, sizeof(items[idx].value), "%s", BOARD_NAME);
    ++idx;

    items[idx].label = Lang::Strings::SETTINGS_ABOUT_CHIP;
    {
        const std::string chip = SystemInfo::GetChipModelName();
        std::snprintf(items[idx].value, sizeof(items[idx].value), "%s", chip.c_str());
    }
    ++idx;

    items[idx].label = Lang::Strings::SETTINGS_ABOUT_CORES;
    std::snprintf(items[idx].value, sizeof(items[idx].value), Lang::Strings::SETTINGS_ABOUT_CORES_FMT,
                  static_cast<unsigned>(chip_info.cores));
    ++idx;

    items[idx].label = Lang::Strings::SETTINGS_ABOUT_FW_CHECK;
    std::snprintf(items[idx].value, sizeof(items[idx].value), Lang::Strings::SETTINGS_ABOUT_FW_VER_FMT,
                  app_desc->version);
    items[idx].firmware_row = true;
    ++idx;

    items[idx].label = Lang::Strings::SETTINGS_ABOUT_BUILD;
    std::snprintf(items[idx].value, sizeof(items[idx].value), "%s %s", app_desc->date,
                  app_desc->time);
    ++idx;

    items[idx].label = Lang::Strings::SETTINGS_ABOUT_MAC;
    {
        const std::string mac = SystemInfo::GetMacAddress();
        std::snprintf(items[idx].value, sizeof(items[idx].value), "%s", mac.c_str());
    }
    ++idx;

    items[idx].label = Lang::Strings::SETTINGS_ABOUT_FLASH;
    reader::FormatFileSize(items[idx].value, sizeof(items[idx].value), SystemInfo::GetFlashSize());
    ++idx;

    items[idx].label = Lang::Strings::SETTINGS_ABOUT_PSRAM;
    {
        const size_t psram_total = esp_psram_get_size();
        if (psram_total == 0) {
            std::snprintf(items[idx].value, sizeof(items[idx].value), Lang::Strings::SETTINGS_ABOUT_NONE);
        } else {
            reader::FormatFileSize(items[idx].value, sizeof(items[idx].value), psram_total);
        }
    }
    ++idx;

    *count = idx;
}

void SetStatusTip(const char* text) {
    auto* display = Board::GetInstance().GetDisplay();
    if (display != nullptr && text != nullptr) {
        display->SetStatus(text);
    }
}

void AsyncApplyCheckResult(void* p) {
    auto* msg = static_cast<CheckResultMsg*>(p);
    if (msg == nullptr) {
        return;
    }
    const uint32_t gen = msg->gen;
    const bool alive = s_tab_alive.load(std::memory_order_acquire);
    const bool current = (gen == s_check_gen.load(std::memory_order_acquire));

    if (!alive || !current) {
        s_check_busy.store(false, std::memory_order_release);
        delete msg;
        return;
    }

    if (msg->kind == CheckResultKind::Failed) {
        SetStatusTip(Lang::Strings::OTA_CHECK_FAILED);
        s_check_busy.store(false, std::memory_order_release);
        delete msg;
        return;
    }
    if (msg->kind == CheckResultKind::Latest) {
        SetStatusTip(Lang::Strings::OTA_ALREADY_LATEST);
        s_check_busy.store(false, std::memory_order_release);
        delete msg;
        return;
    }

    // NewVersion：弹出两按钮确认；升级在主循环执行
    const std::string url = msg->url;
    const std::string cur = msg->current;
    const std::string next = msg->next;
    delete msg;

    const bool shown = OtaConfirmDialog::ShowAsync(
        cur.c_str(), next.c_str(), OtaConfirmDialog::Mode::About,
        [url, cur, next, gen](OtaConfirmDialog::Choice choice) {
            s_check_busy.store(false, std::memory_order_release);
            if (choice != OtaConfirmDialog::Choice::Upgrade) {
                return;
            }
            if (!s_tab_alive.load(std::memory_order_acquire) ||
                gen != s_check_gen.load(std::memory_order_acquire)) {
                return;
            }
            Application::GetInstance().Schedule([url, cur, next]() {
                Application::GetInstance().UpgradeFirmwareUrl(url, cur, next);
            });
        });

    if (!shown) {
        SetStatusTip(Lang::Strings::OTA_CHECK_FAILED);
        s_check_busy.store(false, std::memory_order_release);
    }
}

void CheckFirmwareWorker(void* /*arg*/) {
    auto* msg = new (std::nothrow) CheckResultMsg{};
    if (msg == nullptr) {
        s_check_busy.store(false, std::memory_order_release);
        vTaskDelete(nullptr);
        return;
    }
    msg->gen = s_check_gen.load(std::memory_order_acquire);
    msg->kind = CheckResultKind::Failed;

    auto& board = Board::GetInstance();
    if (!board.EnsureNetworkReady()) {
        ESP_LOGW(TAG, "network not ready");
    } else {
        Ota ota;
        const esp_err_t err = ota.CheckVersion();
        if (err == ESP_OK) {
            std::snprintf(msg->current, sizeof(msg->current), "%s", ota.GetCurrentVersion().c_str());
            if (ota.HasNewVersion()) {
                msg->kind = CheckResultKind::NewVersion;
                std::snprintf(msg->next, sizeof(msg->next), "%s", ota.GetFirmwareVersion().c_str());
                std::snprintf(msg->url, sizeof(msg->url), "%s", ota.GetFirmwareUrl().c_str());
            } else {
                msg->kind = CheckResultKind::Latest;
            }
        } else {
            ESP_LOGW(TAG, "CheckVersion failed: %s", esp_err_to_name(err));
        }
    }

    bool posted = false;
    if (esp_lv_adapter_is_initialized() && esp_lv_adapter_lock(-1) == ESP_OK) {
        posted = (lv_async_call(AsyncApplyCheckResult, msg) == LV_RESULT_OK);
        esp_lv_adapter_unlock();
    }
    if (!posted) {
        s_check_busy.store(false, std::memory_order_release);
        delete msg;
    }
    vTaskDelete(nullptr);
}

void OnFirmwareRowClicked(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_tab_alive.load(std::memory_order_acquire)) {
        return;
    }
    if (s_check_busy.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    if (OtaConfirmDialog::IsActive()) {
        s_check_busy.store(false, std::memory_order_release);
        return;
    }

    s_check_gen.fetch_add(1, std::memory_order_acq_rel);
    SetStatusTip(Lang::Strings::CHECKING_NEW_VERSION);

    const BaseType_t ok =
        xTaskCreate(CheckFirmwareWorker, "about_ota", 12288, nullptr, 5, nullptr);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "create check task failed");
        SetStatusTip(Lang::Strings::OTA_CHECK_FAILED);
        s_check_busy.store(false, std::memory_order_release);
    }
}

lv_obj_t* CreateInfoRow(lv_obj_t* parent, const InfoItem& item, bool with_divider) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, kInfoRowH);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    if (with_divider) {
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_black(), 0);
    }
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_hor(row, kInfoRowPadH, 0);
    lv_obj_set_style_pad_ver(row, kInfoRowPadV, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(row, 2, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    if (item.firmware_row) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(row);
        lv_obj_add_event_cb(row, OnFirmwareRowClicked, LV_EVENT_CLICKED, nullptr);
    } else {
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t* title = lv_label_create(row);
    lv_label_set_text(title, item.label);
    lv_obj_set_width(title, lv_pct(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* subtitle = lv_label_create(row);
    lv_label_set_text(subtitle, item.value);
    lv_obj_set_width(subtitle, lv_pct(100));
    lv_label_set_long_mode(subtitle, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(subtitle, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(subtitle, lv_color_black(), 0);
    lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_clear_flag(subtitle, LV_OBJ_FLAG_CLICKABLE);

    return row;
}

}  // namespace

void SettingsAboutTab_Reset() {
    s_tab_alive.store(false, std::memory_order_release);
    s_check_gen.fetch_add(1, std::memory_order_acq_rel);
    s_check_busy.store(false, std::memory_order_release);
    if (OtaConfirmDialog::IsActive()) {
        OtaConfirmDialog::Dismiss();
    }
}

void SettingsAboutTab_Build(lv_obj_t* page) {
    s_tab_alive.store(true, std::memory_order_release);
    s_check_busy.store(false, std::memory_order_release);

    InfoItem items[8];
    int count = 0;
    CollectInfoItems(items, &count);

    lv_obj_t* list = lv_obj_create(page);
    lv_obj_remove_style_all(list);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_height(list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list, kInfoRowGap, 0);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < count; ++i) {
        CreateInfoRow(list, items[i], i + 1 < count);
    }
}
