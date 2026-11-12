/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file badge_class.c
 * @brief The security table: one row per badge family, picked by band and the reader's model name.
 */
#include "badge_class.h"

#include <stddef.h>
#include <string.h>

/** Characters of a four-byte UID as kode_tag_t writes it: "08:A2:3B:91". */
#define BADGE_UID4_TEXT_LEN   11U
/** The first byte of a random single-size UID (ISO/IEC 14443-3), as text. */
#define BADGE_UID_RANDOM_TAG  "08"
/** Characters of one UID byte as text. */
#define BADGE_UID_BYTE_CHARS  2U

/*
 * The table, read top to bottom: the first row whose band matches and whose
 * prefix starts the reader's model name wins, so every band ends in a row
 * with the prefix "" that catches the rest. The model names are the ones
 * kode_nfc_read_id() and kode_rfid_read_id() give (the SDK's knfc_tag_model()
 * and kode_rfid.c); the README has the same table with its reasons.
 */
static const badge_class_t s_table[] =
{
    {
        "EM4100", BADGE_BAND_LF, BADGE_RISK_CLONEABLE, "EM4100",
        "Cloneable: no cryptography",
        "It sends its ID in clear to any reader. Replace it with DESFire EV2/EV3."
    },
    {
        "", BADGE_BAND_LF, BADGE_RISK_CLONEABLE, "125 kHz badge",
        "Cloneable: no cryptography",
        "125 kHz badges (EM, HID Prox) carry no crypto. Plan a move to 13.56 MHz."
    },
    {
        "Mifare Classic", BADGE_BAND_HF, BADGE_RISK_WEAK, "Mifare Classic",
        "Weak: broken Crypto-1",
        "Its keys fall to free tools in minutes. Move to DESFire EV2/EV3."
    },
    {
        "Mifare Mini", BADGE_BAND_HF, BADGE_RISK_WEAK, "Mifare Mini",
        "Weak: broken Crypto-1",
        "A small Mifare Classic, with the same broken cipher. Replace it."
    },
    {
        "Ultralight", BADGE_BAND_HF, BADGE_RISK_OPEN, "Ultralight / NTAG",
        "No protection",
        "Made for posters and tickets, not doors: its ID is trivial to copy."
    },
    {
        "Type-4", BADGE_BAND_HF, BADGE_RISK_STRONG_IF, "DESFire / ISO-DEP",
        "Strong if the reader checks keys",
        "Safe only if the reader runs AES checks; one that trusts the UID is not."
    },
    {
        "", BADGE_BAND_HF, BADGE_RISK_UNKNOWN, "Unknown 13.56 MHz card",
        "Unknown family",
        "The reader could not name it. Check its markings before you trust it."
    }
};

/** Rows in the table. */
#define BADGE_TABLE_ROWS  (sizeof(s_table) / sizeof(s_table[0]))

/**
 * @brief Whether @p text starts with @p prefix.
 * @param text   The text.
 * @param prefix The start to look for; "" starts every text.
 * @return true when it does.
 */
static bool starts_with(const char *text, const char *prefix)
{
    const size_t n = strlen(prefix);

    return (0 == strncmp(text, prefix, n));
}

const badge_class_t *badge_class_find(badge_band_t band, const char *name)
{
    const badge_class_t *row  = NULL;
    const char          *text = "";
    size_t               i;

    if ( NULL != name )
    {
        text = name;
    }
    for ( i = 0U; (BADGE_TABLE_ROWS > i) && (NULL == row); ++i )
    {
        if ( (s_table[i].band == band) && starts_with(text, s_table[i].prefix) )
        {
            row = &s_table[i];
        }
    }
    if ( NULL == row )
    {
        /* Only a band out of range gets here: the last row says it is unknown. */
        row = &s_table[BADGE_TABLE_ROWS - 1U];
    }
    return row;
}

bool badge_class_uid_random(const char *id)
{
    bool random = false;

    if ( (NULL != id) && (BADGE_UID4_TEXT_LEN == strlen(id)) )
    {
        random = (0 == strncmp(id, BADGE_UID_RANDOM_TAG, BADGE_UID_BYTE_CHARS));
    }
    return random;
}

const char *badge_class_band_name(badge_band_t band)
{
    const char *name = "?";

    if ( BADGE_BAND_HF == band )
    {
        name = "13.56 MHz";
    }
    else if ( BADGE_BAND_LF == band )
    {
        name = "125 kHz";
    }
    else
    {
        /* Out of range: "?". */
    }
    return name;
}
