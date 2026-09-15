#pragma once

#include <functional>

// OTA 升级确认弹窗：系统级 Overlay，用于处理升级确认与忽略逻辑。
class OtaConfirmDialog {
public:
    enum class Mode {
        Boot,
        About,
    };

    enum class Choice {
        Upgrade,
        IgnorePersist,
        RemindLater,
    };

    using ChoiceCallback = std::function<void(Choice)>;

    // 阻塞等待用户选择，适用于开机升级确认流程。
    static Choice ShowBlocking(const char* current_version, const char* new_version, Mode mode);

    // 非阻塞展示并在用户选择后回调，适合关于页场景。
    static bool ShowAsync(const char* current_version, const char* new_version, Mode mode,
                          ChoiceCallback cb);

    static void Dismiss();
    static bool IsActive();
};
