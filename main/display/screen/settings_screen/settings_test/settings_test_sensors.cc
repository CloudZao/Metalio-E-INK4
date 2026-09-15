#include "settings_test_sensors.h"

#include "settings_test_audio.h"
#include "settings_test_common.h"

#include "assets/lang_config.h"
#include "bq27220_gauge.h"
#include "cx25601n.h"
#include "pcf8563.h"
#include "sc7a20h.h"
#include "SdCardManager.hpp"

#include <cstdio>
#include <ctime>

namespace settings_test_sensors_detail {

const char* ChrgStatUi(uint8_t stat) {
    switch (stat) {
    case CX25601N_CHG_STAT_NOT:
        return Lang::Strings::SETTINGS_TEST_BATTERY_CHG_NOT;
    case CX25601N_CHG_STAT_CC:
        return Lang::Strings::SETTINGS_TEST_BATTERY_CHG_CC;
    case CX25601N_CHG_STAT_CV:
        return Lang::Strings::SETTINGS_TEST_BATTERY_CHG_CV;
    case CX25601N_CHG_STAT_TOPOFF:
        return Lang::Strings::SETTINGS_TEST_BATTERY_CHG_TOPOFF;
    default:
        return Lang::Strings::COMMON_UNKNOWN;
    }
}

void PollBq27220() {
    auto& gauge = Bq27220Gauge::GetInstance();
    if (!gauge.IsReady()) {
        SettingsTest_SetRowValue(SettingsTest_Ui().bq27220, Lang::Strings::SETTINGS_TEST_NOT_DETECTED, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().bq27220, false);
        return;
    }
    uint16_t mv = 0;
    if (!gauge.GetVoltageMv(mv)) {
        SettingsTest_SetRowValue(SettingsTest_Ui().bq27220, Lang::Strings::BOOK_READ_FAIL, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().bq27220, false);
        return;
    }
    SettingsTest_SetRowValue(SettingsTest_Ui().bq27220, Lang::Strings::SETTINGS_TEST_OK, false);
    SettingsTest_SetRowStatus(SettingsTest_Ui().bq27220, true);
}

void PollPcf8563() {
    auto& rtc = Pcf8563::GetInstance();
    if (!rtc.IsReady()) {
        SettingsTest_SetRowValue(SettingsTest_Ui().pcf8563, Lang::Strings::SETTINGS_TEST_NOT_DETECTED, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().pcf8563, false);
        return;
    }
    struct tm t {};
    bool valid = false;
    if (!rtc.GetTime(t, &valid)) {
        SettingsTest_SetRowValue(SettingsTest_Ui().pcf8563, Lang::Strings::BOOK_READ_FAIL, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().pcf8563, false);
        return;
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d%s", t.tm_hour, t.tm_min, valid ? "" : " VL");
    SettingsTest_SetRowValue(SettingsTest_Ui().pcf8563, buf, false);
    SettingsTest_SetRowStatus(SettingsTest_Ui().pcf8563, true);
}

void PollCx25601() {
    if (!cx25601n_is_ready()) {
        SettingsTest_SetRowValue(SettingsTest_Ui().cx25601, Lang::Strings::SETTINGS_TEST_NOT_DETECTED, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().cx25601, false);
        return;
    }
    uint8_t stat = 0;
    if (cx25601n_get_chrg_stat(&stat) != ESP_OK) {
        SettingsTest_SetRowValue(SettingsTest_Ui().cx25601, Lang::Strings::BOOK_READ_FAIL, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().cx25601, false);
        return;
    }
    SettingsTest_SetRowValue(SettingsTest_Ui().cx25601, ChrgStatUi(stat), false);
    SettingsTest_SetRowStatus(SettingsTest_Ui().cx25601, true);
}

void PollSdCard() {
    auto& sd = SdCardManager::GetInstance();
    if (!sd.IsMounted()) {
        if (!sd.Mount()) {
            SettingsTest_SetRowValue(SettingsTest_Ui().sdcard, Lang::Strings::SETTINGS_TEST_SD_MOUNT_FAIL, true);
            SettingsTest_SetRowStatus(SettingsTest_Ui().sdcard, false);
            return;
        }
    }
    sdmmc_card_t* card = sd.GetCard();
    if (card == nullptr) {
        SettingsTest_SetRowValue(SettingsTest_Ui().sdcard, Lang::Strings::BOOK_READ_FAIL, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().sdcard, false);
        return;
    }
    const uint64_t bytes =
        static_cast<uint64_t>(card->csd.capacity) * card->csd.sector_size;
    char buf[40];
    if (bytes >= 1024ULL * 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f GB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%u MB",
                      static_cast<unsigned>(bytes / (1024ULL * 1024)));
    }
    SettingsTest_SetRowValue(SettingsTest_Ui().sdcard, buf, false);
    SettingsTest_SetRowStatus(SettingsTest_Ui().sdcard, true);
}

void PollSc7a20h() {
    auto& ui = SettingsTest_Ui();
    if (ui.sc7a20h_captured) {
        return;
    }
    auto& accel = Sc7a20h::GetInstance();
    if (!accel.IsReady()) {
        SettingsTest_SetRowValue(ui.sc7a20h, Lang::Strings::SETTINGS_TEST_NOT_DETECTED, true);
        SettingsTest_SetRowStatus(ui.sc7a20h, false);
        ui.sc7a20h_captured = true;
        return;
    }
    float pitch = 0.0f;
    float roll = 0.0f;
    if (!accel.ReadPitchRollDeg(pitch, roll)) {
        SettingsTest_SetRowValue(ui.sc7a20h, Lang::Strings::BOOK_READ_FAIL, true);
        SettingsTest_SetRowStatus(ui.sc7a20h, false);
        // 读失败允许下一轮 poll 再试，不冻结。
        return;
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "P:%+.1f° R:%+.1f°",
                  static_cast<double>(pitch), static_cast<double>(roll));
    SettingsTest_SetRowValue(ui.sc7a20h, buf, false);
    SettingsTest_SetRowStatus(ui.sc7a20h, true);
    ui.sc7a20h_captured = true;
}

void PollSensorsInternal() {
    if (!SettingsTest_IsLive()) {
        return;
    }
    PollBq27220();
    PollPcf8563();
    PollCx25601();
    PollSdCard();
    PollSc7a20h();
}

void OnPollTimer(lv_timer_t* /*t*/) {
    PollSensorsInternal();
    SettingsTestAudio_Poll();
}
void StopPollTimerInternal() {
    if (SettingsTest_Ui().poll_timer != nullptr) {
        lv_timer_delete(SettingsTest_Ui().poll_timer);
        SettingsTest_Ui().poll_timer = nullptr;
    }
}

}  // namespace settings_test_sensors_detail

void SettingsTestSensors_Poll() {
    settings_test_sensors_detail::PollSensorsInternal();
}

void SettingsTestSensors_StopPollTimer() {
    settings_test_sensors_detail::StopPollTimerInternal();
}

void SettingsTestSensors_StartPollTimer() {
    SettingsTestSensors_StopPollTimer();
    SettingsTest_Ui().poll_timer = lv_timer_create(settings_test_sensors_detail::OnPollTimer,
                                                   kSettingsTestPollPeriodMs, nullptr);
}
