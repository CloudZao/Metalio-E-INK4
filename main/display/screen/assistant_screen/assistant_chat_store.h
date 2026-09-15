#pragma once

/**
 * @file assistant_chat_store.h
 * @brief Boot-scoped A2UI JSON log on SD (optional).
 *
 * Architecture
 * ------------
 * - Ownership: storage only (dir / boot wipe / seq files / FIFO). Parsing and
 *   LVGL stay in assistant_screen.cc — this module never touches UI.
 * - Path: `SD_PATH_CHAT_LOG` (`/sdcard/metalio/e-ink/chat_log`).
 *   No SD → disabled (RAM-only session, original behaviour).
 *   Has SD → wipe at boot, then append each accepted downlink JSON.
 * - Layout: one file per turn `mNNNNN.json` (raw UTF-8 A2UI object). Seq is the
 *   order of replay. No cross-boot persistence.
 * - Budget (daily chat, not infinite archive):
 *     per file ≤ 24KB, ≤ 40 files, ≤ 192KB total.
 *   Overflow drops oldest files then writes (no piecemeal keep).
 * - Failure policy: never fatal to boot or chat UI.
 *
 * Threading: mutex-protected. Typical callers: Application::Start (init),
 * AssistantScreen::AddMessage / Create (already on LVGL task; payloads are small).
 */

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "sd_paths.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASSISTANT_CHAT_STORE_BASE_PATH SD_PATH_CHAT_LOG

/**
 * If SD is mounted: ensure dir, wipe previous boot's files, mark ready.
 * If no SD: ESP_ERR_NOT_FOUND, cache stays disabled.
 * Idempotent; never fatal.
 */
esp_err_t assistant_chat_store_init(void);

bool assistant_chat_store_is_ready(void);

/**
 * Best-effort append of one A2UI JSON object (NUL-terminated or explicit len).
 * Oversized records are skipped. Budget overflow → drop oldest then write.
 */
esp_err_t assistant_chat_store_append(const char *json, size_t len);

/** Delete all records (deleteSurface / explicit). No-op if not ready. */
esp_err_t assistant_chat_store_wipe(void);

/**
 * Walk records in seq order. Callback must not re-enter this module.
 * json is NUL-terminated; valid only during the callback.
 * Returns ESP_OK even if zero records (or store disabled → ESP_ERR_INVALID_STATE).
 */
typedef bool (*assistant_chat_store_visit_fn)(const char *json, size_t len, void *ctx);

esp_err_t assistant_chat_store_for_each(assistant_chat_store_visit_fn fn, void *ctx);

#ifdef __cplusplus
}
#endif
