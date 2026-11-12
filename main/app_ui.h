/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file app_ui.h
 * @brief What Badge Audit shows on the glass, built with kui; only the app's Active Object calls it.
 */
#ifndef APP_UI_H_
#define APP_UI_H_

#include <stdbool.h>
#include <stdint.h>

#include "kui.h"

#include "badge_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Types ---- */

/** @brief What the status line says about the scanning. */
typedef enum {
    APP_UI_SCANNING = 0,   /**< The reader is looking for a badge. */
    APP_UI_PAUSED,         /**< The owner paused the scanning. */
    APP_UI_FAULT           /**< The reader answered with an error; the text says which. */
} app_ui_state_t;

/* ---- Calls ---- */

/**
 * @brief Build the screen on LVGL's active screen; call once, after kscreen_open() succeeded.
 * @param on_tap Called on LVGL's thread for a tap anywhere on the glass.
 * @param user   Handed to @p on_tap as the event's user data.
 */
void app_ui_build(lv_event_cb_t on_tap, void *user);

/**
 * @brief Show the state of the scanning on the status line.
 * @param state What the scanning is doing.
 * @param fault Why the reader failed, for APP_UI_FAULT; NULL or "" otherwise.
 */
void app_ui_status(app_ui_state_t state, const char *fault);

/**
 * @brief Show one badge of the history on the card, or the empty card.
 * @param e     The badge; NULL for none yet.
 * @param age   Its place in the history, 0 for the newest.
 * @param count The badges the history holds.
 */
void app_ui_show(const badge_log_entry_t *e, uint32_t age, uint32_t count);

/**
 * @brief Log anything the screen draws in the glass's corners (catalog rule D2).
 * @return The number of objects outside the glass; 0 when the layout is clean.
 */
uint32_t app_ui_check_glass(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_H_ */
