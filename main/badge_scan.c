/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file badge_scan.c
 * @brief The scan job: kode_nfc_read_id(), then kode_rfid_read_id(), on a worker's task.
 */
#include "badge_scan.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** The start of kode_nfc_read_id()'s error when no badge came: a miss, not a fault. */
#define BADGE_SCAN_NFC_MISS   "no NFC tag"
/** The start of kode_rfid_read_id()'s error when no badge came. */
#define BADGE_SCAN_RFID_MISS  "no RFID tag"

/**
 * @brief Keep a reader's error, unless it only says that no badge came.
 * @param scan  The scan; the first fault is kept.
 * @param error kode_error() after the failed read.
 * @param miss  The start of the error that is a plain miss.
 */
static void note_error(badge_scan_t *scan, const char *error, const char *miss)
{
    const bool is_miss = (0 == strncmp(error, miss, strlen(miss)));

    if ( ('\0' == scan->error[0]) && ('\0' != error[0]) && !is_miss )
    {
        (void)snprintf(scan->error, sizeof(scan->error), "%s", error);
    }
}

kode_status_t badge_scan_job(void *arg, uint32_t *value)
{
    badge_scan_t * const scan = (badge_scan_t *)arg;
    kode_status_t        rc   = KODE_TIMEOUT;

    scan->error[0] = '\0';
    scan->band     = BADGE_BAND_HF;
    scan->tag      = kode_nfc_read_id(BADGE_SCAN_NFC_MS);
    if ( scan->tag.ok )
    {
        rc = KODE_OK;
    }
    else
    {
        /* kode_error() is this task's: it has to be copied before the next call. */
        note_error(scan, kode_error(), BADGE_SCAN_NFC_MISS);
        scan->band = BADGE_BAND_LF;
        scan->tag  = kode_rfid_read_id(BADGE_SCAN_RFID_MS);
        if ( scan->tag.ok )
        {
            /* A badge came: a fault of the other reader is not worth showing now. */
            scan->error[0] = '\0';
            rc = KODE_OK;
        }
        else
        {
            note_error(scan, kode_error(), BADGE_SCAN_RFID_MISS);
            if ( '\0' != scan->error[0] )
            {
                rc = KODE_IO_ERROR;
            }
        }
    }
    *value = (uint32_t)scan->band;
    return rc;
}
