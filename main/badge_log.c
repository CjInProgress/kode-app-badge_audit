/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file badge_log.c
 * @brief The session's history, a ring of BADGE_LOG_MAX entries; only the app's Active Object calls it.
 */
#include "badge_log.h"

#include <stdio.h>
#include <string.h>

/** @brief The history: a ring, newest at @c head, @c count entries behind it. */
typedef struct {
    badge_log_entry_t entries[BADGE_LOG_MAX];   /**< The ring. */
    uint32_t          head;                     /**< The slot of the newest entry. */
    uint32_t          count;                    /**< Entries held. */
} badge_log_t;

/* In RAM only: the history is the session's, and a badge's UID is not
 * something to leave in flash for the next person who picks up the Dot. */
static badge_log_t s_log;

/**
 * @brief Whether a read is the same badge as an entry.
 * @param e    The entry.
 * @param band The read's band.
 * @param tag  The read.
 * @param rnd  The read's UID is random.
 * @return true when they are the same badge.
 */
static bool same_badge(const badge_log_entry_t *e, badge_band_t band, const kode_tag_t *tag,
                       bool rnd)
{
    bool same = false;

    if ( (band == e->band) && (0 == strcmp(e->name, tag->name)) )
    {
        if ( rnd && e->random_uid )
        {
            /* A random UID changes at every read: the model is all there is to match. */
            same = true;
        }
        else
        {
            same = (0 == strcmp(e->id, tag->id));
        }
    }
    return same;
}

void badge_log_clear(void)
{
    (void)memset(&s_log, 0, sizeof(s_log));
}

bool badge_log_add(badge_band_t band, const kode_tag_t *tag)
{
    const bool         rnd   = badge_class_uid_random(tag->id);
    badge_log_entry_t *e     = NULL;
    bool               added = false;

    if ( (0U < s_log.count) && same_badge(&s_log.entries[s_log.head], band, tag, rnd) )
    {
        e = &s_log.entries[s_log.head];
        e->reads++;
        /* A random UID is shown as last read. */
        (void)snprintf(e->id, sizeof(e->id), "%s", tag->id);
    }
    else
    {
        if ( 0U < s_log.count )
        {
            s_log.head = (s_log.head + 1U) % BADGE_LOG_MAX;
        }
        if ( BADGE_LOG_MAX > s_log.count )
        {
            s_log.count++;
        }
        e = &s_log.entries[s_log.head];
        e->cls        = badge_class_find(band, tag->name);
        e->band       = band;
        e->random_uid = rnd;
        e->reads      = 1U;
        (void)snprintf(e->id, sizeof(e->id), "%s", tag->id);
        (void)snprintf(e->name, sizeof(e->name), "%s", tag->name);
        added = true;
    }
    return added;
}

uint32_t badge_log_count(void)
{
    return s_log.count;
}

const badge_log_entry_t *badge_log_get(uint32_t age)
{
    const badge_log_entry_t *e = NULL;

    if ( age < s_log.count )
    {
        /* Backwards from the head, wrapping: adding BADGE_LOG_MAX keeps it unsigned. */
        e = &s_log.entries[((s_log.head + BADGE_LOG_MAX) - age) % BADGE_LOG_MAX];
    }
    return e;
}
