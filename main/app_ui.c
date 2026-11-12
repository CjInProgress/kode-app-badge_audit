/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file app_ui.c
 * @brief Badge Audit's screen: a title, the status of the scanning, the badge's card and its place in the history.
 */
#include "app_ui.h"

#include <stddef.h>

#include "kode_sys.h"

#define TAG "badge_audit.ui"   /**< This file's log tag. */

/** Where the column starts, from the top of the glass: the first row of rule D2. */
#define UI_TOP_Y          53
/** The gap between two rows of the column. */
#define UI_ROW_GAP        KUI_SPACE_MD
/** The card's corner radius: rounder than a row, so it reads as a card. */
#define UI_CARD_RADIUS    28
/** The card's padding. */
#define UI_CARD_PAD       KUI_SPACE_LG
/** The gap between two lines of the card. */
#define UI_CARD_GAP       KUI_SPACE_SM
/** The diameter of the lamp beside the verdict. */
#define UI_LAMP_D         18

/** @brief The labels and the lamp that change; LVGL owns the objects. */
typedef struct {
    bool       built;     /**< app_ui_build() has run. */
    lv_obj_t  *status;    /**< What the scanning is doing. */
    lv_obj_t  *lamp;      /**< Glows in the verdict's colour. */
    lv_obj_t  *verdict;   /**< The verdict, in its colour. */
    lv_obj_t  *family;    /**< The model and the band. */
    lv_obj_t  *uid;       /**< The UID. */
    lv_obj_t  *tip;       /**< What to do about it. */
    lv_obj_t  *footer;    /**< Its place in the history. */
} app_ui_t;

static app_ui_t s_ui;   /**< The one screen. */

/**
 * @brief The colour a risk level is shown in.
 * @param risk The level.
 * @param lit  Receives whether the lamp is lit for it.
 * @return The colour, from kui's tokens.
 */
static lv_color_t risk_colour(badge_risk_t risk, bool *lit)
{
    lv_color_t colour = KUI_COLOR_TEXT_MUTED;

    *lit = true;
    switch ( risk )
    {
        case BADGE_RISK_CLONEABLE:
        case BADGE_RISK_OPEN:
        {
            colour = KUI_COLOR_DANGER;
            break;
        }
        case BADGE_RISK_WEAK:
        {
            colour = KUI_COLOR_WARN;
            break;
        }
        case BADGE_RISK_STRONG_IF:
        {
            colour = KUI_COLOR_OK;
            break;
        }
        default:
        {
            /* BADGE_RISK_UNKNOWN: no colour claims anything. */
            *lit = false;
            break;
        }
    }
    return colour;
}

/**
 * @brief A label that wraps within its parent's width.
 * @param parent The parent.
 * @param font   One of kui's faces.
 * @param colour Its colour.
 * @param align  How its lines sit.
 * @return The label.
 */
static lv_obj_t *label_new(lv_obj_t *parent, const lv_font_t *font, lv_color_t colour,
                           lv_text_align_t align)
{
    lv_obj_t * const label = lv_label_create(parent);

    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, colour, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_text(label, "");
    return label;
}

/**
 * @brief Let a tap through to the screen: a container would otherwise take it.
 * @param obj The container.
 */
static void pass_taps(lv_obj_t *obj)
{
    lv_obj_set_clickable(obj, false);
    lv_obj_set_scrollable(obj, false);
}

/**
 * @brief The card: the verdict beside its lamp, the model, the UID and the tip.
 * @param parent The column.
 */
static void card_build(lv_obj_t *parent)
{
    lv_obj_t * const card = kui_flex_create(parent, LV_FLEX_FLOW_COLUMN, UI_CARD_GAP, 0U,
                                            LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                                            LV_FLEX_ALIGN_START);
    lv_obj_t *row;

    lv_obj_set_name(card, "card");
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_style_bg_color(card, KUI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(card, UI_CARD_PAD, 0);
    pass_taps(card);

    row = kui_flex_create(card, LV_FLEX_FLOW_ROW, KUI_SPACE_MD, 0U, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(row, lv_pct(100));
    pass_taps(row);
    s_ui.lamp    = kui_lamp_create(row, UI_LAMP_D);
    s_ui.verdict = label_new(row, &kui_font_name, KUI_COLOR_TEXT_ON_BG, LV_TEXT_ALIGN_LEFT);
    /* The verdict takes what the lamp leaves of the row, and wraps there. */
    lv_obj_set_width(s_ui.verdict, 0);
    lv_obj_set_flex_grow(s_ui.verdict, 1U);

    s_ui.family = label_new(card, &kui_font_caption, KUI_COLOR_TEXT_ON_BG, LV_TEXT_ALIGN_LEFT);
    s_ui.uid    = label_new(card, &kui_font_caption, KUI_COLOR_TEXT_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.tip    = label_new(card, &kui_font_caption, KUI_COLOR_TEXT_MUTED, LV_TEXT_ALIGN_LEFT);
}

void app_ui_build(lv_event_cb_t on_tap, void *user)
{
    lv_obj_t * const scr = lv_screen_active();
    lv_obj_t        *col;
    lv_obj_t        *title;

    lv_obj_add_style(scr, &kui_style_screen, 0);
    lv_obj_set_scrollable(scr, false);
    lv_obj_add_event_cb(scr, on_tap, LV_EVENT_CLICKED, user);

    /* One full-width column, kui's row width, between the corners. */
    col = kui_flex_create(scr, LV_FLEX_FLOW_COLUMN, UI_ROW_GAP, 0U, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_name(col, "column");
    lv_obj_set_width(col, KUI_PILL_W);
    lv_obj_align(col, LV_ALIGN_TOP_MID, 0, UI_TOP_Y);
    pass_taps(col);

    title = label_new(col, &kui_font_heading, KUI_COLOR_TEXT_ON_BG, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text(title, "Badge Audit");
    s_ui.status = label_new(col, &kui_font_caption, KUI_COLOR_TEXT_MUTED, LV_TEXT_ALIGN_CENTER);
    card_build(col);
    s_ui.footer = label_new(col, &kui_font_caption, KUI_COLOR_TEXT_MUTED, LV_TEXT_ALIGN_CENTER);

    s_ui.built = true;
    app_ui_status(APP_UI_SCANNING, NULL);
    app_ui_show(NULL, 0U, 0U);
}

void app_ui_status(app_ui_state_t state, const char *fault)
{
    if ( s_ui.built )
    {
        if ( APP_UI_PAUSED == state )
        {
            lv_label_set_text(s_ui.status, "Paused. TOP or tap to scan.");
            lv_obj_set_style_text_color(s_ui.status, KUI_COLOR_TEXT_MUTED, 0);
        }
        else if ( (APP_UI_FAULT == state) && (NULL != fault) )
        {
            lv_label_set_text_fmt(s_ui.status, "Reader: %s", fault);
            lv_obj_set_style_text_color(s_ui.status, KUI_COLOR_WARN, 0);
        }
        else
        {
            lv_label_set_text(s_ui.status, "Scanning. Hold a badge to the Dot.");
            lv_obj_set_style_text_color(s_ui.status, KUI_COLOR_TEXT_MUTED, 0);
        }
    }
}

void app_ui_show(const badge_log_entry_t *e, uint32_t age, uint32_t count)
{
    bool       lit = false;
    lv_color_t colour;

    if ( !s_ui.built )
    {
        /* No screen yet: the console's log has the scan. */
    }
    else if ( NULL == e )
    {
        kui_lamp_set(s_ui.lamp, KUI_COLOR_OFF, false);
        lv_label_set_text(s_ui.verdict, "No badge yet");
        lv_obj_set_style_text_color(s_ui.verdict, KUI_COLOR_TEXT_ON_BG, 0);
        lv_label_set_text(s_ui.family, "Reads 13.56 MHz and 125 kHz badges.");
        lv_label_set_text(s_ui.uid, "");
        lv_label_set_text(s_ui.tip, "Read and assess only: nothing is copied, written or saved.");
        lv_label_set_text(s_ui.footer, "TOP or tap: pause scanning");
    }
    else
    {
        colour = risk_colour(e->cls->risk, &lit);
        kui_lamp_set(s_ui.lamp, colour, lit);
        lv_label_set_text(s_ui.verdict, e->cls->verdict);
        lv_obj_set_style_text_color(s_ui.verdict, colour, 0);
        if ( '\0' != e->name[0] )
        {
            lv_label_set_text_fmt(s_ui.family, "%s, %s", e->name, badge_class_band_name(e->band));
        }
        else
        {
            lv_label_set_text_fmt(s_ui.family, "%s, %s", e->cls->family,
                                  badge_class_band_name(e->band));
        }
        if ( e->random_uid )
        {
            lv_label_set_text_fmt(s_ui.uid, "UID %s (random)", e->id);
        }
        else
        {
            lv_label_set_text_fmt(s_ui.uid, "UID %s", e->id);
        }
        lv_label_set_text(s_ui.tip, e->cls->tip);
        lv_label_set_text_fmt(s_ui.footer, "%u of %u, read %ux - UP/DOWN",
                              (unsigned)(age + 1U), (unsigned)count, (unsigned)e->reads);
    }
}

/**
 * @brief Log one object kui_panel_check() found outside the glass.
 * @param obj      The object; unused, its name says which.
 * @param name     Its name.
 * @param corner   The corner it enters.
 * @param depth_px How far into the corner.
 * @param user     Unused.
 */
static void glass_report(lv_obj_t *obj, const char *name, kui_panel_corner_t corner,
                         int32_t depth_px, void *user)
{
    (void)obj;
    (void)user;
    KLOG_W(TAG, "'%s' is %d px into corner %d", name, (int)depth_px, (int)corner);
}

uint32_t app_ui_check_glass(void)
{
    const kui_glass_t glass = kui_glass_kode_dot();
    uint32_t          n     = 0U;

    if ( s_ui.built )
    {
        /* The layout is computed on the next render pass; the check needs it now. */
        lv_obj_update_layout(lv_screen_active());
        n = kui_panel_check(&glass, lv_screen_active(), &glass_report, NULL);
        if ( 0U == n )
        {
            KLOG_I(TAG, "the screen clears the glass's corners");
        }
    }
    return n;
}
