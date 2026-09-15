#ifndef DISPLAY_H
#define DISPLAY_H

#include "emoji_collection.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#define HAVE_LVGL 1
#include <lvgl.h>
#endif

#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>

#include <string>
#include <chrono>
#include <cstdint>

class Theme {
public:
    Theme(const std::string& name) : name_(name) {}
    virtual ~Theme() = default;

    inline std::string name() const { return name_; }
private:
    std::string name_;
};

// Idle 时状态栏中部文案策略，由当前 Screen 在进/离页时设置。
enum class IdleStatusMode : uint8_t {
    kClock = 0,      // 周期刷新 HH:MM
    kFixedText = 1,  // 固定文案，如会话页的「待命」
};

class Display {
public:
    Display();
    virtual ~Display();

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    virtual void SetTheme(Theme* theme);
    virtual Theme* GetTheme() { return current_theme_; }
    virtual void UpdateStatusBar(bool update_all = false);
    virtual void SetPowerSaveMode(bool on);

    // 切页时由 Screen 设置 Idle 状态栏策略；离页后恢复为 kClock。
    // fixed_text 仅在 kFixedText 时生效，空值时回退为「待命」。
    void SetIdleStatusMode(IdleStatusMode mode, const char* fixed_text = nullptr);
    IdleStatusMode GetIdleStatusMode() const { return idle_status_mode_; }
    const char* GetIdleStatusFixedText() const { return idle_status_fixed_text_; }
    // UpdateStatusBar / ApplyIdle 共用，判断是否允许刷时钟文案。
    bool AllowsIdleStatusClock() const;

    inline int width() const { return width_; }
    inline int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;

    Theme* current_theme_ = nullptr;
    IdleStatusMode idle_status_mode_ = IdleStatusMode::kClock;
    char idle_status_fixed_text_[32] = {};

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;
};


class DisplayLockGuard {
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    Display *display_;
};

class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override {
        return true;
    }
    virtual void Unlock() override {}
};

#endif
