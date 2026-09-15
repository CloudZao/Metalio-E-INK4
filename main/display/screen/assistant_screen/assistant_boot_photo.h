#pragma once

/**
 * 百问 AI：BOOT 双击拍照 → A2UI Image 插入对话。
 *
 * 架构（勿再旁路直驱 UsbUvcStill + OTG）：
 * - 按键线程只置 busy 并起 worker；禁止在按键/LVGL 线程做 USB
 * - 静拍唯一入口：Camera::CaptureStillJpeg → UvcStillCamera::Capture
 *   （OTG on → 拉低 P0.0 → 会话 → JPEG → StopSession → OTG off；otg_held_ 自管）
 * - 双击先插「用户：拍照中」；拍完只补 Image（不再发「用户：拍照」）
 * - JPEG→L8 → a2ui_image_put_local_l8(mem://…) →（可选）A2I1 img_cache 翻页回退
 * - 同时 POST 原始 JPEG → api::VisionUploadUrl（软失败不影响插图）
 * - ScreenLvAsync → AssistantScreen::AddMessage（A2UI Image 语法）
 * - 上屏走 L8（与产测相机同源）；禁止 I1 直显（本板只剩白底线框）
 */
bool AssistantBootPhoto_OnDoubleClick();
