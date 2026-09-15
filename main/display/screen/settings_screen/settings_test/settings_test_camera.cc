#include "settings_test_camera.h"

#include "settings_test_common.h"
#include "settings_common.h"

#include "assets/lang_config.h"
#include "fontpack_lvgl.h"
#include "haptic_feedback.h"
#include "image_util.h"
#include "reader_types.h"
#include "usb_uvc_still.h"

#include <cstdio>
#include <memory>
#include <new>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr const char* TAG = "SettingsTestCam";
constexpr uint32_t kDetectTimeoutMs = 8000;
constexpr uint32_t kCaptureTimeoutMs = 20000;
constexpr int kWorkerStack = 10 * 1024;
constexpr UBaseType_t kWorkerPrio = 5;
constexpr lv_coord_t kOverlayPad = 14;
constexpr lv_coord_t kPhotoBtnW = 102; // 容纳英文 Capture
constexpr lv_coord_t kPhotoBtnH = 40;
constexpr lv_coord_t kCameraTitleW = 96; // 容纳英文 Camera

lv_obj_t* s_mask = nullptr;
lv_obj_t* s_img = nullptr;
lv_obj_t* s_photo_btn = nullptr;
lv_obj_t* s_overlay_photo_btn = nullptr;
std::unique_ptr<reader::RasterImage> s_raster;
bool s_busy = false;
bool s_detect_ok = false;
char s_detect_id[40]{};
uint32_t s_session_gen = 0;

struct DetectJob {
    uint32_t gen = 0;
};

struct DetectDoneMsg {
    uint32_t gen = 0;
    esp_err_t err = ESP_FAIL;
    UsbUvcStill::DetectInfo info{};
};

struct CaptureJob {
    uint32_t gen = 0;
    int max_w = 640;
    int max_h = 360;
};

struct CaptureDoneMsg {
    uint32_t gen = 0;
    esp_err_t err = ESP_FAIL;
    char detail[48]{};
    reader::RasterImage* raster = nullptr;
};

void CloseOverlay();
void StartCapture();

void ApplyIdleRowText() {
    auto& row = SettingsTest_Ui().camera;
    if (s_busy || UsbUvcStill::IsBusy()) {
        SettingsTest_SetRowValue(row, Lang::Strings::SETTINGS_TEST_BUSY, false);
        return;
    }
    if (s_detect_ok) {
        SettingsTest_SetRowValue(row, Lang::Strings::SETTINGS_TEST_PASS, false);
        SettingsTest_SetRowStatus(row, true);
    } else if (s_detect_id[0]) {
        SettingsTest_SetRowValue(row, s_detect_id, true);
        SettingsTest_SetRowStatus(row, false);
    } else {
        SettingsTest_SetRowValue(row, Lang::Strings::SETTINGS_TEST_DETECTING, false);
    }
}

lv_obj_t* MakePhotoButton(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(btn, kSettingsBorderW, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(btn);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, Lang::Strings::SETTINGS_TEST_CAMERA_SHOT);
    lv_obj_set_width(lbl, w - 12);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    return btn;
}

void CloseOverlay() {
    if (s_img != nullptr) {
        lv_image_set_src(s_img, static_cast<const void*>(nullptr));
        s_img = nullptr;
    }
    s_overlay_photo_btn = nullptr;
    if (s_mask != nullptr) {
        lv_obj_delete(s_mask);
        s_mask = nullptr;
    }
    s_raster.reset();
}

void OnOverlayCloseClicked(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    CloseOverlay();
}

void OnOverlayPhotoClicked(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    StartCapture();
}

void ShowOverlay(reader::RasterImage* raster) {
    CloseOverlay();
    if (!SettingsTest_IsLive() || raster == nullptr || raster->empty()) {
        delete raster;
        return;
    }

    lv_obj_t* parent = SettingsTest_Ui().root_scr;
    if (parent == nullptr) {
        delete raster;
        return;
    }

    s_raster.reset(raster);
    s_raster->BindDsc();

    lv_obj_t* mask = lv_obj_create(parent);
    s_mask = mask;
    lv_obj_remove_style_all(mask);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(mask, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_style_bg_color(mask, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_COVER, 0);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(mask);
    lv_label_set_text(title, Lang::Strings::SETTINGS_TEST_CAMERA_RESULT);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, kOverlayPad);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* img = lv_image_create(mask);
    s_img = img;
    lv_image_set_src(img, &s_raster->dsc);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -28);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);

    s_overlay_photo_btn = MakePhotoButton(mask, 96, 44, OnOverlayPhotoClicked);
    lv_obj_align(s_overlay_photo_btn, LV_ALIGN_BOTTOM_LEFT, kOverlayPad, -kOverlayPad);

    lv_obj_t* close_btn = lv_obj_create(mask);
    lv_obj_remove_style_all(close_btn);
    lv_obj_set_size(close_btn, 96, 44);
    lv_obj_set_style_bg_color(close_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(close_btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(close_btn, kSettingsBorderW, 0);
    lv_obj_set_style_radius(close_btn, 8, 0);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
    HapticAttachClick(close_btn);
    lv_obj_add_event_cb(close_btn, OnOverlayCloseClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_RIGHT, -kOverlayPad, -kOverlayPad);

    lv_obj_t* close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, Lang::Strings::SETTINGS_TEST_WIFI_CLOSE);
    lv_obj_set_style_text_font(close_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_center(close_lbl);
    lv_obj_clear_flag(close_lbl, LV_OBJ_FLAG_CLICKABLE);
}

void OnCaptureDoneAsync(void* user_data) {
    auto* msg = static_cast<CaptureDoneMsg*>(user_data);
    s_busy = false;

    if (msg == nullptr) {
        return;
    }

    const bool stale = (msg->gen != s_session_gen) || !SettingsTest_IsLive();
    if (stale) {
        ESP_LOGI(TAG, "drop stale capture result gen=%u cur=%u", static_cast<unsigned>(msg->gen),
                 static_cast<unsigned>(s_session_gen));
        delete msg->raster;
        delete msg;
        return;
    }

    auto& row = SettingsTest_Ui().camera;
    if (msg->err == ESP_OK && msg->raster != nullptr) {
        ApplyIdleRowText();
        ShowOverlay(msg->raster);
        msg->raster = nullptr;
    } else {
        SettingsTest_SetRowValue(row, msg->detail[0] ? msg->detail : Lang::Strings::SETTINGS_TEST_FAIL, true);
        SettingsTest_SetRowStatus(row, false);
        delete msg->raster;
        msg->raster = nullptr;
    }
    delete msg;
}

void CaptureWorker(void* arg) {
    std::unique_ptr<CaptureJob> job(static_cast<CaptureJob*>(arg));
    auto* msg = new (std::nothrow) CaptureDoneMsg();
    if (msg == nullptr) {
        s_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    msg->gen = job ? job->gen : 0;
    const int max_w = job ? job->max_w : 640;
    const int max_h = job ? job->max_h : 360;

    if (!UsbUvcStill::IsSessionActive()) {
        const esp_err_t serr = UsbUvcStill::StartSession(kDetectTimeoutMs);
        if (serr != ESP_OK && serr != ESP_ERR_TIMEOUT) {
            msg->err = serr;
            snprintf(msg->detail, sizeof(msg->detail), "%s", UsbUvcStill::ErrMessage(serr));
            if (!SettingsTest_PostLvAsync(OnCaptureDoneAsync, msg)) {
                delete msg;
                s_busy = false;
            }
            vTaskDelete(nullptr);
            return;
        }
    }

    UsbUvcStill::JpegBuffer jpeg;
    const esp_err_t err = UsbUvcStill::CaptureOne(jpeg, kCaptureTimeoutMs);
    msg->err = err;

    if (err != ESP_OK) {
        snprintf(msg->detail, sizeof(msg->detail), "%s", UsbUvcStill::ErrMessage(err));
        jpeg.Release();
    } else {
        auto* raster = new (std::nothrow) reader::RasterImage();
        if (raster == nullptr) {
            msg->err = ESP_ERR_NO_MEM;
            snprintf(msg->detail, sizeof(msg->detail), "%s", UsbUvcStill::ErrMessage(ESP_ERR_NO_MEM));
        } else if (!reader::DecodeImageToL8(jpeg.data, jpeg.len, max_w, max_h, *raster) ||
                   raster->empty()) {
            msg->err = ESP_FAIL;
            snprintf(msg->detail, sizeof(msg->detail), "%s", Lang::Strings::WALLPAPER_DECODE_FAIL);
            delete raster;
        } else {
            msg->raster = raster;
            snprintf(msg->detail, sizeof(msg->detail), "%ux%u", raster->width, raster->height);
        }
        jpeg.Release();
    }

    if (!SettingsTest_PostLvAsync(OnCaptureDoneAsync, msg)) {
        delete msg->raster;
        delete msg;
        s_busy = false;
    }
    vTaskDelete(nullptr);
}

void StartCapture() {
    if (!SettingsTest_IsLive()) {
        return;
    }
    if (s_busy || UsbUvcStill::IsBusy()) {
        SettingsTest_SetRowValue(SettingsTest_Ui().camera, Lang::Strings::SETTINGS_TEST_CAPTURING, false);
        return;
    }
    if (s_mask != nullptr) {
        CloseOverlay();
    }

    auto* job = new (std::nothrow) CaptureJob();
    if (job == nullptr) {
        SettingsTest_SetRowValue(SettingsTest_Ui().camera, Lang::Strings::BOOK_OUT_OF_MEMORY, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().camera, false);
        return;
    }
    job->gen = s_session_gen;
    job->max_w = static_cast<int>(LV_HOR_RES) - 2 * kOverlayPad;
    job->max_h = static_cast<int>(LV_VER_RES) - 2 * kOverlayPad - 100;
    if (job->max_w < 64) {
        job->max_w = 64;
    }
    if (job->max_h < 64) {
        job->max_h = 64;
    }

    s_busy = true;
    SettingsTest_SetRowValue(SettingsTest_Ui().camera, Lang::Strings::SETTINGS_TEST_CAPTURING, false);

    const BaseType_t ok =
        xTaskCreate(CaptureWorker, "test_cam", kWorkerStack, job, kWorkerPrio, nullptr);
    if (ok != pdPASS) {
        delete job;
        s_busy = false;
        SettingsTest_SetRowValue(SettingsTest_Ui().camera, Lang::Strings::SETTINGS_TEST_TASK_CREATE_FAIL, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().camera, false);
    }
}

void OnDetectDoneAsync(void* user_data) {
    auto* msg = static_cast<DetectDoneMsg*>(user_data);
    s_busy = false;
    if (msg == nullptr) {
        return;
    }

    const bool stale = (msg->gen != s_session_gen) || !SettingsTest_IsLive();
    if (stale) {
        delete msg;
        return;
    }

    s_detect_ok = (msg->err == ESP_OK && msg->info.ok);
    if (s_detect_ok) {
        s_detect_id[0] = '\0';
    } else if (msg->err == ESP_ERR_TIMEOUT) {
        snprintf(s_detect_id, sizeof(s_detect_id), "%s", Lang::Strings::SETTINGS_TEST_NOT_PASS);
    } else {
        snprintf(s_detect_id, sizeof(s_detect_id), "%s", UsbUvcStill::ErrMessage(msg->err));
    }
    ApplyIdleRowText();
    delete msg;
}

void DetectWorker(void* arg) {
    std::unique_ptr<DetectJob> job(static_cast<DetectJob*>(arg));
    // OnLoad 已拉低 P0.0(USB_MUX_SEL)，直接 Detect，不再额外等待
    if (!job || job->gen != s_session_gen) {
        s_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    auto* msg = new (std::nothrow) DetectDoneMsg();
    if (msg == nullptr) {
        s_busy = false;
        vTaskDelete(nullptr);
        return;
    }
    msg->gen = job->gen;
    msg->err = UsbUvcStill::Detect(msg->info, kDetectTimeoutMs);

    if (!SettingsTest_PostLvAsync(OnDetectDoneAsync, msg)) {
        delete msg;
        s_busy = false;
    }
    vTaskDelete(nullptr);
}

void StartDetect() {
    if (!SettingsTest_IsLive()) {
        return;
    }
    if (s_busy || UsbUvcStill::IsBusy()) {
        return;
    }

    auto* job = new (std::nothrow) DetectJob();
    if (job == nullptr) {
        SettingsTest_SetRowValue(SettingsTest_Ui().camera, Lang::Strings::BOOK_OUT_OF_MEMORY, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().camera, false);
        return;
    }
    job->gen = s_session_gen;
    s_busy = true;
    s_detect_ok = false;
    s_detect_id[0] = '\0';
    SettingsTest_SetRowValue(SettingsTest_Ui().camera, Lang::Strings::SETTINGS_TEST_DETECTING, false);

    const BaseType_t ok =
        xTaskCreate(DetectWorker, "test_cam_det", kWorkerStack, job, kWorkerPrio, nullptr);
    if (ok != pdPASS) {
        delete job;
        s_busy = false;
        SettingsTest_SetRowValue(SettingsTest_Ui().camera, Lang::Strings::SETTINGS_TEST_TASK_CREATE_FAIL, true);
        SettingsTest_SetRowStatus(SettingsTest_Ui().camera, false);
    }
}

void OnRowClicked(lv_event_t* /*e*/) {
    StartCapture();
}

void OnRowPhotoClicked(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    StartCapture();
}

}  // namespace

void SettingsTestCamera_BuildRow(lv_obj_t* parent) {
    auto& ui = SettingsTest_Ui();

    lv_obj_t* row = lv_obj_create(parent);
    ui.camera.row = row;
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
    lv_obj_add_event_cb(row, OnRowClicked, LV_EVENT_CLICKED, nullptr);

    ui.camera.icon = lv_image_create(row);
    lv_obj_set_size(ui.camera.icon, kSettingsTestIconSz, kSettingsTestIconSz);
    lv_obj_clear_flag(ui.camera.icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui.camera.icon, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title_lbl = lv_label_create(row);
    lv_label_set_text(title_lbl, Lang::Strings::SETTINGS_TEST_CAMERA);
    lv_obj_set_width(title_lbl, kCameraTitleW);
    lv_obj_set_height(title_lbl, lv_font_get_line_height(fontpack_lv_font_ui()));
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(title_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(title_lbl, LV_OBJ_FLAG_CLICKABLE);

    ui.camera.value = lv_label_create(row);
    lv_label_set_text(ui.camera.value, "…");
    lv_obj_set_flex_grow(ui.camera.value, 1);
    lv_obj_set_height(ui.camera.value, lv_font_get_line_height(fontpack_lv_font_ui()));
    lv_label_set_long_mode(ui.camera.value, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(ui.camera.value, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(ui.camera.value, lv_color_black(), 0);
    lv_obj_set_style_text_align(ui.camera.value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_clear_flag(ui.camera.value, LV_OBJ_FLAG_CLICKABLE);

    s_photo_btn = MakePhotoButton(row, kPhotoBtnW, kPhotoBtnH, OnRowPhotoClicked);
}

void SettingsTestCamera_OnLoad() {
    CloseOverlay();
    ++s_session_gen;
    s_detect_ok = false;
    s_detect_id[0] = '\0';
    UsbUvcStill::SelectUsbMux(true);
    if (!SettingsTest_IsLive()) {
        return;
    }
    StartDetect();
}

void SettingsTestCamera_Teardown() {
    ++s_session_gen;
    CloseOverlay();
    s_photo_btn = nullptr;
    s_busy = false;
    // 退页会重启整机释放 Host；此处只切回烧录 MUX
    UsbUvcStill::SelectUsbMux(false);
}
