#include "lv_adapter_display.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_log.h>
#include <esp_mmap_assets.h>
#include <esp_timer.h>
#include <font_awesome.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "application.h"
#include "assets/lang_config.h"
#include "assistant_screen/assistant_screen.h"
#include "audio_codec.h"
#include "board.h"
#include "power_policy.h"
#include "esp_lv_adapter.h"
#include "esp_lv_fs.h"
#include "book_screen/book_reader_prefs.h"
#include "haptic_feedback.h"
#include "home_screen/home_screen.h"
#include "touch_missing_screen/touch_missing_screen.h"
#include "mmap_generate_resources.h"
#include <wifi_station.h>
#include "a2ui_image.h"
#include "sd_paths.h"
#include "SdCardManager.hpp"
#include "wallpaper_screen/wallpaper_active.h"
#include "reader/book_home_snapshot.h"

#include "esp_lcd_panel_ssd1677.h"
#include "esp_lcd_ssd1677_commands.h"
#include "fontpack_lvgl.h"

#include <cerrno>
#include <sys/stat.h>

static const char* TAG = "LVAdapterDisplay";

LVAdapterDisplay* LVAdapterDisplay::instance_ = nullptr;

namespace {

#pragma pack(push, 1)
struct A2i1Header {
    uint32_t magic;
    uint16_t width;
    uint16_t height;
    uint16_t stride;
    uint16_t reserved;
};
#pragma pack(pop)

bool ParseA2i1(const uint8_t* buf, size_t len, uint16_t* w, uint16_t* h, uint16_t* stride,
               const uint8_t** payload) {
    if (buf == nullptr || len < sizeof(A2i1Header) + 8) {
        return false;
    }
    const auto* hdr = reinterpret_cast<const A2i1Header*>(buf);
    if (hdr->magic != A2UI_I1_MAGIC || hdr->width == 0 || hdr->height == 0 || hdr->stride == 0) {
        return false;
    }
    const size_t need =
        sizeof(A2i1Header) + 8u + static_cast<size_t>(hdr->stride) * static_cast<size_t>(hdr->height);
    if (len < need) {
        return false;
    }
    if (w) {
        *w = hdr->width;
    }
    if (h) {
        *h = hdr->height;
    }
    if (stride) {
        *stride = hdr->stride;
    }
    if (payload) {
        *payload = buf + sizeof(A2i1Header); /* palette (8) + bitmap */
    }
    return true;
}

// 优先用 SD 上由 NVS 指向的壁纸；调用方负责用 heap_caps_free 释放。
bool TryLoadShutdownA2i1FromSd(uint8_t** out_buf, size_t* out_len) {
    if (out_buf == nullptr || out_len == nullptr) {
        return false;
    }
    *out_buf = nullptr;
    *out_len = 0;

    if (!SdCardManager::GetInstance().IsMounted()) {
        ESP_LOGI(TAG, "shutdown img: SD not mounted → built-in");
        return false;
    }

    char path[192] = {};
    if (!wallpaper::TryResolveActiveWallpaperPath(path, sizeof(path))) {
        ESP_LOGI(TAG, "shutdown img: no NVS shutdown file → built-in");
        return false;
    }

    struct stat st {};
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        ESP_LOGI(TAG, "shutdown img: missing %s → built-in", path);
        return false;
    }
    // A2I1 头至少 magic+wh+stride+res+palette = 20
    constexpr off_t kMinBytes = 20;
    constexpr off_t kMaxBytes = 256 * 1024;
    if (st.st_size < kMinBytes || st.st_size > kMaxBytes) {
        ESP_LOGW(TAG, "shutdown img: bad size %ld → built-in", static_cast<long>(st.st_size));
        return false;
    }

    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        ESP_LOGW(TAG, "shutdown img: fopen %s fail errno=%d → built-in", path, errno);
        return false;
    }

    const size_t need = static_cast<size_t>(st.st_size);
    uint8_t* buf = static_cast<uint8_t*>(
        heap_caps_malloc(need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (buf == nullptr) {
        buf = static_cast<uint8_t*>(heap_caps_malloc(need, MALLOC_CAP_8BIT));
    }
    if (buf == nullptr) {
        fclose(f);
        ESP_LOGW(TAG, "shutdown img: alloc %u fail → built-in", static_cast<unsigned>(need));
        return false;
    }

    const size_t nread = fread(buf, 1, need, f);
    fclose(f);
    if (nread != need) {
        heap_caps_free(buf);
        ESP_LOGW(TAG, "shutdown img: short read %u/%u → built-in", static_cast<unsigned>(nread),
                 static_cast<unsigned>(need));
        return false;
    }

    // 快速魔数校验，坏文件留给内置，避免关机白屏/花屏
    if (!(need >= 4 && buf[0] == 'A' && buf[1] == '2' && buf[2] == 'I' && buf[3] == '1')) {
        heap_caps_free(buf);
        ESP_LOGW(TAG, "shutdown img: not A2I1 %s → built-in", path);
        return false;
    }

    ESP_LOGI(TAG, "shutdown img: use SD %s (%u bytes)", path, static_cast<unsigned>(need));
    *out_buf = buf;
    *out_len = need;
    return true;
}

constexpr size_t kMaxTouchVirtualKeys = 8;
TouchVirtualKey s_vk_keys[kMaxTouchVirtualKeys];
size_t s_vk_count = 0;
TouchVirtualKeyEventCb s_vk_cb = nullptr;
void* s_vk_user = nullptr;

// long_press_ms==0：按下即 Click（旧行为），仅用 was_down 防抖。
bool s_vk_instant_was_down = false;
bool s_finger_was_down = false;

// long_press_ms>0：完整按住生命周期（无堆分配，无定时器句柄）。
struct TouchVkHoldState {
    const char* name = nullptr;
    uint16_t long_press_ms = 0;
    int64_t down_us = 0;
    bool long_emitted = false;
    bool long_consumed = false;
};
TouchVkHoldState s_vk_hold{};

// 盖板键与 LVGL 刷屏并行：feed 读 I2C + 发虚拟键；屏内点走边沿锁存再喂 LVGL。
// 1=打印 feed latch / indev deliver 详细轨迹；量产保持 0。
#ifndef LV_ADAPTER_TOUCH_TRACE
#define LV_ADAPTER_TOUCH_TRACE 0
#endif
constexpr int kTouchFeedPeriodMs = 10;
constexpr UBaseType_t kTouchFeedPriority = 5; // > LVGL(1)
constexpr uint8_t kTouchEdgeQueueDepth = 50; // LVGL 忙时 down/up 边沿积压上限
struct TouchDownEdge {
    uint16_t x = 0;
    uint16_t y = 0;
};
struct TouchFeedSnapshot {
    uint8_t live_count = 0;     // 当前是否按着（UI，非盖板键）
    uint16_t live_x = 0;
    uint16_t live_y = 0;
    TouchDownEdge down_q[kTouchEdgeQueueDepth]{}; // 按下边沿坐标环（按次交付，勿覆盖）
    uint8_t down_head = 0;      // 下一笔待交付
    uint8_t pending_down = 0;   // 未交付给 LVGL 的按下边沿
    uint8_t pending_up = 0;     // 未交付给 LVGL 的抬起边沿
    bool lvgl_down = false;     // LVGL 侧是否已处于 pressed
    bool suppress_lvgl = false; // 盖板键占用中
};
portMUX_TYPE s_touch_snap_mux = portMUX_INITIALIZER_UNLOCKED;
TouchFeedSnapshot s_touch_snap{};
TaskHandle_t s_touch_feed_task = nullptr;
volatile bool s_touch_feed_run = false;

const TouchVirtualKey* HitVirtualKey(int x, int y) {
    for (size_t i = 0; i < s_vk_count; ++i) {
        if (x == s_vk_keys[i].x && y == s_vk_keys[i].y) {
            return &s_vk_keys[i];
        }
    }
    return nullptr;
}

bool EmitVkEvent(const char* name, TouchVkEvent event) {
    if (s_vk_cb == nullptr || name == nullptr) {
        return false;
    }
    // 与 BOOT 键同：可在非 LVGL 任务调用；页面跳转内部须 ScreenLvAsync，勿在此同步碰 LVGL。
    const int64_t t0 = esp_timer_get_time();
    const bool r = s_vk_cb(name, event, s_vk_user);
    const int us = static_cast<int>(esp_timer_get_time() - t0);
    if (event == TouchVkEvent::Click || event == TouchVkEvent::LongPress) {
        ESP_LOGI(TAG, "vk cb %s ev=%d ret=%d took %d us", name, static_cast<int>(event),
                 r ? 1 : 0, us);
        if (us > 20000) {
            ESP_LOGW(TAG, "vk cb %s blocked touch_feed %d us", name, us);
        }
    }
    return r;
}

void EndHoldPress(bool emit_click_if_unconsumed) {
    if (s_vk_hold.name == nullptr) {
        return;
    }
    const char* name = s_vk_hold.name;
    const bool emit_click = emit_click_if_unconsumed && !s_vk_hold.long_consumed;
    EmitVkEvent(name, TouchVkEvent::PressUp);
    if (emit_click) {
        // 可长按键短按：按下未震，松手 Click 时补震
        HapticPulseIfEnabled();
        EmitVkEvent(name, TouchVkEvent::Click);
    }
    s_vk_hold = {};
}

/** 结束一次 UI 按下（抬起或改点到盖板键）：锁存 up，避免 LVGL 忙碌时丢掉松手。 */
bool TouchFeedUiReleaseLocked() {
    if (s_touch_snap.live_count == 0) {
        return false;
    }
    s_touch_snap.live_count = 0;
    if (s_touch_snap.pending_up < kTouchEdgeQueueDepth) {
        s_touch_snap.pending_up++;
    }
    return true;
}

bool TouchFeedUiPressLocked(uint16_t x, uint16_t y) {
    s_touch_snap.live_count = 1;
    s_touch_snap.live_x = x;
    s_touch_snap.live_y = y;
    if (s_touch_snap.pending_down < kTouchEdgeQueueDepth) {
        const uint8_t i =
            static_cast<uint8_t>((s_touch_snap.down_head + s_touch_snap.pending_down) % kTouchEdgeQueueDepth);
        s_touch_snap.down_q[i].x = x;
        s_touch_snap.down_q[i].y = y;
        s_touch_snap.pending_down++;
    }
    return true;
}

void TouchFeedKickLvgl(const char* why, uint8_t pend_d, uint8_t pend_u, uint8_t live, bool lvgl_d,
                       uint16_t x, uint16_t y) {
#if LV_ADAPTER_TOUCH_TRACE
    ESP_LOGI(TAG, "feed latch %s pend_d=%u pend_u=%u live=%u lvgl_d=%d xy=%u,%u → kick indev", why,
             pend_d, pend_u, live, lvgl_d ? 1 : 0, x, y);
#else
    (void)why;
    (void)pend_d;
    (void)pend_u;
    (void)live;
    (void)lvgl_d;
    (void)x;
    (void)y;
#endif
    if (auto* disp = LVAdapterDisplay::Instance()) {
        disp->KickTouchInput();
    }
}

/** touch_feed：按下边沿早震；盖板键逻辑；屏内锁存 down/up 边沿供 LVGL 补齐 CLICKED。 */
void TouchFeedHandleSample(uint8_t count, int x, int y) {
    if (count == 0) {
        if (s_finger_was_down) {
            ESP_LOGI(TAG, "touch up");
        }
        s_finger_was_down = false;
        s_vk_instant_was_down = false;
        EndHoldPress(true);
        bool kick = false;
        uint8_t pend_d = 0, pend_u = 0, live = 0;
        bool lvgl_d = false;
        uint16_t lx = 0, ly = 0;
        portENTER_CRITICAL(&s_touch_snap_mux);
        s_touch_snap.suppress_lvgl = false;
        kick = TouchFeedUiReleaseLocked();
        pend_d = s_touch_snap.pending_down;
        pend_u = s_touch_snap.pending_up;
        live = s_touch_snap.live_count;
        lvgl_d = s_touch_snap.lvgl_down;
        lx = s_touch_snap.live_x;
        ly = s_touch_snap.live_y;
        portEXIT_CRITICAL(&s_touch_snap_mux);
        if (kick) {
            TouchFeedKickLvgl("ui_up", pend_d, pend_u, live, lvgl_d, lx, ly);
        }
        return;
    }

    PowerPolicy::GetInstance().NotifyUserActivity();

    if (!s_finger_was_down) {
        s_finger_was_down = true;
        const TouchVirtualKey* early_hit = HitVirtualKey(x, y);
        ESP_LOGI(TAG, "touch down x=%d y=%d %s", x, y,
                 early_hit != nullptr ? early_hit->name : "ui");
        if (early_hit != nullptr) {
            HapticBeginPress();
            // 可长按键：按下不震，短按 Click / 长按达成时再震（避免连震两下）
            if (early_hit->long_press_ms == 0) {
                HapticPulseOnFingerDown();
            }
        } else {
            HapticBeginPress();
            HapticTryPulseAtUiPoint(static_cast<int16_t>(x), static_cast<int16_t>(y));
        }
    }

    const TouchVirtualKey* hit = HitVirtualKey(x, y);
    if (hit == nullptr) {
        s_vk_instant_was_down = false;
        EndHoldPress(true);
        const uint16_t ux = static_cast<uint16_t>(x);
        const uint16_t uy = static_cast<uint16_t>(y);
        bool kick = false;
        uint8_t pend_d = 0, pend_u = 0, live = 0;
        bool lvgl_d = false;
        portENTER_CRITICAL(&s_touch_snap_mux);
        s_touch_snap.suppress_lvgl = false;
        if (s_touch_snap.live_count == 0) {
            kick = TouchFeedUiPressLocked(ux, uy);
        } else {
            s_touch_snap.live_x = ux;
            s_touch_snap.live_y = uy;
        }
        pend_d = s_touch_snap.pending_down;
        pend_u = s_touch_snap.pending_up;
        live = s_touch_snap.live_count;
        lvgl_d = s_touch_snap.lvgl_down;
        portEXIT_CRITICAL(&s_touch_snap_mux);
        if (kick) {
            TouchFeedKickLvgl("ui_down", pend_d, pend_u, live, lvgl_d, ux, uy);
        }
        return;
    }

    // 盖板键：不喂 LVGL；若刚才在点 UI，补一次抬起边沿
    bool kick = false;
    uint8_t pend_d = 0, pend_u = 0, live = 0;
    bool lvgl_d = false;
    uint16_t lx = 0, ly = 0;
    portENTER_CRITICAL(&s_touch_snap_mux);
    s_touch_snap.suppress_lvgl = true;
    kick = TouchFeedUiReleaseLocked();
    pend_d = s_touch_snap.pending_down;
    pend_u = s_touch_snap.pending_up;
    live = s_touch_snap.live_count;
    lvgl_d = s_touch_snap.lvgl_down;
    lx = s_touch_snap.live_x;
    ly = s_touch_snap.live_y;
    portEXIT_CRITICAL(&s_touch_snap_mux);
    if (kick) {
        TouchFeedKickLvgl("ui_to_vk", pend_d, pend_u, live, lvgl_d, lx, ly);
    }

    if (hit->long_press_ms == 0) {
        EndHoldPress(true);
        if (!s_vk_instant_was_down) {
            s_vk_instant_was_down = true;
            ESP_LOGI(TAG, "virtual key [%s] click", hit->name);
            EmitVkEvent(hit->name, TouchVkEvent::Click);
        }
        return;
    }

    s_vk_instant_was_down = false;

    if (s_vk_hold.name != nullptr && std::strcmp(s_vk_hold.name, hit->name) != 0) {
        EndHoldPress(true);
    }

    if (s_vk_hold.name == nullptr) {
        s_vk_hold.name = hit->name;
        s_vk_hold.long_press_ms = hit->long_press_ms;
        s_vk_hold.down_us = esp_timer_get_time();
        s_vk_hold.long_emitted = false;
        s_vk_hold.long_consumed = false;
        ESP_LOGI(TAG, "virtual key [%s] press-down (long=%ums)", hit->name,
                 static_cast<unsigned>(hit->long_press_ms));
        return;
    }

    if (!s_vk_hold.long_emitted) {
        const int64_t elapsed_ms = (esp_timer_get_time() - s_vk_hold.down_us) / 1000;
        if (elapsed_ms >= static_cast<int64_t>(s_vk_hold.long_press_ms)) {
            s_vk_hold.long_emitted = true;
            ESP_LOGI(TAG, "virtual key [%s] long-press %ums", s_vk_hold.name,
                     static_cast<unsigned>(s_vk_hold.long_press_ms));
            s_vk_hold.long_consumed = EmitVkEvent(s_vk_hold.name, TouchVkEvent::LongPress);
        }
    }
}

void TouchFeedTask(void* arg) {
    auto* tp = static_cast<esp_lcd_touch_handle_t>(arg);
    esp_lcd_touch_point_data_t pts[1] = {};
    ESP_LOGI(TAG, "touch_feed start prio=%u period=%dms", static_cast<unsigned>(kTouchFeedPriority),
             kTouchFeedPeriodMs);
    while (s_touch_feed_run) {
        uint8_t count = 0;
        // 待机 LP 已写 A5 深睡：勿再 I2C 读（CST816S 不应答 → panel_io 刷 E 级日志）
        if (tp != nullptr && !PowerPolicy::GetInstance().IsTouchAsleep()) {
            if (esp_lcd_touch_read_data(tp) == ESP_OK) {
                if (esp_lcd_touch_get_data(tp, pts, &count, 1) != ESP_OK) {
                    count = 0;
                }
            }
        }
        const int x = count > 0 ? static_cast<int>(pts[0].x) : 0;
        const int y = count > 0 ? static_cast<int>(pts[0].y) : 0;
        TouchFeedHandleSample(count, x, y);
        vTaskDelay(pdMS_TO_TICKS(kTouchFeedPeriodMs));
    }
    s_touch_feed_task = nullptr;
    vTaskDelete(nullptr);
}

void StartTouchFeed(esp_lcd_touch_handle_t tp) {
    if (tp == nullptr || s_touch_feed_task != nullptr) {
        return;
    }
    s_touch_feed_run = true;
    BaseType_t ok = xTaskCreatePinnedToCore(TouchFeedTask, "touch_feed", 4096, tp,
                                             kTouchFeedPriority, &s_touch_feed_task, 0);
    if (ok != pdPASS) {
        s_touch_feed_run = false;
        s_touch_feed_task = nullptr;
        ESP_LOGE(TAG, "touch_feed create failed");
    }
}

void StopTouchFeed() {
    if (s_touch_feed_task == nullptr) {
        return;
    }
    s_touch_feed_run = false;
    for (int i = 0; i < 50 && s_touch_feed_task != nullptr; ++i) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * LVGL indev：按边沿交付，而不是只读「此刻快照」。
 * 否则 LVGL 忙时整段 down→up 被折叠成一次 up，出现「第一次只震不点、第二次才切页」。
 */
esp_err_t CustomTouchRead(esp_lcd_touch_handle_t tp, esp_lcd_touch_point_data_t* points, uint8_t* count,
                          uint8_t max_count, void* /*user_ctx*/) {
    (void)tp;
    if (points == nullptr || count == nullptr || max_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t out_count = 0;
    uint16_t out_x = 0;
    uint16_t out_y = 0;
    const char* why = "idle";
    uint8_t pend_d = 0, pend_u = 0, live = 0;
    bool lvgl_d_before = false;

    portENTER_CRITICAL(&s_touch_snap_mux);
    auto& s = s_touch_snap;
    pend_d = s.pending_down;
    pend_u = s.pending_up;
    live = s.live_count;
    lvgl_d_before = s.lvgl_down;
    if (s.lvgl_down) {
        if (s.pending_up > 0) {
            s.pending_up--;
            s.lvgl_down = false;
            out_count = 0;
            why = "deliver_up(pend)";
        } else if (s.live_count > 0 && !s.suppress_lvgl) {
            out_count = 1;
            out_x = s.live_x;
            out_y = s.live_y;
            why = "hold";
        } else {
            // 已松开但边沿被吞：补一次 up，避免粘住 PRESSED
            s.lvgl_down = false;
            out_count = 0;
            why = "deliver_up(force)";
        }
    } else if (s.pending_down > 0) {
        out_x = s.down_q[s.down_head].x;
        out_y = s.down_q[s.down_head].y;
        s.down_head = static_cast<uint8_t>((s.down_head + 1) % kTouchEdgeQueueDepth);
        s.pending_down--;
        s.lvgl_down = true;
        out_count = 1;
        why = "deliver_down(pend)";
    } else if (s.live_count > 0 && !s.suppress_lvgl) {
        s.lvgl_down = true;
        out_count = 1;
        out_x = s.live_x;
        out_y = s.live_y;
        why = "deliver_down(live)";
    } else {
        out_count = 0;
        why = "idle";
    }
    const uint8_t pend_d_after = s.pending_down;
    const uint8_t pend_u_after = s.pending_up;
    const bool lvgl_d_after = s.lvgl_down;
    // down 已交付但 up 仍锁存：必须再踢一次 indev，否则 LVGL 可能不再读，CLICKED 永不到
    const bool kick_again = (out_count == 1 && pend_u_after > 0) || (out_count == 0 && pend_d_after > 0);
    portEXIT_CRITICAL(&s_touch_snap_mux);

    // 只打边沿交付，避免 hold/idle 刷屏；对照 feed latch 时间戳即可定位
#if LV_ADAPTER_TOUCH_TRACE
    if (why[0] == 'd') { // deliver_*
        ESP_LOGI(TAG,
                 "indev %s out=%u xy=%u,%u before(pend_d=%u pend_u=%u live=%u lvgl_d=%d) "
                 "after(pend_d=%u pend_u=%u lvgl_d=%d)%s",
                 why, out_count, out_x, out_y, pend_d, pend_u, live, lvgl_d_before ? 1 : 0, pend_d_after,
                 pend_u_after, lvgl_d_after ? 1 : 0, kick_again ? " → kick again" : "");
    }
#else
    (void)why;
    (void)pend_d;
    (void)pend_u;
    (void)live;
    (void)lvgl_d_before;
    (void)pend_d_after;
    (void)pend_u_after;
    (void)lvgl_d_after;
#endif

    if (kick_again) {
        if (auto* disp = LVAdapterDisplay::Instance()) {
            disp->KickTouchInput();
        }
    }

    if (out_count == 0) {
        *count = 0;
        return ESP_OK;
    }
    *count = 1;
    points[0] = {};
    points[0].x = out_x;
    points[0].y = out_y;
    return ESP_OK;
}

}  // namespace

/*
 * LVGL I1 / HTILED：1=白、0=黑，与面板 VRAM（0xFF=白）一致，写屏不取反。
 * 全刷：fast LUT（仅全↔局切换时下发）→ 0x26=0x24=新图 → 0x22=0xC7 → 0x20 → BUSY → 0x3F/0x01
 * 局刷：partial LUT → 边框0x80 → 整帧只写 0x24=新图（不写 0x26、不做 BUSY 后 Sync）→ 0x22=0xCF → 0x20
 * 每 10 次局刷强制一次全刷；开机/关机走全刷。
 *
 * 触摸：PARTIAL 启动后立刻 flush_ready；下次写屏用 TryFinish（不等 BUSY），忙则 coalesce。
 * coalesce 后若无后续 flush（如打开书进度条→正文），BUSY 结束后由 esp_timer 补刷 work_fb。
 * 盖板虚拟键：touch_feed 并行采点/震动/发事件（跳转内部 lv_async）。
 * 屏内：无锁对照 HapticAttachClick 区域表早震；feed 锁存 down/up 供 indev。
 */
static constexpr uint8_t kLvglWhite = 0xFF;
static constexpr uint8_t kLvglBlack = 0x00;
static constexpr int kPartialDirtyPadPx = 24;
/** 连续局刷达到该次数后，下一次改走全刷清残影 */
static constexpr uint32_t kPartialsPerFull = 15;

struct EpdFlushCtx {
    lv_display_t* disp = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;
    SemaphoreHandle_t done_sem = nullptr;
    uint8_t* last_fb = nullptr;
    uint8_t* work_fb = nullptr;
    size_t fb_size = 0;
    int panel_w = 0;
    int panel_h = 0;
    bool has_last = false;
    uint32_t partial_refresh_count = 0;
    bool defer_boot_paint = true;  // SetupUI 完成前只攒帧不上屏
    bool boot_fb_ready = false;
    bool defer_standby_paint = false;     // 进待机 settle：只攒帧，Park 再全刷
    volatile bool suppress_lvgl_notify = false;
    volatile bool freeze_updates = false; // 关机画已落墨 / 待机 park：丢弃后续 LVGL flush
    bool standby_frozen = false;          // 待机冻结 flush（无 Deep Sleep）
    bool force_next_full = false;         // 下一帧强制全刷（退待机后画回底层）
    bool force_full_c7 = false;           // 强制全刷用 0xC7（进/出待机）；否则 0xC4
    /**
     * 局刷已启动、BUSY 尚未结束：flush 已提前 flush_ready，LVGL 可继续读触摸。
     * 下次写屏前 Drain（不等 Sync）。
     */
    bool refresh_inflight = false;
    bool coalesce_pending = false; // BUSY 时合并脏帧，flush 立刻返回好让 indev 跑
    bool coalesce_kick_armed = false; // 已挂 esp_timer 等待 BUSY 后补刷 coalesce
    esp_timer_handle_t coalesce_timer = nullptr;
};

namespace {

void EpdAreaRounder(lv_area_t* area, void* user_data) {
    (void)user_data;
    // 扩 invalidate：让 LVGL 把旧字/AA 边缘也重绘成白，局刷 CURR 才有黑→白差分
    area->x1 -= kPartialDirtyPadPx;
    area->y1 -= kPartialDirtyPadPx / 2;
    area->x2 += kPartialDirtyPadPx;
    area->y2 += kPartialDirtyPadPx / 2;
    if (area->x1 < 0) {
        area->x1 = 0;
    }
    if (area->y1 < 0) {
        area->y1 = 0;
    }
    area->x1 = (area->x1 >> 3) << 3;
    area->x2 = ((area->x2 >> 3) << 3) + 7;
}

bool EpdRefreshDoneCb(const esp_lcd_panel_handle_t handle, const void* edata, void* user_data) {
    (void)handle;
    (void)edata;
    auto* ctx = static_cast<EpdFlushCtx*>(user_data);
    if (!ctx) {
        return false;
    }

    if (ctx->suppress_lvgl_notify) {
        BaseType_t hp_task = pdFALSE;
        if (ctx->done_sem) {
            xSemaphoreGiveFromISR(ctx->done_sem, &hp_task);
        }
        if (hp_task == pdTRUE) {
            portYIELD_FROM_ISR();
        }
        return false;
    }

    return esp_lv_adapter_display_notify_frame_done_from_isr(ctx->disp);
}

static void AlignArea(int x_start, int y_start, int x_end, int y_end, int* x_draw, int* y_draw,
                      int* w_draw, int* h_draw) {
    const int len_x = x_end - x_start;
    const int len_y = y_end - y_start;
    *x_draw = x_start - (x_start % 8);
    *y_draw = y_start;
    *w_draw = ((len_x + 7) / 8) * 8;
    *h_draw = len_y;
}

// 扩脏区：边距盖住 AA/旧字边缘；再按 last⊕work 把真正变过的字节并进去
static void ExpandPartialDirty(EpdFlushCtx* ctx, int* x_draw, int* y_draw, int* w_draw, int* h_draw) {
    int x0 = *x_draw - kPartialDirtyPadPx;
    int y0 = *y_draw - kPartialDirtyPadPx;
    int x1 = *x_draw + *w_draw + kPartialDirtyPadPx;
    int y1 = *y_draw + *h_draw + kPartialDirtyPadPx;
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > ctx->panel_w) {
        x1 = ctx->panel_w;
    }
    if (y1 > ctx->panel_h) {
        y1 = ctx->panel_h;
    }
    x0 = (x0 >> 3) << 3;
    x1 = ((x1 + 7) >> 3) << 3;
    if (x1 > ctx->panel_w) {
        x1 = ctx->panel_w;
    }

    const int fb_stride = ctx->panel_w / 8;
    int ch_x0 = x1;
    int ch_x1 = x0;
    int ch_y0 = y1;
    int ch_y1 = y0;
    bool any = false;
    for (int y = y0; y < y1; y++) {
        const uint8_t* a = ctx->last_fb + y * fb_stride + (x0 / 8);
        const uint8_t* b = ctx->work_fb + y * fb_stride + (x0 / 8);
        const int nbytes = (x1 - x0) / 8;
        for (int i = 0; i < nbytes; i++) {
            if (a[i] == b[i]) {
                continue;
            }
            any = true;
            const int bx0 = x0 + i * 8;
            const int bx1 = bx0 + 8;
            if (bx0 < ch_x0) {
                ch_x0 = bx0;
            }
            if (bx1 > ch_x1) {
                ch_x1 = bx1;
            }
            if (y < ch_y0) {
                ch_y0 = y;
            }
            if (y + 1 > ch_y1) {
                ch_y1 = y + 1;
            }
        }
    }

    if (!any) {
        *w_draw = 0;
        *h_draw = 0;
        return;
    }

    // 变化核再扩一圈，避免字缘灰度抖到脏区外
    ch_x0 -= kPartialDirtyPadPx;
    ch_y0 -= kPartialDirtyPadPx / 2;
    ch_x1 += kPartialDirtyPadPx;
    ch_y1 += kPartialDirtyPadPx / 2;
    if (ch_x0 < 0) {
        ch_x0 = 0;
    }
    if (ch_y0 < 0) {
        ch_y0 = 0;
    }
    if (ch_x1 > ctx->panel_w) {
        ch_x1 = ctx->panel_w;
    }
    if (ch_y1 > ctx->panel_h) {
        ch_y1 = ctx->panel_h;
    }
    ch_x0 = (ch_x0 >> 3) << 3;
    ch_x1 = ((ch_x1 + 7) >> 3) << 3;
    if (ch_x1 > ctx->panel_w) {
        ch_x1 = ctx->panel_w;
    }

    *x_draw = ch_x0;
    *y_draw = ch_y0;
    *w_draw = ch_x1 - ch_x0;
    *h_draw = ch_y1 - ch_y0;
}

static void OverlayDirty(uint8_t* dst, const uint8_t* color_map, int panel_w, int x_draw,
                         int y_draw, int w_draw, int h_draw) {
    const int fb_stride = panel_w / 8;
    const int row_bytes = w_draw / 8;
    for (int row = 0; row < h_draw; row++) {
        const int y = y_draw + row;
        memcpy(dst + y * fb_stride + x_draw / 8, color_map + y * fb_stride + x_draw / 8, row_bytes);
    }
}

static esp_err_t WaitRefreshDone(EpdFlushCtx* ctx, const char* tag) {
    if (xSemaphoreTake(ctx->done_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGW(TAG, "%s timeout", tag);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/** 局刷：prepare → 整帧只写 0x24=新图（不写 0x26）→ 0xCF。 */
static esp_err_t DrawPartialDirty(EpdFlushCtx* ctx, esp_lcd_panel_handle_t panel, const uint8_t* prev_fb,
                                  const uint8_t* curr_fb, int x_draw, int y_draw, int w_draw,
                                  int h_draw) {
    (void)prev_fb;
    (void)x_draw;
    (void)y_draw;
    (void)w_draw;
    (void)h_draw;
    if (ctx == nullptr || panel == nullptr || curr_fb == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // curr_fb 在 PSRAM：draw_bitmap 内再拷到内部 DMA bounce，勿再经 tx_* 中转
    ESP_RETURN_ON_ERROR(epaper_panel_prepare_for_partial(panel), TAG, "partial prepare");
    epaper_panel_set_bitmap_color(panel, SSD1677_EPAPER_BITMAP_CURRENT);
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_draw_bitmap(panel, 0, 0, ctx->panel_w, ctx->panel_h, curr_fb), TAG,
        "partial curr 0x24");
    epaper_panel_set_refresh_mode(panel, SSD1677_EPAPER_REFRESH_PARTIAL);
    return epaper_panel_refresh_screen(panel);
}

static esp_err_t DrawFullScreenPartial(EpdFlushCtx* ctx, esp_lcd_panel_handle_t panel,
                                       const uint8_t* prev_fb, const uint8_t* curr_fb) {
    return DrawPartialDirty(ctx, panel, prev_fb, curr_fb, 0, 0, ctx->panel_w, ctx->panel_h);
}

/** 全刷：0x26/0x24 同写新图；mode 选 FULL(0xC4) / STANDBY(0xC7，进/出待机与关机)。 */
static esp_err_t FullRefreshBothSame(EpdFlushCtx* ctx, const uint8_t* curr_fb, const char* wait_tag,
                                     esp_lcd_ssd1677_refresh_mode_t mode = SSD1677_EPAPER_REFRESH_FULL) {
    if (ctx == nullptr || ctx->panel == nullptr || curr_fb == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    // 同帧写 PREV/CURRENT；每次 draw_bitmap 自拷 bounce，二次调用前会排空 SPI
    epaper_panel_set_bitmap_color(ctx->panel, SSD1677_EPAPER_BITMAP_PREVIOUS);
    esp_err_t err =
        esp_lcd_panel_draw_bitmap(ctx->panel, 0, 0, ctx->panel_w, ctx->panel_h, curr_fb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s 0x26: %s", wait_tag, esp_err_to_name(err));
        return err;
    }
    epaper_panel_set_bitmap_color(ctx->panel, SSD1677_EPAPER_BITMAP_CURRENT);
    err = esp_lcd_panel_draw_bitmap(ctx->panel, 0, 0, ctx->panel_w, ctx->panel_h, curr_fb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s 0x24: %s", wait_tag, esp_err_to_name(err));
        return err;
    }
    epaper_panel_set_refresh_mode(ctx->panel, mode);
    // FULL/STANDBY 在 panel 内同步等 BUSY，不再 WaitRefreshDone（下降沿）
    err = epaper_panel_refresh_screen(ctx->panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s refresh: %s", wait_tag, esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

static esp_err_t FullScreenPartialTo(EpdFlushCtx* ctx, const uint8_t* prev_fb, const uint8_t* curr_fb,
                                     const char* wait_tag)
{
    esp_err_t err = DrawFullScreenPartial(ctx, ctx->panel, prev_fb, curr_fb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: %s", wait_tag, esp_err_to_name(err));
        return err;
    }
    return WaitRefreshDone(ctx, wait_tag);
}

/** 排空上一次异步局刷（等 BUSY，不做 Sync）。 */
static void DrainRefreshIfNeeded(EpdFlushCtx* ctx) {
    if (ctx == nullptr || !ctx->refresh_inflight) {
        return;
    }
    (void)WaitRefreshDone(ctx, "drain");
    ctx->refresh_inflight = false;
    ctx->suppress_lvgl_notify = false;
}

/** 非阻塞：BUSY 未结束返回 false（调用方 coalesce + flush_ready，勿堵 indev）。 */
static bool TryFinishInflightRefresh(EpdFlushCtx* ctx) {
    if (ctx == nullptr || !ctx->refresh_inflight) {
        return true;
    }
    if (xSemaphoreTake(ctx->done_sem, 0) != pdTRUE) {
        return false;
    }
    ctx->refresh_inflight = false;
    ctx->suppress_lvgl_notify = false;
    return true;
}

static void ArmCoalesceKick(EpdFlushCtx* ctx);

/** BUSY 结束后把 coalesce 的 work_fb 局刷上屏（无新 LVGL flush 时补一刀）。 */
static void CommitCoalescedFrame(EpdFlushCtx* ctx) {
    if (ctx == nullptr || !ctx->coalesce_pending || ctx->freeze_updates || ctx->defer_standby_paint) {
        return;
    }
    if (!TryFinishInflightRefresh(ctx)) {
        ArmCoalesceKick(ctx);
        return;
    }
    if (!ctx->has_last || ctx->work_fb == nullptr || ctx->last_fb == nullptr || ctx->panel == nullptr) {
        ctx->coalesce_pending = false;
        return;
    }

    ctx->suppress_lvgl_notify = true;
    xSemaphoreTake(ctx->done_sem, 0);
    const esp_err_t err =
        DrawPartialDirty(ctx, ctx->panel, ctx->last_fb, ctx->work_fb, 0, 0, ctx->panel_w, ctx->panel_h);
    if (err != ESP_OK) {
        ctx->suppress_lvgl_notify = false;
        ctx->coalesce_pending = false;
        ESP_LOGW(TAG, "coalesce commit: %s", esp_err_to_name(err));
        return;
    }

    ctx->refresh_inflight = true;
    ctx->partial_refresh_count++;
    memcpy(ctx->last_fb, ctx->work_fb, ctx->fb_size);
    ctx->coalesce_pending = false;
    ESP_LOGI(TAG, "coalesce commit after busy");
}

static void CoalesceKickAsync(void* user_data) {
    auto* ctx = static_cast<EpdFlushCtx*>(user_data);
    if (ctx == nullptr) {
        return;
    }
    ctx->coalesce_kick_armed = false;
    // 已在 LVGL 线程（lv_async）；勿再 DisplayLock，避免与 adapter 锁重入死锁
    CommitCoalescedFrame(ctx);
}

static void CoalesceEspTimerCb(void* arg) {
    // 勿在 timer 任务里直接碰 panel/LVGL：投递到 LVGL 线程
    if (lv_async_call(CoalesceKickAsync, arg) != LV_RESULT_OK) {
        auto* ctx = static_cast<EpdFlushCtx*>(arg);
        if (ctx != nullptr) {
            ctx->coalesce_kick_armed = false;
        }
    }
}

static void ArmCoalesceKick(EpdFlushCtx* ctx) {
    if (ctx == nullptr || ctx->coalesce_timer == nullptr || ctx->freeze_updates ||
        ctx->defer_standby_paint) {
        return;
    }
    if (!ctx->coalesce_pending) {
        return;
    }
    // 已在跑则等回调；回调里若仍 BUSY 会再 arm
    if (esp_timer_is_active(ctx->coalesce_timer)) {
        ctx->coalesce_kick_armed = true;
        return;
    }
    ctx->coalesce_kick_armed = true;
    if (esp_timer_start_once(ctx->coalesce_timer, 50000) != ESP_OK) { // 50ms
        ctx->coalesce_kick_armed = false;
    }
}

/** A2I1 逻辑竖屏 → 面板 HTILED（与 adapter ROTATE_270：px=ly, py=ph-1-lx）。 */
void BlitA2i1ToPanelFb(uint8_t* dst_fb, int panel_w, int panel_h, const uint8_t* a2i1_payload,
                       uint16_t log_w, uint16_t log_h, uint16_t src_stride) {
    const uint8_t* src = a2i1_payload + 8; /* skip LVGL palette */
    const int dst_stride = panel_w / 8;
    memset(dst_fb, kLvglWhite, static_cast<size_t>(dst_stride) * static_cast<size_t>(panel_h));
    for (int ly = 0; ly < static_cast<int>(log_h); ly++) {
        for (int lx = 0; lx < static_cast<int>(log_w); lx++) {
            const bool white =
                (src[ly * src_stride + (lx >> 3)] & static_cast<uint8_t>(0x80 >> (lx & 7))) != 0;
            const int px = ly;
            const int py = panel_h - 1 - lx;
            if (px < 0 || py < 0 || px >= panel_w || py >= panel_h) {
                continue;
            }
            uint8_t* byte = &dst_fb[py * dst_stride + (px >> 3)];
            const uint8_t mask = static_cast<uint8_t>(0x80 >> (px & 7));
            if (white) {
                *byte |= mask;
            } else {
                *byte = static_cast<uint8_t>(*byte & ~mask);
            }
        }
    }
}

/** 关机：关机图一次全刷（0xC7，与进/出待机相同）。 */
esp_err_t EpdShutdownFullFb(EpdFlushCtx* ctx, const uint8_t* curr_fb) {
    if (ctx == nullptr || ctx->panel == nullptr || ctx->last_fb == nullptr || curr_fb == nullptr ||
        ctx->work_fb == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    DrainRefreshIfNeeded(ctx);
    ctx->suppress_lvgl_notify = true;
    xSemaphoreTake(ctx->done_sem, 0);

    ESP_LOGI(TAG, "shutdown: FULL 0xC7");
    esp_err_t err = FullRefreshBothSame(ctx, curr_fb, "shutdown", SSD1677_EPAPER_REFRESH_STANDBY);
    ESP_LOGI(TAG, "shutdown: done err=%s", esp_err_to_name(err));
    ctx->suppress_lvgl_notify = false;
    if (err == ESP_OK) {
        memcpy(ctx->last_fb, curr_fb, ctx->fb_size);
        ctx->has_last = true;
    }
    return err;
}

esp_err_t EpdLvglDrawBitmap(lv_display_t* disp, esp_lcd_panel_handle_t panel, int x_start,
                            int y_start, int x_end, int y_end, const void* color_map,
                            void* user_ctx) {
    (void)disp;
    auto* ctx = static_cast<EpdFlushCtx*>(user_ctx);
    if (!ctx || !color_map || !ctx->last_fb || !ctx->work_fb) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->freeze_updates) {
        if (ctx->disp) {
            lv_display_flush_ready(ctx->disp);
        }
        return ESP_OK;
    }

    int x_draw = 0, y_draw = 0, w_draw = 0, h_draw = 0;
    AlignArea(x_start, y_start, x_end, y_end, &x_draw, &y_draw, &w_draw, &h_draw);
    if (w_draw <= 0 || h_draw <= 0 || x_draw < 0 || y_draw < 0 ||
        x_draw + w_draw > ctx->panel_w || y_draw + h_draw > ctx->panel_h) {
        ESP_LOGW(TAG, "bad area (%d,%d)-(%d,%d)", x_start, y_start, x_end, y_end);
        return ESP_ERR_INVALID_ARG;
    }

    // 勿在 flush_cb 里 Wait BUSY：否则 LVGL 跑不了 indev，touch_feed 边沿无法交付
    if (!TryFinishInflightRefresh(ctx)) {
        if (!ctx->coalesce_pending) {
            if (ctx->boot_fb_ready || ctx->has_last) {
                memcpy(ctx->work_fb, ctx->last_fb, ctx->fb_size);
            } else {
                memset(ctx->work_fb, kLvglWhite, ctx->fb_size);
            }
        }
        OverlayDirty(ctx->work_fb, static_cast<const uint8_t*>(color_map), ctx->panel_w, x_draw,
                     y_draw, w_draw, h_draw);
        if (ctx->defer_standby_paint) {
            memcpy(ctx->last_fb, ctx->work_fb, ctx->fb_size);
            ctx->coalesce_pending = false;
            ctx->coalesce_kick_armed = false;
            lv_display_flush_ready(ctx->disp);
            return ESP_OK;
        }
        ctx->coalesce_pending = true;
        ArmCoalesceKick(ctx);
        lv_display_flush_ready(ctx->disp);
        return ESP_OK;
    }

    if (ctx->coalesce_pending) {
        OverlayDirty(ctx->work_fb, static_cast<const uint8_t*>(color_map), ctx->panel_w, x_draw,
                     y_draw, w_draw, h_draw);
    } else if (ctx->boot_fb_ready || ctx->has_last) {
        memcpy(ctx->work_fb, ctx->last_fb, ctx->fb_size);
        OverlayDirty(ctx->work_fb, static_cast<const uint8_t*>(color_map), ctx->panel_w, x_draw,
                     y_draw, w_draw, h_draw);
    } else {
        memset(ctx->work_fb, kLvglWhite, ctx->fb_size);
        OverlayDirty(ctx->work_fb, static_cast<const uint8_t*>(color_map), ctx->panel_w, x_draw,
                     y_draw, w_draw, h_draw);
    }

    if (ctx->defer_boot_paint) {
        memcpy(ctx->last_fb, ctx->work_fb, ctx->fb_size);
        ctx->boot_fb_ready = true;
        ctx->coalesce_pending = false;
        ctx->coalesce_kick_armed = false;
        lv_display_flush_ready(ctx->disp);
        return ESP_OK;
    }

    if (ctx->defer_standby_paint) {
        memcpy(ctx->last_fb, ctx->work_fb, ctx->fb_size);
        ctx->coalesce_pending = false;
        ctx->coalesce_kick_armed = false;
        lv_display_flush_ready(ctx->disp);
        return ESP_OK;
    }

    if (!ctx->has_last) {
        ctx->suppress_lvgl_notify = true;
        xSemaphoreTake(ctx->done_sem, 0);
        esp_err_t err = FullRefreshBothSame(ctx, ctx->work_fb, "first_paint");
        ctx->suppress_lvgl_notify = false;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "first paint: %s", esp_err_to_name(err));
            return err;
        }
        memcpy(ctx->last_fb, ctx->work_fb, ctx->fb_size);
        ctx->has_last = true;
        ctx->partial_refresh_count = 0;
        ctx->coalesce_pending = false;
        ctx->coalesce_kick_armed = false;
        lv_display_flush_ready(ctx->disp);
        return ESP_OK;
    }

    // 退待机后首帧（0xC7）/ 每 10 次局刷或其它强制全刷（0xC4）
    if (ctx->force_next_full || ctx->partial_refresh_count >= kPartialsPerFull) {
        const bool from_force = ctx->force_next_full;
        const bool use_c7 = from_force && ctx->force_full_c7;
        ctx->force_next_full = false;
        ctx->force_full_c7 = false;
        ctx->coalesce_pending = false;
        ctx->coalesce_kick_armed = false;
        ctx->suppress_lvgl_notify = true;
        xSemaphoreTake(ctx->done_sem, 0);
        const auto mode = use_c7 ? SSD1677_EPAPER_REFRESH_STANDBY : SSD1677_EPAPER_REFRESH_FULL;
        ESP_LOGI(TAG, "%s FULL 0x22=%02X", from_force ? "forced" : "periodic",
                 use_c7 ? SSD1677_PARAM_DISP_UPDATE_FULL_STANDBY : SSD1677_PARAM_DISP_UPDATE_FULL);
        esp_err_t err = FullRefreshBothSame(ctx, ctx->work_fb,
                                            from_force ? "forced_full" : "periodic_full", mode);
        ctx->suppress_lvgl_notify = false;
        ctx->refresh_inflight = false;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "full: %s", esp_err_to_name(err));
            return err;
        }
        memcpy(ctx->last_fb, ctx->work_fb, ctx->fb_size);
        ctx->partial_refresh_count = 0;
        lv_display_flush_ready(ctx->disp);
        return ESP_OK;
    }

    const bool was_coalesce = ctx->coalesce_pending;
    if (was_coalesce) {
        x_draw = 0;
        y_draw = 0;
        w_draw = ctx->panel_w;
        h_draw = ctx->panel_h;
        ctx->coalesce_pending = false;
        ctx->coalesce_kick_armed = false;
    } else {
        ExpandPartialDirty(ctx, &x_draw, &y_draw, &w_draw, &h_draw);
        if (w_draw <= 0 || h_draw <= 0) {
            lv_display_flush_ready(ctx->disp);
            return ESP_OK;
        }
    }

    ctx->suppress_lvgl_notify = true;
    xSemaphoreTake(ctx->done_sem, 0);

    // 局刷：整帧 0x24=新（不写 0x26）
    ESP_LOGD(TAG, "PARTIAL full-frame 0x24 #%u", ctx->partial_refresh_count + 1);
    {
        const esp_err_t err =
            DrawPartialDirty(ctx, panel, ctx->last_fb, ctx->work_fb, x_draw, y_draw, w_draw, h_draw);
        if (err != ESP_OK) {
            ctx->suppress_lvgl_notify = false;
            ESP_LOGE(TAG, "partial: %s", esp_err_to_name(err));
            return err;
        }
    }

    ctx->refresh_inflight = true;
    ctx->partial_refresh_count++;

    memcpy(ctx->last_fb, ctx->work_fb, ctx->fb_size);
    lv_display_flush_ready(ctx->disp);
    return ESP_OK;
}

/** SetupUI 末尾：首页 FULL（0x24=0x26）；之后局刷只写整帧 0x24。 */
void EpdCommitBootBaseline(EpdFlushCtx* ctx)
{
    if (ctx == nullptr || !ctx->defer_boot_paint) {
        return;
    }
    ctx->defer_boot_paint = false;
    if (!ctx->boot_fb_ready || ctx->last_fb == nullptr || ctx->panel == nullptr) {
        ESP_LOGW(TAG, "boot paint skip (no fb)");
        return;
    }
    ctx->suppress_lvgl_notify = true;
    xSemaphoreTake(ctx->done_sem, 0);
    esp_err_t err = FullRefreshBothSame(ctx, ctx->last_fb, "boot");
    if (err == ESP_OK) {
        ctx->has_last = true;
        ctx->partial_refresh_count = 0;
    } else {
        ESP_LOGE(TAG, "boot paint: %s", esp_err_to_name(err));
    }
    ctx->suppress_lvgl_notify = false;
}

}  // namespace

LVAdapterDisplay::LVAdapterDisplay(const esp_lcd_panel_handle_t panel,
                                   const esp_lcd_panel_io_handle_t panel_io,
                                   const esp_lcd_touch_handle_t touch_handle, const int width,
                                   const int height) {
    instance_ = this;
    width_ = width;
    height_ = height;

    esp_timer_create_args_t notification_timer_args = {
        .callback =
            [](void* arg) {
                auto* display = static_cast<LVAdapterDisplay*>(arg);
                DisplayLockGuard lock(display);
                if (display->notification_label_) {
                    lv_obj_add_flag(display->notification_label_, LV_OBJ_FLAG_HIDDEN);
                }
                if (display->status_label_ && !AssistantScreen::IsPttWaveVisible()) {
                    lv_obj_remove_flag(display->status_label_, LV_OBJ_FLAG_HIDDEN);
                }
            },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "epd_notif_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&notification_timer_args, &notification_timer_));

    epd_flush_ctx_ = new EpdFlushCtx();
    epd_flush_ctx_->panel = panel;
    epd_flush_ctx_->panel_w = SSD1677_PANEL_WIDTH;
    epd_flush_ctx_->panel_h = SSD1677_PANEL_HEIGHT;
    epd_flush_ctx_->fb_size = SSD1677_PANEL_BUFFER_SIZE;
    epd_flush_ctx_->done_sem = xSemaphoreCreateBinary();

    {
        esp_timer_create_args_t coalesce_args = {
            .callback = CoalesceEspTimerCb,
            .arg = epd_flush_ctx_,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "epd_coalesce",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&coalesce_args, &epd_flush_ctx_->coalesce_timer));
    }
    // last/work 在 PSRAM（仅 CPU 读写）。发屏由面板侧拷到内部 DMA bounce，
    // 避免 PSRAM 直 DMA 的 cache 不一致与鬼影。内部只常驻 1×48KB bounce。
    const uint32_t psram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    epd_flush_ctx_->last_fb =
        static_cast<uint8_t*>(heap_caps_malloc(epd_flush_ctx_->fb_size, psram));
    epd_flush_ctx_->work_fb =
        static_cast<uint8_t*>(heap_caps_malloc(epd_flush_ctx_->fb_size, psram));

    if (epd_flush_ctx_->last_fb) {
        memset(epd_flush_ctx_->last_fb, kLvglWhite, epd_flush_ctx_->fb_size);
    }

    if (!epd_flush_ctx_->done_sem || !epd_flush_ctx_->last_fb || !epd_flush_ctx_->work_fb) {
        ESP_LOGE(TAG, "epd flush ctx alloc failed");
    }

    esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_cfg.task_stack_size = 16 * 1024;
    adapter_cfg.task_max_delay_ms = 1000;
    // PSRAM 栈上禁止调用 spi_flash/NVS（会触发 cache_utils assert）。
    // 业务里凡 EnsureAudioServiceRunning / Settings 等须放到 DRAM 栈任务（参见录音/翻译）。
    adapter_cfg.stack_in_psram = true;
    adapter_cfg.task_priority = 1;
    adapter_cfg.task_core_id = 1;

    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_cfg));
    ESP_ERROR_CHECK(esp_lv_adapter_set_default_display_idf_callback_registration_enabled(false));

    esp_lv_adapter_display_config_t disp_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_MONO_DEFAULT_CONFIG(
        panel, panel_io, static_cast<uint16_t>(width), static_cast<uint16_t>(height),
        ESP_LV_ADAPTER_ROTATE_270, ESP_LV_ADAPTER_MONO_LAYOUT_HTILED);
    disp_cfg.profile.use_psram = true;

    lv_display_t* disp = esp_lv_adapter_register_display(&disp_cfg);
    ESP_ERROR_CHECK(disp ? ESP_OK : ESP_FAIL);
    epd_flush_ctx_->disp = disp;

    epaper_panel_callbacks_t epd_cbs = {
        .on_epaper_refresh_done = EpdRefreshDoneCb,
    };
    ESP_ERROR_CHECK(epaper_panel_register_event_callbacks(panel, &epd_cbs, epd_flush_ctx_));
    ESP_ERROR_CHECK(esp_lv_adapter_set_area_rounder_cb(disp, EpdAreaRounder, nullptr));

    esp_lv_adapter_draw_bitmap_callbacks_t draw_cbs = {
        .custom_draw_bitmap = EpdLvglDrawBitmap,
    };
    ESP_ERROR_CHECK(
        esp_lv_adapter_set_draw_bitmap_callbacks(disp, &draw_cbs, epd_flush_ctx_));

    if (touch_handle != nullptr) {
        esp_lv_adapter_touch_config_t touch_cfg =
            ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
        touch_cfg.callbacks.custom_touch_read = CustomTouchRead;
        touch_indev_ = esp_lv_adapter_register_touch(&touch_cfg);
        ESP_ERROR_CHECK(touch_indev_ ? ESP_OK : ESP_FAIL);
        StartTouchFeed(touch_handle);
    }

    ESP_ERROR_CHECK(esp_lv_adapter_start());

    const mmap_assets_config_t mmap_cfg = {
        .partition_label = "resources",
        .max_files = MMAP_RESOURCES_FILES,
        .checksum = MMAP_RESOURCES_CHECKSUM,
        .flags = {.mmap_enable = true},
    };
    ESP_ERROR_CHECK(mmap_assets_new(&mmap_cfg, &resources_assets_));

    esp_lv_fs_handle_t fs_handle = nullptr;
    const fs_cfg_t fs_cfg = {
        .fs_letter = 'A',
        .fs_nums = MMAP_RESOURCES_FILES,
        .fs_assets = resources_assets_,
    };
    ESP_ERROR_CHECK(esp_lv_adapter_fs_mount(&fs_cfg, &fs_handle));

    // 在内部 RAM 栈的初始化线程预读 NVS，避免首次进首页/设置时在 LVGL(PSRAM 栈)里碰 flash。
    (void)HomeScreen::LoadCardStyle();
    (void)HapticIsEnabled();
    BookReaderPrefsEnsureLoaded();
    // 待机壁纸路由：开机即灌 NVS→缓存，勿等 Application::Start 联网后才 classic
    wallpaper::HydrateFromNvsNow();
    // 阅读首页快照：与壁纸同点灌入，首页可点时内存已有最近阅读/总时长
    reader::book_home_snapshot::HydrateFromNvs();

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        SetupUI();
        esp_lv_adapter_unlock();
    }
}

void LVAdapterDisplay::SetupUI() {
    if (touch_indev_ == nullptr) {
        ESP_LOGW(TAG, "SetupUI: no touch, load TouchMissingScreen");
        lv_screen_load(TouchMissingScreen::Create());
    } else {
        ESP_LOGI(TAG, "SetupUI: create HomeScreen");
        lv_screen_load(HomeScreen::Create());
        ESP_LOGI(TAG, "SetupUI: HomeScreen loaded");
    }
    // 攒帧后黑→白→首页上屏
    if (lv_screen_active() != nullptr) {
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(nullptr);
    }
    EpdCommitBootBaseline(epd_flush_ctx_);
}

void LVAdapterDisplay::KickTouchInput() {
    if (touch_indev_ != nullptr) {
        esp_lv_adapter_touch_notify_interrupt(touch_indev_);
    }
}

bool LVAdapterDisplay::TryGetResource(const char* name, const uint8_t** mem, size_t* size) const {
    if (name == nullptr || name[0] == '\0' || mem == nullptr || size == nullptr) {
        return false;
    }
    *mem = nullptr;
    *size = 0;
    if (resources_assets_ == nullptr) {
        return false;
    }
    const int n = mmap_assets_get_stored_files(resources_assets_);
    for (int i = 0; i < n; ++i) {
        const char* nm = mmap_assets_get_name(resources_assets_, i);
        if (nm == nullptr || std::strcmp(nm, name) != 0) {
            continue;
        }
        const uint8_t* p = mmap_assets_get_mem(resources_assets_, i);
        const int sz = mmap_assets_get_size(resources_assets_, i);
        if (p == nullptr || sz <= 0) {
            return false;
        }
        *mem = p;
        *size = static_cast<size_t>(sz);
        return true;
    }
    return false;
}

bool TouchUiFingerIsDown(void) {
    portENTER_CRITICAL(&s_touch_snap_mux);
    // live 仍按着，且没有未交付的抬起（抬起已锁存说明手指已离开）
    const bool down = s_touch_snap.live_count > 0 && s_touch_snap.pending_up == 0;
    portEXIT_CRITICAL(&s_touch_snap_mux);
    return down;
}

bool TouchUiHasPendingEdges(void) {
    portENTER_CRITICAL(&s_touch_snap_mux);
    const bool pending = s_touch_snap.pending_down > 0 || s_touch_snap.pending_up > 0;
    portEXIT_CRITICAL(&s_touch_snap_mux);
    return pending;
}

void LVAdapterDisplay::RegisterTouchVirtualKeys(const TouchVirtualKey* keys, size_t count,
                                                TouchVirtualKeyEventCb cb, void* user_data) {
    s_vk_count = 0;
    s_vk_cb = cb;
    s_vk_user = user_data;
    s_vk_instant_was_down = false;
    s_vk_hold = {};
    if (keys == nullptr || count == 0) {
        return;
    }
    if (count > kMaxTouchVirtualKeys) {
        count = kMaxTouchVirtualKeys;
    }
    for (size_t i = 0; i < count; ++i) {
        s_vk_keys[i] = keys[i];
    }
    s_vk_count = count;
    ESP_LOGI(TAG, "registered %u touch virtual keys", static_cast<unsigned>(s_vk_count));
}

void LVAdapterDisplay::RestoreStatusWidgetsLocked() {
    // 切页只换控件指针，图标/静音/时钟缓存跨页保留，首帧与页面内容同一次局刷画出。
    // 局刷已改为启动后立刻 flush_ready（BUSY 延后到下次 Drain），状态栏合并刷新仍有助于少刷。
    if (network_label_ != nullptr) {
        lv_label_set_text(network_label_, network_icon_ != nullptr ? network_icon_ : "");
    }
    if (battery_label_ != nullptr) {
        lv_label_set_text(battery_label_, battery_icon_ != nullptr ? battery_icon_ : "");
    }
    if (battery_pct_label_ != nullptr) {
        lv_label_set_text(battery_pct_label_, battery_pct_cache_);
    }
    if (mute_label_ != nullptr) {
        lv_label_set_text(mute_label_, muted_ ? FONT_AWESOME_VOLUME_XMARK : "");
    }
    if (status_label_ != nullptr) {
        // 无缓存时勿回退「待命」：首页 Idle 应以时钟为准，由 ApplyIdleStatusBar 写入
        const char* body = status_body_cache_[0] != '\0' ? status_body_cache_ : "--:--";
        ApplyStatusTextLocked(body);
        if (!AssistantScreen::IsPttWaveVisible()) {
            lv_obj_remove_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
            if (notification_label_ != nullptr) {
                lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void LVAdapterDisplay::BindStatusWidgets(lv_obj_t* network, lv_obj_t* mute, lv_obj_t* battery,
                                         lv_obj_t* status, lv_obj_t* notification,
                                         lv_obj_t* low_battery_popup, lv_obj_t* battery_pct) {
    network_label_ = network;
    mute_label_ = mute;
    battery_label_ = battery;
    battery_pct_label_ = battery_pct;
    status_label_ = status;
    notification_label_ = notification;
    low_battery_popup_ = low_battery_popup;

    // 勿清 battery_icon_/network_icon_/muted_/status_body_cache_，也勿调 Board/UpdateStatusBar
    // （SetupUI 持锁重入会死锁）。有缓存则立刻灌进新控件，与本页首帧合并刷新。
    RestoreStatusWidgetsLocked();

    if (pending_notification_[0] != '\0' && notification_label_ != nullptr) {
        lv_label_set_text(notification_label_, pending_notification_);
        lv_obj_remove_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
        if (status_label_ != nullptr) {
            lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (notification_timer_ != nullptr) {
            esp_timer_stop(notification_timer_);
            if (pending_notification_ms_ > 0) {
                esp_timer_start_once(notification_timer_,
                                     static_cast<uint64_t>(pending_notification_ms_) * 1000ULL);
            }
        }
        pending_notification_[0] = '\0';
        pending_notification_ms_ = 0;
    }
}

void LVAdapterDisplay::QueueStatusNotification(const char* notification, int duration_ms) {
    if (notification == nullptr || notification[0] == '\0') {
        pending_notification_[0] = '\0';
        pending_notification_ms_ = 0;
        return;
    }
    std::snprintf(pending_notification_, sizeof(pending_notification_), "%s", notification);
    pending_notification_ms_ = duration_ms;
    // 若状态栏已绑定，直接显示
    if (notification_label_ != nullptr) {
        ShowNotification(pending_notification_, duration_ms);
        pending_notification_[0] = '\0';
        pending_notification_ms_ = 0;
    }
}

LVAdapterDisplay::~LVAdapterDisplay() {
    if (instance_ == this) {
        instance_ = nullptr;
    }
    StopTouchFeed();
    if (notification_timer_) {
        esp_timer_stop(notification_timer_);
        esp_timer_delete(notification_timer_);
        notification_timer_ = nullptr;
    }
    if (epd_flush_ctx_) {
        if (epd_flush_ctx_->coalesce_timer) {
            esp_timer_stop(epd_flush_ctx_->coalesce_timer);
            esp_timer_delete(epd_flush_ctx_->coalesce_timer);
            epd_flush_ctx_->coalesce_timer = nullptr;
        }
        if (epd_flush_ctx_->last_fb) {
            heap_caps_free(epd_flush_ctx_->last_fb);
        }
        if (epd_flush_ctx_->work_fb) {
            heap_caps_free(epd_flush_ctx_->work_fb);
        }
        if (epd_flush_ctx_->done_sem) {
            vSemaphoreDelete(epd_flush_ctx_->done_sem);
        }
        delete epd_flush_ctx_;
        epd_flush_ctx_ = nullptr;
    }
}

void LVAdapterDisplay::SetEmotion(const char* emotion) {
    // 非对话页无表情控件，跳过加锁，少打断其它屏局刷。
    if (!AssistantScreen::IsActive()) {
        return;
    }
    DisplayLockGuard lock(this);
    AssistantScreen::SetEmotion(emotion);
}

void LVAdapterDisplay::SetChatMessage(const char* role, const char* content) {
    // 只转发；AddMessage 内自行短持锁做 UI。解析/落盘在锁外，避免流式每包堵住翻页。
    if (!AssistantScreen::IsActive()) {
        return;
    }
    AssistantScreen::AddMessage(role, content);
}

void LVAdapterDisplay::SetStatusTitlePrefix(const char* prefix) {
    DisplayLockGuard lock(this);
    if (prefix == nullptr || prefix[0] == '\0') {
        status_title_prefix_[0] = '\0';
    } else {
        std::snprintf(status_title_prefix_, sizeof(status_title_prefix_), "%s", prefix);
    }
}

void LVAdapterDisplay::BeginStandbyEnterPaint() {
    DisplayLockGuard lock(this);
    auto* ctx = epd_flush_ctx_;
    if (ctx == nullptr || ctx->panel == nullptr) {
        return;
    }
    DrainRefreshIfNeeded(ctx);
    ctx->defer_standby_paint = true;
    ctx->coalesce_pending = false;
    ctx->coalesce_kick_armed = false;
    ESP_LOGI(TAG, "standby enter: defer paint until FULL");
}

void LVAdapterDisplay::ParkEpdForStandby() {
    DisplayLockGuard lock(this);
    auto* ctx = epd_flush_ctx_;
    if (ctx == nullptr || ctx->panel == nullptr || ctx->standby_frozen) {
        return;
    }

    DrainRefreshIfNeeded(ctx);
    ctx->defer_standby_paint = false;

    // settle 攒帧完毕：全刷落待机画面后再冻结
    const uint8_t* fb = ctx->last_fb;
    if (ctx->coalesce_pending && ctx->work_fb != nullptr) {
        fb = ctx->work_fb;
    }
    if (fb != nullptr && ctx->has_last && ctx->last_fb != nullptr) {
        ctx->suppress_lvgl_notify = true;
        xSemaphoreTake(ctx->done_sem, 0);
        ESP_LOGI(TAG, "standby enter: FULL 0xC7 (content before freeze)");
        const esp_err_t err =
            FullRefreshBothSame(ctx, fb, "standby_enter", SSD1677_EPAPER_REFRESH_STANDBY);
        ctx->suppress_lvgl_notify = false;
        if (err == ESP_OK) {
            if (fb != ctx->last_fb) {
                memcpy(ctx->last_fb, fb, ctx->fb_size);
            }
            ctx->partial_refresh_count = 0;
            ctx->force_next_full = false;
            ctx->force_full_c7 = false;
        } else {
            ESP_LOGW(TAG, "standby enter FULL: %s", esp_err_to_name(err));
        }
    }
    ctx->coalesce_pending = false;
    ctx->coalesce_kick_armed = false;
    ctx->refresh_inflight = false;

    // 冻结后续 LVGL flush；不做 0x10 Deep Sleep
    ctx->freeze_updates = true;
    ctx->standby_frozen = true;
    ESP_LOGI(TAG, "standby: flush frozen (no deep sleep)");
}

void LVAdapterDisplay::WakeEpdFromStandby() {
    DisplayLockGuard lock(this);
    auto* ctx = epd_flush_ctx_;
    if (ctx == nullptr || ctx->panel == nullptr) {
        return;
    }
    const bool was_defer = ctx->defer_standby_paint;
    const bool was_frozen = ctx->standby_frozen;
    ctx->defer_standby_paint = false;
    ctx->coalesce_pending = false;
    ctx->coalesce_kick_armed = false;
    if (!was_defer && !was_frozen) {
        return;
    }
    if (was_frozen) {
        ctx->standby_frozen = false;
        ctx->freeze_updates = false;
    }
    // settle 前退出或正常 dismiss：等卸 Overlay 后首帧全刷底层（0xC7）
    ctx->force_next_full = true;
    ctx->force_full_c7 = true;
    ESP_LOGI(TAG, "standby: exit defer/freeze (next frame FULL 0xC7)");
}

void LVAdapterDisplay::RequestNextFullRefresh() {
    if (epd_flush_ctx_ == nullptr) {
        return;
    }
    epd_flush_ctx_->force_next_full = true;
}

void LVAdapterDisplay::SleepEpdForPowerOff() {
    if (epd_flush_ctx_ == nullptr || epd_flush_ctx_->panel == nullptr) {
        return;
    }
    DrainRefreshIfNeeded(epd_flush_ctx_);
    const esp_err_t err = epaper_panel_deep_sleep(epd_flush_ctx_->panel);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "EPD deep sleep: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "EPD deep sleep before PWR_KEY");
}

void LVAdapterDisplay::ShowPoweredOffScreen() {
    // 若待机曾冻结 flush，先恢复再画关机图
    WakeEpdFromStandby();
    DisplayLockGuard lock(this);
    BindStatusWidgets(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // 直接往 EPD 帧缓冲画 A2I1，再关机全刷（0xC7）。
    // 加载链：NVS 源文件 →（失败/坏图）内置 mmap →（再失败）「已关机」文字
    bool have_img = false;
    uint8_t* sd_buf = nullptr;
    size_t sd_len = 0;
    if (epd_flush_ctx_ != nullptr && epd_flush_ctx_->work_fb != nullptr) {
        const uint8_t* mem = nullptr;
        size_t plen = 0;
        bool from_sd = TryLoadShutdownA2i1FromSd(&sd_buf, &sd_len);

        auto try_parse = [&](const uint8_t* raw, size_t raw_len, bool strip_mmap_prefix) -> bool {
            if (raw == nullptr || raw_len == 0) {
                return false;
            }
            const uint8_t* p = raw;
            size_t n = raw_len;
            if (strip_mmap_prefix && n >= 6 && p[0] == 0x5A && p[1] == 0x5A && p[2] == 'A' &&
                p[3] == '2' && p[4] == 'I' && p[5] == '1') {
                p += 2;
                n -= 2;
            }
            uint16_t w = 0, h = 0, stride = 0;
            const uint8_t* payload = nullptr;
            if (!ParseA2i1(p, n, &w, &h, &stride, &payload) || payload == nullptr) {
                return false;
            }
            BlitA2i1ToPanelFb(epd_flush_ctx_->work_fb, epd_flush_ctx_->panel_w,
                              epd_flush_ctx_->panel_h, payload, w, h, stride);
            const esp_err_t err = EpdShutdownFullFb(epd_flush_ctx_, epd_flush_ctx_->work_fb);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "shutdown full refresh failed: %s", esp_err_to_name(err));
                return false;
            }
            return true;
        };

        if (from_sd) {
            if (try_parse(sd_buf, sd_len, false)) {
                have_img = true;
            } else {
                ESP_LOGW(TAG, "shutdown img: SD A2I1 unusable → built-in");
                heap_caps_free(sd_buf);
                sd_buf = nullptr;
                sd_len = 0;
                from_sd = false;
            }
        }

        if (!have_img && resources_assets_ != nullptr) {
            mem = mmap_assets_get_mem(resources_assets_, MMAP_RESOURCES_BG_SHUTDOWN_A2I1);
            const int sz = mmap_assets_get_size(resources_assets_, MMAP_RESOURCES_BG_SHUTDOWN_A2I1);
            if (mem != nullptr && sz > 0) {
                plen = static_cast<size_t>(sz);
                ESP_LOGI(TAG, "shutdown img: use built-in bg_shutdown.a2i1 (%u bytes)",
                         static_cast<unsigned>(plen));
                have_img = try_parse(mem, plen, true);
                if (!have_img) {
                    ESP_LOGW(TAG, "shutdown img: built-in A2I1 parse/blit fail");
                }
            } else {
                ESP_LOGW(TAG, "shutdown img: built-in missing mem=%p sz=%d", mem, sz);
            }
        } else if (!have_img) {
            ESP_LOGW(TAG, "shutdown img: no resources_assets_");
        }
    }
    if (sd_buf != nullptr) {
        heap_caps_free(sd_buf);
        sd_buf = nullptr;
    }

    if (have_img) {
        // 冻结后续 flush，禁止 LVGL 再把白屏刷上去。
        if (epd_flush_ctx_ != nullptr) {
            epd_flush_ctx_->freeze_updates = true;
        }
    } else {
        lv_obj_t* scr = lv_obj_create(nullptr);
        lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* lbl = lv_label_create(scr);
        lv_label_set_text(lbl, Lang::Strings::POWERED_OFF);
        const lv_font_t* font = fontpack_lv_font_get(30, 2);
        if (font == nullptr) {
            font = fontpack_lv_font_ui();
        }
        if (font != nullptr) {
            lv_obj_set_style_text_font(lbl, font, 0);
        }
        lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
        lv_obj_center(lbl);
        lv_obj_t* old_scr = lv_screen_active();
        lv_screen_load(scr);
        if (old_scr != nullptr && old_scr != scr) {
            lv_obj_delete(old_scr);
        }
        lv_obj_invalidate(scr);
        lv_refr_now(nullptr);
        if (epd_flush_ctx_ != nullptr) {
            epd_flush_ctx_->freeze_updates = true;
        }
    }
    ESP_LOGI(TAG, "powered-off screen shown (a2i1=%d)", have_img ? 1 : 0);
}

void LVAdapterDisplay::ApplyStatusTextLocked(const char* status) {
    const char* body = (status != nullptr) ? status : "";
    if (body != status_body_cache_) {
        std::snprintf(status_body_cache_, sizeof(status_body_cache_), "%s", body);
    }
    if (status_label_ == nullptr) {
        return;
    }
    if (status_title_prefix_[0] != '\0') {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s·%s", status_title_prefix_, status_body_cache_);
        lv_label_set_text(status_label_, buf);
    } else {
        lv_label_set_text(status_label_, status_body_cache_);
    }
}

void LVAdapterDisplay::SetStatus(const char* status) {
    // 百问页：屏蔽待命/聆听/回答，保留连接网络/登录等系统提示（跟 DeviceState）
    if (AssistantScreen::IsActive()) {
        const DeviceState state = Application::GetInstance().GetDeviceState();
        if (state == kDeviceStateIdle || state == kDeviceStateListening ||
            state == kDeviceStateSpeaking) {
            return;
        }
    }
    DisplayLockGuard lock(this);
    if (status_label_ == nullptr) {
        return;
    }
    ApplyStatusTextLocked(status);
    lv_obj_remove_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    if (notification_label_) {
        lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
    }
    last_status_update_time_ = std::chrono::system_clock::now();
}

void LVAdapterDisplay::ShowNotification(const char* notification, int duration_ms) {
    DisplayLockGuard lock(this);
    if (notification_label_ == nullptr) {
        return;
    }
    lv_label_set_text(notification_label_, notification);
    lv_obj_remove_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
    if (status_label_) {
        lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    }
    // 音量键通知后同步顶栏静音图标
    if (auto* codec = Board::GetInstance().GetAudioCodec()) {
        const bool want_mute = (codec->output_volume() == 0);
        if (want_mute != muted_ && mute_label_ != nullptr) {
            muted_ = want_mute;
            lv_label_set_text(mute_label_, muted_ ? FONT_AWESOME_VOLUME_XMARK : "");
        }
    }
    if (notification_timer_) {
        esp_timer_stop(notification_timer_);
        ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, duration_ms * 1000ULL));
    }
}

void LVAdapterDisplay::UpdateStatusBar(bool update_all) {
    // 全屏页（老化/关机图等）已 ClearStatusBindings：勿再抢 DisplayLock，减轻与 LVGL 竞态
    if (mute_label_ == nullptr && network_label_ == nullptr && battery_label_ == nullptr &&
        status_label_ == nullptr && notification_label_ == nullptr && battery_pct_label_ == nullptr) {
        return;
    }

    auto& app = Application::GetInstance();
    auto& board = Board::GetInstance();
    auto* codec = board.GetAudioCodec();

    // I2C / 业务读取放锁外；所有 label 改动合并进一次 DisplayLock，减少无效局刷。
    const bool want_mute = (codec != nullptr && codec->output_volume() == 0);

    char time_str[16] = {};
    bool set_time = false;
    const bool assistant_page = AssistantScreen::IsActive();
    const auto device_state = app.GetDeviceState();
    // 百问联网/启动中：顶栏保留「连接中…」等，勿用时钟盖住；Idle/聆听/播报仍刷时钟（对话文案已在 SetStatus 过滤）
    const bool assistant_net_hint =
        assistant_page && (device_state == kDeviceStateConnecting ||
                           device_state == kDeviceStateStarting ||
                           device_state == kDeviceStateWifiConfiguring ||
                           device_state == kDeviceStateActivating);
    const bool want_clock = AllowsIdleStatusClock() && !assistant_net_hint &&
                            (assistant_page ||
                             (!app.HasPendingActivationCode() && !app.IsXiaozhiVoiceActive() &&
                              device_state == kDeviceStateIdle));
    if (want_clock &&
        (update_all || last_status_update_time_ + std::chrono::seconds(10) <
                            std::chrono::system_clock::now())) {
        time_t now = time(nullptr);
        struct tm* tm = localtime(&now);
        if (tm && tm->tm_year >= 2025 - 1900) {
            strftime(time_str, sizeof(time_str), "%H:%M", tm);
            set_time = true;
        }
    }

    int battery_level = 0;
    bool charging = false;
    bool discharging = false;
    const char* new_battery_icon = nullptr;
    const bool battery_ok = board.GetBatteryLevel(battery_level, charging, discharging);
    if (battery_ok) {
        if (charging) {
            new_battery_icon = FONT_AWESOME_BATTERY_BOLT;
        } else {
            static const char* levels[] = {
                FONT_AWESOME_BATTERY_EMPTY, FONT_AWESOME_BATTERY_QUARTER, FONT_AWESOME_BATTERY_HALF,
                FONT_AWESOME_BATTERY_THREE_QUARTERS, FONT_AWESOME_BATTERY_FULL, FONT_AWESOME_BATTERY_FULL,
            };
            new_battery_icon = levels[battery_level / 20];
        }
    }

    const char* new_network_icon = nullptr;
    static int seconds_counter = 0;
    const bool wifi_busy = WifiStation::GetInstance().IsLinkInProgress();
    // 常态约 10s；WiFi 扫描/连接中每秒刷，配合弱→中→强梯度
    if (update_all || wifi_busy || (++seconds_counter % 10 == 0)) {
        static const std::vector<DeviceState> allowed_states = {
            kDeviceStateIdle,       kDeviceStateStarting,   kDeviceStateWifiConfiguring,
            kDeviceStateConnecting, kDeviceStateListening,  kDeviceStateSpeaking,
            kDeviceStateActivating,
        };
        if (update_all || wifi_busy ||
            std::find(allowed_states.begin(), allowed_states.end(), device_state) !=
                allowed_states.end()) {
            new_network_icon = board.GetNetworkStateIcon();
        }
    }

    bool play_low_battery = false;
    {
        DisplayLockGuard lock(this);
        if (mute_label_ == nullptr) {
            return;
        }

        if (want_mute != muted_) {
            muted_ = want_mute;
            lv_label_set_text(mute_label_, muted_ ? FONT_AWESOME_VOLUME_XMARK : "");
        }

        if (set_time && status_label_ != nullptr) {
            ApplyStatusTextLocked(time_str);
            // 百问 PTT 波形占用状态栏中央时只更新文案，勿揭开时钟盖住动画
            // 通知展示中同样勿用时钟盖住（如开机 IO 扩展异常）
            if (!AssistantScreen::IsPttWaveVisible()) {
                const bool notif_on =
                    notification_label_ != nullptr &&
                    !lv_obj_has_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
                if (!notif_on) {
                    lv_obj_remove_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
                    if (notification_label_ != nullptr) {
                        lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }
            last_status_update_time_ = std::chrono::system_clock::now();
        }

        if (battery_ok && new_battery_icon != nullptr) {
            if (battery_icon_ != new_battery_icon) {
                battery_icon_ = new_battery_icon;
                if (battery_label_ != nullptr) {
                    lv_label_set_text(battery_label_, battery_icon_);
                }
            }
            if (battery_level != battery_level_cache_) {
                battery_level_cache_ = battery_level;
                snprintf(battery_pct_cache_, sizeof(battery_pct_cache_), "%d%%", battery_level);
                if (battery_pct_label_ != nullptr) {
                    lv_label_set_text(battery_pct_label_, battery_pct_cache_);
                }
            }
            if (low_battery_popup_ != nullptr) {
                if (std::strcmp(new_battery_icon, FONT_AWESOME_BATTERY_EMPTY) == 0 && discharging) {
                    if (lv_obj_has_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN)) {
                        lv_obj_remove_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
                        play_low_battery = true;
                    }
                } else if (!lv_obj_has_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }

        if (new_network_icon != nullptr && network_icon_ != new_network_icon) {
            network_icon_ = new_network_icon;
            if (network_label_ != nullptr) {
                lv_label_set_text(network_label_, network_icon_);
            }
        }
    }

    if (play_low_battery) {
        app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
    }
}

void LVAdapterDisplay::SetPowerSaveMode(bool on) {
    if (on) {
        SetChatMessage("system", "");
        SetEmotion("sleepy");
    } else {
        SetChatMessage("system", "");
        SetEmotion("neutral");
    }
}

void LVAdapterDisplay::SetPreviewImage(const void* image) {
    (void)image;
}

void LVAdapterDisplay::SetTheme(Theme* theme) {
    Display::SetTheme(theme);
}

bool LVAdapterDisplay::Lock(int timeout_ms) {
    return esp_lv_adapter_lock(timeout_ms) == ESP_OK;
}

void LVAdapterDisplay::Unlock() {
    esp_lv_adapter_unlock();
}
