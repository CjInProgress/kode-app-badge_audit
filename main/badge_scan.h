/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file badge_scan.h
 * @brief One read-only scan for a badge, both bands in turn: a job for a kode_worker, never for an Active Object.
 */
#ifndef BADGE_SCAN_H_
#define BADGE_SCAN_H_

#include <stdint.h>

#include "kode.h"

#include "badge_class.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Limits ---- */

#define BADGE_SCAN_NFC_MS     600U   /**< How long a scan looks for a 13.56 MHz badge, ms. */
#define BADGE_SCAN_RFID_MS    400U   /**< How long a scan looks for a 125 kHz badge, ms: AO_Rfid decodes in windows of about 150 ms. */
#define BADGE_SCAN_ERROR_MAX  64U    /**< Bytes kept of a reader's error text, NUL included. */

/* ---- Types ---- */

/** @brief One scan's result, written by the job and read by the Active Object once the done event came. */
typedef struct {
    badge_band_t band;                          /**< The band the badge answered on. */
    kode_tag_t   tag;                           /**< The badge; @c ok false when none came. */
    char         error[BADGE_SCAN_ERROR_MAX];   /**< The reader's fault; "" for none or a plain miss. */
} badge_scan_t;

/* ---- Calls ---- */

/**
 * @brief The scan: look for a 13.56 MHz badge, then for a 125 kHz one.
 *
 * A kode_worker_fn_t. Read-only: it asks each reader for the badge's
 * identity and nothing else. Both reads block for their window, which is why
 * this runs on a worker's task. Nothing here reaches the flash.
 *
 * @param arg   The badge_scan_t to fill; its owner leaves it alone until the done event.
 * @param value Receives the band, for the log.
 * @return KODE_OK with a badge, KODE_TIMEOUT when none came, KODE_IO_ERROR when a reader failed.
 */
kode_status_t badge_scan_job(void *arg, uint32_t *value);

#ifdef __cplusplus
}
#endif

#endif /* BADGE_SCAN_H_ */
