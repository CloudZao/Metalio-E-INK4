#include "display_orient.h"

#include <cstdlib>

#include <esp_log.h>
#include <lvgl.h>

namespace {

constexpr const char* TAG = "DispOrient";

int s_orient = kDisplayUiPortrait;
int s_panel_w = 800;
int s_panel_h = 480;
lv_display_t* s_disp = nullptr;

int ClampOrient(int orient) {
    if (orient < 0 || orient >= kDisplayUiOrientCount) {
        return kDisplayUiPortrait;
    }
    return orient;
}

void LogicalToPanelAt(int orient, int lx, int ly, int* px, int* py) {
    const int pw = s_panel_w;
    const int ph = s_panel_h;
    switch (ClampOrient(orient)) {
        case kDisplayUiLandLeft:
            *px = lx;
            *py = ly;
            break;
        case kDisplayUiLandRight:
            *px = pw - 1 - lx;
            *py = ph - 1 - ly;
            break;
        case kDisplayUiPortrait:
        default:
            *px = ly;
            *py = ph - 1 - lx;
            break;
    }
}

void TouchToLogicalAt(int orient, int tx, int ty, int* lx, int* ly) {
    const int pw = s_panel_w;
    const int ph = s_panel_h;
    switch (ClampOrient(orient)) {
        case kDisplayUiLandLeft:
            *lx = ty;
            *ly = ph - 1 - tx;
            break;
        case kDisplayUiLandRight:
            *lx = pw - 1 - ty;
            *ly = tx;
            break;
        case kDisplayUiPortrait:
        default:
            *lx = tx;
            *ly = ty;
            break;
    }
}

void FailCheck(const char* what) {
    ESP_LOGE(TAG, "self-check failed: %s", what);
    abort();
}

void SelfCheck() {
    int px = 0, py = 0, lx = 0, ly = 0;
    LogicalToPanelAt(kDisplayUiPortrait, 0, 0, &px, &py);
    if (px != 0 || py != s_panel_h - 1) {
        FailCheck("portrait (0,0)");
    }
    LogicalToPanelAt(kDisplayUiPortrait, s_panel_h - 1, 0, &px, &py);
    if (px != 0 || py != 0) {
        FailCheck("portrait (h-1,0)");
    }
    LogicalToPanelAt(kDisplayUiLandLeft, 0, 0, &px, &py);
    if (px != 0 || py != 0) {
        FailCheck("land-left (0,0)");
    }
    LogicalToPanelAt(kDisplayUiLandRight, 0, 0, &px, &py);
    if (px != s_panel_w - 1 || py != s_panel_h - 1) {
        FailCheck("land-right (0,0)");
    }
    TouchToLogicalAt(kDisplayUiPortrait, 12, 34, &lx, &ly);
    if (lx != 12 || ly != 34) {
        FailCheck("touch portrait");
    }
    TouchToLogicalAt(kDisplayUiLandLeft, 0, 0, &lx, &ly);
    if (lx != 0 || ly != s_panel_h - 1) {
        FailCheck("touch land-left");
    }
    LogicalToPanelAt(kDisplayUiLandLeft, lx, ly, &px, &py);
    if (px != 0 || py != s_panel_h - 1) {
        FailCheck("touch/panel land-left");
    }
}

}  // namespace

void DisplayUiBind(lv_display_t* disp, int panel_w, int panel_h) {
    s_disp = disp;
    if (panel_w > 0 && panel_h > 0) {
        s_panel_w = panel_w;
        s_panel_h = panel_h;
    }
    s_orient = kDisplayUiPortrait;
    SelfCheck();
}

bool DisplayUiSetOrient(int orient) {
    orient = ClampOrient(orient);
    const int log_w = (orient == kDisplayUiPortrait) ? s_panel_h : s_panel_w;
    const int log_h = (orient == kDisplayUiPortrait) ? s_panel_w : s_panel_h;
    const bool changed = (s_orient != orient);
    s_orient = orient;
    if (s_disp != nullptr) {
        lv_display_set_resolution(s_disp, log_w, log_h);
    }
    if (changed) {
        ESP_LOGI(TAG, "orient=%d log=%dx%d", orient, log_w, log_h);
    }
    return changed;
}

int DisplayUiGetOrient(void) {
    return s_orient;
}

bool DisplayUiIsLandscape(void) {
    return s_orient != kDisplayUiPortrait;
}

int DisplayUiLogW(void) {
    return (s_orient == kDisplayUiPortrait) ? s_panel_h : s_panel_w;
}

int DisplayUiLogH(void) {
    return (s_orient == kDisplayUiPortrait) ? s_panel_w : s_panel_h;
}

void DisplayUiLogicalToPanel(int lx, int ly, int* px, int* py) {
    if (px == nullptr || py == nullptr) {
        return;
    }
    LogicalToPanelAt(s_orient, lx, ly, px, py);
}

void DisplayUiTouchToLogical(int tx, int ty, int* lx, int* ly) {
    if (lx == nullptr || ly == nullptr) {
        return;
    }
    TouchToLogicalAt(s_orient, tx, ty, lx, ly);
}
