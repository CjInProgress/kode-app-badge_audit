/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file badge_class.h
 * @brief How secure a badge is, from the band it answered on and the model the reader named.
 */
#ifndef BADGE_CLASS_H_
#define BADGE_CLASS_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Types ---- */

/** @brief The radio band a badge answered on. */
typedef enum {
    BADGE_BAND_HF = 0,   /**< 13.56 MHz, read by the NFC chip (kode_nfc_read_id()). */
    BADGE_BAND_LF,       /**< 125 kHz, read by the RFID front end (kode_rfid_read_id()). */
    BADGE_BAND_COUNT     /**< The number of bands; not a band. */
} badge_band_t;

/** @brief How much a badge resists being copied, worst first. */
typedef enum {
    BADGE_RISK_CLONEABLE = 0,   /**< No cryptography at all: a read is a copy. */
    BADGE_RISK_OPEN,            /**< Readable by anyone; not made to be a credential. */
    BADGE_RISK_WEAK,            /**< Has cryptography, and it is broken. */
    BADGE_RISK_STRONG_IF,       /**< Sound cryptography, if the reader uses it. */
    BADGE_RISK_UNKNOWN,         /**< A family this table does not know. */
    BADGE_RISK_COUNT            /**< The number of levels; not a level. */
} badge_risk_t;

/** @brief One row of the security table: what a badge family is and what to tell its owner. */
typedef struct {
    const char   *prefix;    /**< The start of the reader's model name that picks this row; "" matches any. */
    badge_band_t  band;      /**< The band the row is for. */
    badge_risk_t  risk;      /**< How much the family resists a copy. */
    const char   *family;    /**< The family's name, as the screen shows it. */
    const char   *verdict;   /**< The verdict, a few words. */
    const char   *tip;       /**< One sentence on what to do about it. */
} badge_class_t;

/* ---- Calls ---- */

/**
 * @brief Find the row of the security table for a badge.
 * @param band The band the badge answered on.
 * @param name The model the reader named, such as "Mifare Classic 1K"; NULL is treated as "".
 * @return The row, never NULL: an unknown family gets the band's catch-all row.
 */
const badge_class_t *badge_class_find(badge_band_t band, const char *name);

/**
 * @brief Whether a UID is a random one, re-drawn by the card at every power-up.
 *
 * ISO/IEC 14443-3 marks a random single-size UID with 0x08 in its first byte;
 * a DESFire card in privacy mode sends one. It cannot be used to track the
 * badge, nor to check it at a door.
 *
 * @param id The UID as kode_tag_t gives it, "08:A2:3B:91"; NULL answers false.
 * @return true when @p id is a four-byte UID that starts with 0x08.
 */
bool badge_class_uid_random(const char *id);

/**
 * @brief The band's name, as the screen shows it.
 * @param band The band.
 * @return "13.56 MHz" or "125 kHz"; "?" for a value out of range.
 */
const char *badge_class_band_name(badge_band_t band);

#ifdef __cplusplus
}
#endif

#endif /* BADGE_CLASS_H_ */
