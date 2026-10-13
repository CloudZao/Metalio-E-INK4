#include "metalio_display_setup.h"

#include "config.h"
#include "display/lv_adapter_display.h"
#include "display/screen/vk_key_handler.h"
#include "haptic_feedback.h"
#include "power_policy.h"

void MetalioTouch_AfterWake(void) {
    if (auto* disp = LVAdapterDisplay::Instance()) {
        disp->KickTouchInput();
    }
}

static bool MetalioDisplay_OnTouchVirtualKeyEvent(const char* name, TouchVkEvent event,
                                                  void* /*user_data*/) {
    if (name == nullptr) {
        return false;
    }
    switch (event) {
        case TouchVkEvent::Click:
            // 短震已在 touch_feed 按下边沿发出，此处勿再震（否则松手连震）
            VkKey_Dispatch(name);
            return false;
        case TouchVkEvent::LongPress: {
            // 长按阈值到达：再震一次区分「短按确认」与「长按动作」
            const bool consumed = VkKey_OnLongPress(name);
            if (consumed) {
                HapticPulseIfEnabled();
            }
            return consumed;
        }
        case TouchVkEvent::PressUp:
            VkKey_OnPressUp(name);
            return false;
    }
    return false;
}

Display* MetalioDisplay_Create(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io,
                               esp_lcd_touch_handle_t touch) {
    Display* display = new LVAdapterDisplay(panel, panel_io, touch, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    if (touch == nullptr) {
        return display;
    }

    static const TouchVirtualKey kTouchKeys[] = {
        // HOME：与 BOOT 同阈值 kBootLongPressMs(500)；长按→回系统首页（含百问页）
        {"vk_home", TOUCH_VK_HOME_X, TOUCH_VK_HOME_Y, kBootLongPressMs},
        // prev/next：百问长按→首页/末页；其它页未消费则松手仍走短按
        {"vk_prev", TOUCH_VK_PREV_X, TOUCH_VK_PREV_Y, kBootLongPressMs},
        {"vk_next", TOUCH_VK_NEXT_X, TOUCH_VK_NEXT_Y, kBootLongPressMs},
    };
    if (auto* disp = LVAdapterDisplay::Instance()) {
        disp->RegisterTouchVirtualKeys(kTouchKeys, sizeof(kTouchKeys) / sizeof(kTouchKeys[0]),
                                       MetalioDisplay_OnTouchVirtualKeyEvent, nullptr);
    }
    return display;
}
