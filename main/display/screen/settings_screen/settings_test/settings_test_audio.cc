#include "settings_test_audio.h"

#include "settings_test_common.h"

#include "application.h"
#include "assets/lang_config.h"
#include "audio_service.h"
#include "device_state.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "screen_common.h"
#include "settings_common.h"

#include <atomic>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace settings_test_audio_detail {

constexpr const char* kTag = "SettingsTestAudio";
constexpr uint32_t kMaxRecordMs = 5000;
constexpr uint32_t kPlaybackMarginMs = 500;
constexpr uint32_t kConfirmDelayMs = 1000;

enum class State {
    Idle,
    Recording,
    Playing,
};

State s_state = State::Idle;
bool s_wake_disabled = false;
bool s_holding_testing_state = false;
DeviceState s_saved_device_state = kDeviceStateIdle;
int64_t s_record_start_us = 0;

lv_obj_t* s_record_btn = nullptr;
lv_obj_t* s_record_lbl = nullptr;
lv_obj_t* s_confirm_mask = nullptr;
lv_timer_t* s_max_record_timer = nullptr;
lv_timer_t* s_playback_timer = nullptr;
lv_timer_t* s_confirm_timer = nullptr;

std::atomic<bool> s_audio_ready{false};
TaskHandle_t s_audio_init_task = nullptr;

void UpdateButtonUi();
void KickAudioInitIfNeeded();
void LeaveAudioTestingState();

void StopConfirmTimer() {
    if (s_confirm_timer != nullptr) {
        lv_timer_delete(s_confirm_timer);
        s_confirm_timer = nullptr;
    }
}

void CloseConfirmDialog() {
    if (s_confirm_mask != nullptr && SettingsTest_IsLive()) {
        lv_obj_delete(s_confirm_mask);
    }
    s_confirm_mask = nullptr;
}

void OnAudioConfirmResult(bool pass) {
    if (!SettingsTest_IsLive()) {
        return;
    }
    SettingsTest_SetRowStatus(SettingsTest_Ui().audio, pass);
    ESP_LOGI(kTag, "user confirm audio: %s", pass ? "pass" : "fail");
}

void OnConfirmYesClicked(lv_event_t* /*e*/) {
    CloseConfirmDialog();
    OnAudioConfirmResult(true);
}

void OnConfirmNoClicked(lv_event_t* /*e*/) {
    CloseConfirmDialog();
    OnAudioConfirmResult(false);
}

void ShowConfirmDialog() {
    if (!SettingsTest_IsLive() || SettingsTest_Ui().root_scr == nullptr ||
        s_confirm_mask != nullptr) {
        return;
    }

    lv_obj_t* mask = lv_obj_create(SettingsTest_Ui().root_scr);
    s_confirm_mask = mask;
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(mask, 0, 0);
    ScreenApplyDotBackdrop(mask);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* card = lv_obj_create(mask);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, LV_HOR_RES - 48, 220);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, kSettingsBorderW, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* msg = lv_label_create(card);
    lv_label_set_text(msg, Lang::Strings::SETTINGS_TEST_AUDIO_CONFIRM);
    lv_obj_set_width(msg, lv_pct(100));
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(msg, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(msg, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* btn_row = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, 48);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_CLICKABLE);

    auto make_btn = [&](const char* text, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_obj_create(btn_row);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 120, 44);
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, lv_color_black(), 0);
        lv_obj_set_style_border_width(btn, kSettingsBorderW, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
        lv_obj_center(lbl);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    };
    make_btn(Lang::Strings::SETTINGS_TEST_YES, OnConfirmYesClicked);
    make_btn(Lang::Strings::SETTINGS_TEST_NO, OnConfirmNoClicked);
}

void OnConfirmTimer(lv_timer_t* /*t*/) {
    s_confirm_timer = nullptr;
    if (!SettingsTest_IsLive()) {
        return;
    }
    ShowConfirmDialog();
}

void ScheduleConfirmDialog() {
    StopConfirmTimer();
    s_confirm_timer = lv_timer_create(OnConfirmTimer, kConfirmDelayMs, nullptr);
    lv_timer_set_repeat_count(s_confirm_timer, 1);
}

void StopPlaybackTimer() {
    if (s_playback_timer != nullptr) {
        lv_timer_delete(s_playback_timer);
        s_playback_timer = nullptr;
    }
}

void FinishPlaybackUi() {
    if (s_state != State::Playing || !SettingsTest_IsLive()) {
        return;
    }
    StopPlaybackTimer();
    LeaveAudioTestingState();
    s_state = State::Idle;
    UpdateButtonUi();
    ScheduleConfirmDialog();
    ESP_LOGI(kTag, "playback finished");
}

void OnPlaybackTimer(lv_timer_t* /*t*/) {
    s_playback_timer = nullptr;
    FinishPlaybackUi();
}

void SchedulePlaybackUiReset() {
    const int64_t now = esp_timer_get_time();
    int duration_ms = static_cast<int>((now - s_record_start_us) / 1000);
    if (duration_ms < 1) {
        duration_ms = 1;
    }
    const uint32_t wait_ms = static_cast<uint32_t>(duration_ms) + kPlaybackMarginMs;

    StopPlaybackTimer();
    s_playback_timer = lv_timer_create(OnPlaybackTimer, wait_ms, nullptr);
    lv_timer_set_repeat_count(s_playback_timer, 1);
}

void RestoreWakeWord() {
    if (!s_wake_disabled) {
        return;
    }
    Application::GetInstance().GetAudioService().EnableWakeWordDetection(true);
    s_wake_disabled = false;
}

void DisableWakeWordIfNeeded() {
    auto& as = Application::GetInstance().GetAudioService();
    if (!as.IsWakeWordRunning()) {
        return;
    }
    as.EnableWakeWordDetection(false);
    s_wake_disabled = true;
}

// PowerPolicy 仅在 Speaking / AudioTesting 时开 PA；自检只走 EnableAudioTesting，需同步 DeviceState。
void EnterAudioTestingState() {
    if (s_holding_testing_state) {
        return;
    }
    auto& app = Application::GetInstance();
    s_saved_device_state = app.GetDeviceState();
    if (s_saved_device_state != kDeviceStateAudioTesting) {
        app.SetDeviceState(kDeviceStateAudioTesting);
    }
    s_holding_testing_state = true;
}

void LeaveAudioTestingState() {
    if (!s_holding_testing_state) {
        return;
    }
    s_holding_testing_state = false;
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() != kDeviceStateAudioTesting) {
        return;
    }
    DeviceState restore = s_saved_device_state;
    if (restore == kDeviceStateAudioTesting) {
        restore = kDeviceStateIdle;
    }
    app.SetDeviceState(restore);
}

void UpdateButtonUi() {
    lv_obj_t* row = SettingsTest_Ui().audio.row;
    if (!SettingsTest_Ui().alive || row == nullptr || s_record_lbl == nullptr) {
        return;
    }

    switch (s_state) {
    case State::Idle:
        lv_label_set_text(s_record_lbl, Lang::Strings::SETTINGS_TEST_AUDIO_HOLD);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        break;
    case State::Recording:
        lv_label_set_text(s_record_lbl, Lang::Strings::SETTINGS_TEST_AUDIO_RECORDING);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        break;
    case State::Playing:
        lv_label_set_text(s_record_lbl, Lang::Strings::RECORD_PLAYING);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
        break;
    }
}

void StopMaxRecordTimer() {
    if (s_max_record_timer != nullptr) {
        lv_timer_delete(s_max_record_timer);
        s_max_record_timer = nullptr;
    }
}

void StopRecordingAndPlay() {
    if (s_state != State::Recording) {
        return;
    }

    StopMaxRecordTimer();
    auto& as = Application::GetInstance().GetAudioService();
    as.EnableAudioTesting(false);
    RestoreWakeWord();
    s_state = State::Playing;
    UpdateButtonUi();
    SchedulePlaybackUiReset();
    ESP_LOGI(kTag, "record stopped, playback started");
}

void OnMaxRecordTimer(lv_timer_t* /*t*/) {
    s_max_record_timer = nullptr;
    ESP_LOGI(kTag, "max record duration reached (%lums)", kMaxRecordMs);
    StopRecordingAndPlay();
}

void StartRecording() {
    if (s_state != State::Idle || !SettingsTest_IsLive()) {
        return;
    }

    StopConfirmTimer();
    CloseConfirmDialog();

    auto& as = Application::GetInstance().GetAudioService();
    if (!as.IsStarted()) {
        ESP_LOGW(kTag, "audio service not started");
        return;
    }
    if (as.IsAudioProcessorRunning()) {
        ESP_LOGW(kTag, "audio processor busy, skip record");
        return;
    }

    DisableWakeWordIfNeeded();
    EnterAudioTestingState();
    as.ResetDecoder();
    as.EnableAudioTesting(true);

    s_record_start_us = esp_timer_get_time();
    s_state = State::Recording;
    UpdateButtonUi();
    StopMaxRecordTimer();
    s_max_record_timer = lv_timer_create(OnMaxRecordTimer, kMaxRecordMs, nullptr);
    lv_timer_set_repeat_count(s_max_record_timer, 1);
    ESP_LOGI(kTag, "recording started");
}

void OnRecordPressed(lv_event_t* /*e*/) {
    KickAudioInitIfNeeded();
    StartRecording();
}

void OnRecordReleased(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
        return;
    }
    if (s_state == State::Recording) {
        StopRecordingAndPlay();
    }
}

void ForceStopWithoutPlayback() {
    StopMaxRecordTimer();
    StopPlaybackTimer();
    StopConfirmTimer();
    CloseConfirmDialog();

    auto& as = Application::GetInstance().GetAudioService();
    if (s_state == State::Recording) {
        as.ResetDecoder();
        as.EnableAudioTesting(false);
    } else if (s_state == State::Playing) {
        as.ResetDecoder();
    }
    LeaveAudioTestingState();
    RestoreWakeWord();
    s_state = State::Idle;
}

void AsyncUpdateButtonUi(void* /*p*/) {
    if (!SettingsTest_Ui().alive) {
        return;
    }
    UpdateButtonUi();
}

void AudioInitTask(void* /*arg*/) {
    Application::GetInstance().EnsureAudioServiceRunning();
    s_audio_ready.store(Application::GetInstance().GetAudioService().IsStarted());
    s_audio_init_task = nullptr;
    if (SettingsTest_IsLive()) {
        if (!SettingsTest_PostLvAsync(AsyncUpdateButtonUi, nullptr)) {
            ESP_LOGW(kTag, "PostLvAsync for audio ready ui failed");
        }
    }
    vTaskDelete(nullptr);
}

void KickAudioInitIfNeeded() {
    if (s_audio_ready.load() &&
        Application::GetInstance().GetAudioService().IsStarted()) {
        return;
    }
    if (s_audio_init_task != nullptr) {
        return;
    }
    s_audio_ready.store(false);
    if (xTaskCreatePinnedToCore(AudioInitTask, "stest_audio", 8 * 1024, nullptr, 5,
                                &s_audio_init_task, 0) != pdPASS) {
        s_audio_init_task = nullptr;
        ESP_LOGE(kTag, "audio init task create failed");
    }
}

void BuildRowInternal(lv_obj_t* parent) {
    auto& ui = SettingsTest_Ui();

    lv_obj_t* row = lv_obj_create(parent);
    ui.audio.row = row;
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, kSettingsTestRowH);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_black(), 0);
    lv_obj_set_style_pad_hor(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(row);
    lv_obj_add_event_cb(row, OnRecordPressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(row, OnRecordReleased, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(row, OnRecordReleased, LV_EVENT_PRESS_LOST, nullptr);

    ui.audio.icon = lv_image_create(row);
    lv_obj_set_size(ui.audio.icon, kSettingsTestIconSz, kSettingsTestIconSz);
    lv_obj_clear_flag(ui.audio.icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui.audio.icon, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title_lbl = lv_label_create(row);
    lv_label_set_text(title_lbl, Lang::Strings::SETTINGS_TEST_AUDIO_TITLE);
    lv_obj_set_width(title_lbl, 88); // 容纳英文 Audio
    lv_obj_set_height(title_lbl, lv_font_get_line_height(fontpack_lv_font_ui()));
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(title_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_clear_flag(title_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* spacer = lv_obj_create(row);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_height(spacer, 1);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);

    s_record_btn = lv_obj_create(row);
    lv_obj_remove_style_all(s_record_btn);
    lv_obj_set_size(s_record_btn, 152, 40); // 容纳英文 Hold to talk
    lv_obj_set_style_bg_color(s_record_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_record_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_record_btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_record_btn, kSettingsBorderW, 0);
    lv_obj_set_style_radius(s_record_btn, 8, 0);
    lv_obj_clear_flag(s_record_btn, LV_OBJ_FLAG_CLICKABLE);

    s_record_lbl = lv_label_create(s_record_btn);
    lv_label_set_text(s_record_lbl, Lang::Strings::SETTINGS_TEST_AUDIO_HOLD);
    lv_obj_set_width(s_record_lbl, 140);
    lv_label_set_long_mode(s_record_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_record_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_record_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_center(s_record_lbl);
    lv_obj_clear_flag(s_record_lbl, LV_OBJ_FLAG_CLICKABLE);
}

}  // namespace settings_test_audio_detail

void SettingsTestAudio_BuildRow(lv_obj_t* parent) {
    settings_test_audio_detail::BuildRowInternal(parent);
}

void SettingsTestAudio_OnLoad() {
    settings_test_audio_detail::s_state = settings_test_audio_detail::State::Idle;
    settings_test_audio_detail::UpdateButtonUi();
}

void SettingsTestAudio_Teardown() {
    settings_test_audio_detail::ForceStopWithoutPlayback();
    settings_test_audio_detail::s_record_btn = nullptr;
    settings_test_audio_detail::s_record_lbl = nullptr;
    settings_test_audio_detail::s_confirm_mask = nullptr;
    settings_test_audio_detail::s_audio_ready.store(false);
}

void SettingsTestAudio_Poll() {
    if (!SettingsTest_IsLive()) {
        return;
    }
    if (settings_test_audio_detail::s_state != settings_test_audio_detail::State::Playing ||
        settings_test_audio_detail::s_playback_timer != nullptr) {
        return;
    }
    settings_test_audio_detail::FinishPlaybackUi();
}
