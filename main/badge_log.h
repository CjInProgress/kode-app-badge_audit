/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file badge_log.h
 * @brief The session's history: the last badges read, newest first, kept in RAM until the app ends.
 */
#ifndef BADGE_LOG_H_
#define BADGE_LOG_H_

#include <stdbool.h>
#include <stdint.h>

#include "kode.h"

#include "badge_class.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Limits ---- */

#define BADGE_LOG_MAX  8U   /**< Badges the history keeps; the oldest goes when a new one comes. */

/* ---- Types ---- */

/** @brief One badge in the history. */
typedef struct {
    const badge_class_t *cls;                       /**< Its row of the security table. */
    badge_band_t         band;                      /**< The band it answered on. */
    bool                 random_uid;                /**< Its UID is re-drawn at every read. */
    uint32_t             reads;                     /**< Reads of it since it came in, at least 1. */
    char                 id[KODE_TAG_ID_MAX];       /**< Its UID, "04:A2:3B:91". */
    char                 name[KODE_TAG_NAME_MAX];   /**< The model the reader named. */
} badge_log_entry_t;

/* ---- Calls ---- */

/**
 * @brief Empty the history.
 */
void badge_log_clear(void);

/**
 * @brief Add a read badge, or count one more read of the newest.
 *
 * A badge held on the antenna is read again and again: a read of the same
 * badge as the newest entry only counts. Two reads are the same badge when
 * their band, model and UID match, or, for a random UID, their band and model.
 *
 * @param band The band it answered on.
 * @param tag  What the reader gave; not NULL, and @c ok is true.
 * @return true when the badge is new to the top of the history, false when it was counted.
 */
bool badge_log_add(badge_band_t band, const kode_tag_t *tag);

/**
 * @brief The number of badges the history holds.
 * @return 0 to BADGE_LOG_MAX.
 */
uint32_t badge_log_count(void);

/**
 * @brief One badge of the history.
 * @param age 0 for the newest, up to badge_log_count() - 1.
 * @return The entry; NULL when @p age is not below badge_log_count().
 */
const badge_log_entry_t *badge_log_get(uint32_t age);

#ifdef __cplusplus
}
#endif

#endif /* BADGE_LOG_H_ */
