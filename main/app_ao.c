/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file app_ao.c
 * @brief Badge Audit's Active Object: the screen, and a worker that keeps reading for a badge.
 */
#include "app_ao.h"

#include <string.h>

#include "kode_worker.h"
#include "kscreen.h"

#include "app_ui.h"
#include "badge_class.h"
#include "badge_log.h"
#include "badge_scan.h"

#define TAG "badge_audit.ao"   /**< This file's log tag. */

#define APP_WORKER_NAME         "badge_scan"   /**< The worker's task name. */
#define APP_WORKER_STACK_BYTES  8192U          /**< The worker's stack, in PSRAM: the least the catalog accepts (rule C8). */
#define APP_WORKER_DEPTH        1U             /**< Jobs that may wait besides the one running. */

/** How long the scanning rests after the reader answered with an error, ms, so a failing reader is not hammered. */
#define APP_RETRY_MS            2000U

/** @brief The app's own signals: they start at MAX_SIG and are posted, never published. */
enum AppAoSignals
{
    APP_TICK_SIG = MAX_SIG,   /**< kscreen's render pass. */
    APP_SCAN_DONE_SIG,        /**< The worker's scan returned. */
    APP_RETRY_SIG             /**< The rest after a reader error is over. */
};

/** @brief The Active Object's own state, private to this file. */
typedef struct {
    QActive       super;       /**< The QP/C base; must stay first. */
    kscreen_t     screen;      /**< LVGL on the glass, driven from this Active Object. */
    kode_worker_t worker;      /**< Runs the scans, which wait for the reader. */
    QTimeEvt      retry;       /**< The rest after a reader error. */
    badge_scan_t  scan;        /**< The job's argument: it outlives the step that submits it. */
    uint16_t      job_seq;     /**< The sequence number of the scan the Active Object waits for. */
    bool          worker_up;   /**< The worker started. */
    bool          job_out;     /**< A scan is with the worker. */
    bool          resting;     /**< The retry timer runs. */
    bool          scanning;    /**< The owner wants the scanning on. */
    uint32_t      view;        /**< The history entry on the card, 0 for the newest. */
} AppAo;

static QEvtPtr     s_queue[KQP_AO_QUEUE_LEN];                            /**< Its event queue. */
static StackType_t s_stack[KQP_AO_STACK_BYTES / sizeof(StackType_t)];    /**< Its stack, in words; LVGL renders on it. */
static AppAo       s_ao;                                                 /**< The one instance. */

QActive * const AO_App = &s_ao.super;

static QState AppAo_initial(AppAo * const me, void const * const par);
static QState AppAo_running(AppAo * const me, QEvt const * const e);
static QState AppAo_waiting(AppAo * const me, QEvt const * const e);
static QState AppAo_drawing(AppAo * const me, QEvt const * const e);

void AppAo_ctor(void)
{
    AppAo * const me = &s_ao;

    QActive_ctor(&me->super, Q_STATE_CAST(&AppAo_initial));
    kscreen_ctor(&me->screen, &me->super, (enum_t)APP_TICK_SIG);
    QTimeEvt_ctorX(&me->retry, &me->super, (enum_t)APP_RETRY_SIG, 0U);
    (void)memset(&me->scan, 0, sizeof(me->scan));
    me->job_seq   = 0U;
    me->worker_up = false;
    me->job_out   = false;
    me->resting   = false;
    me->scanning  = true;
    me->view      = 0U;
    badge_log_clear();
}

void AppAo_start(void)
{
    /* A named task: an assert then names it instead of "AO". */
    QActive_setAttr(AO_App, TASK_NAME_ATTR, "badge_audit");
    /* KODE_PRIO_USER_GUI, the level the SDK keeps for a renderer: below every owner it asks. */
    QACTIVE_START(AO_App, KODE_PRIO_USER_GUI, s_queue, Q_DIM(s_queue),
                  s_stack, sizeof(s_stack), (void *)0);
}

/* ---- The Active Object's helpers ---- */

/**
 * @brief Hand the next scan to the worker, when the owner wants one and none is out.
 * @param me The Active Object.
 */
static void scan_next(AppAo * const me)
{
    kode_status_t rc;

    if ( me->worker_up && me->scanning && !me->job_out && !me->resting )
    {
        rc = kode_worker_submit(&me->worker, &badge_scan_job, &me->scan,
                                (enum_t)APP_SCAN_DONE_SIG, &me->job_seq);
        if ( KODE_OK == rc )
        {
            me->job_out = true;
        }
        else
        {
            KLOG_W(TAG, "the worker took no scan (status %d)", (int)rc);
        }
    }
}

/**
 * @brief Show the history entry the owner is looking at.
 * @param me The Active Object.
 */
static void show_view(AppAo const * const me)
{
    app_ui_show(badge_log_get(me->view), me->view, badge_log_count());
}

/**
 * @brief Show the status line for the scanning as it is now.
 * @param me The Active Object.
 */
static void show_status(AppAo const * const me)
{
    if ( me->scanning )
    {
        app_ui_status(APP_UI_SCANNING, NULL);
    }
    else
    {
        app_ui_status(APP_UI_PAUSED, NULL);
    }
}

/**
 * @brief Take a scan's result: record a badge, or say why the reader failed.
 * @param me   The Active Object.
 * @param done The worker's done event for the scan awaited.
 */
static void scan_done(AppAo * const me, KqpWorkerDoneEvt const * const done)
{
    const badge_log_entry_t *e;

    me->job_out = false;
    if ( KODE_OK == (kode_status_t)done->status )
    {
        if ( badge_log_add(me->scan.band, &me->scan.tag) )
        {
            e = badge_log_get(0U);
            KLOG_I(TAG, "%s (%s), UID %s: %s", me->scan.tag.name,
                   badge_class_band_name(me->scan.band), me->scan.tag.id, e->cls->verdict);
            /* A new badge goes on the card, whatever the owner was looking at. */
            me->view = 0U;
        }
        show_view(me);
        show_status(me);
    }
    else if ( KODE_IO_ERROR == (kode_status_t)done->status )
    {
        KLOG_W(TAG, "reader: %s", me->scan.error);
        app_ui_status(APP_UI_FAULT, me->scan.error);
        me->resting = true;
        QTimeEvt_armX(&me->retry, KQP_TICKS_PER_MS(APP_RETRY_MS), 0U);
    }
    else
    {
        /* No badge in this cycle: the status line already says it is scanning. */
    }
}

/**
 * @brief Act on a press of TOP, UP or DOWN; BOT is kodeOS's.
 * @param me The Active Object.
 * @param be The button's event.
 */
static void on_button(AppAo * const me, ButtonEvt const * const be)
{
    const uint32_t count = badge_log_count();

    if ( be->pressed )
    {
        if ( KODE_SYS_BUTTON_TOP == (uint32_t)be->id )
        {
            me->scanning = !me->scanning;
            show_status(me);
            scan_next(me);
        }
        else if ( (KODE_SYS_BUTTON_UP == (uint32_t)be->id) && ((me->view + 1U) < count) )
        {
            /* Up goes back in time, to an older badge. */
            me->view++;
            show_view(me);
        }
        else if ( (KODE_SYS_BUTTON_DOWN == (uint32_t)be->id) && (0U < me->view) )
        {
            me->view--;
            show_view(me);
        }
        else
        {
            /* BOT, or the end of the history: nothing to do. */
        }
    }
}

/**
 * @brief A tap on the glass: LVGL's click, on this Active Object's thread; the same as TOP.
 * @param e The click; its user data is the Active Object.
 */
static void tap_cb(lv_event_t *e)
{
    AppAo * const me = (AppAo *)lv_event_get_user_data(e);

    me->scanning = !me->scanning;
    show_status(me);
    scan_next(me);
}

/* ---- The states ---- */

/**
 * @brief The initial transition: subscribe, ask for the panel, start the worker and the first scan.
 * @param me  The Active Object.
 * @param par Unused.
 * @return The transition to AppAo_waiting.
 */
static QState AppAo_initial(AppAo * const me, void const * const par)
{
    kode_status_t rc;

    (void)par;
    /* Subscribe first; kscreen_start() subscribes to the panel and the touch feed before it asks. */
    QActive_subscribe(&me->super, BUTTON_EVT_SIG);
    kscreen_start(&me->screen);

    /* Here and not in a state: the initial transition runs once, and a worker starts once. */
    rc = kode_worker_start(&me->worker, &me->super, APP_WORKER_NAME, APP_WORKER_STACK_BYTES,
                           (uint8_t)KODE_PRIO_USER_LOW, (uint8_t)APP_WORKER_DEPTH);
    if ( KODE_OK != rc )
    {
        KLOG_E(TAG, "the worker did not start (status %d); no scans", (int)rc);
    }
    else
    {
        me->worker_up = true;
        /* The scanning runs whether or not a panel comes: the console logs every badge. */
        scan_next(me);
    }
    KLOG_I(TAG, "up; hold a badge to the Dot. TOP pauses, UP/DOWN browse the history");
    return Q_TRAN(&AppAo_waiting);
}

/**
 * @brief The parent of both states: the scanning and the buttons, with or without a picture.
 * @param me The Active Object.
 * @param e  The event.
 * @return Q_HANDLED() for what it handles, Q_SUPER() otherwise.
 */
static QState AppAo_running(AppAo * const me, QEvt const * const e)
{
    QState status;

    switch ( e->sig )
    {
        case APP_SCAN_DONE_SIG:
        {
            KqpWorkerDoneEvt const * const done = Q_EVT_CAST(KqpWorkerDoneEvt);

            /* The worker decides nothing: an answer to an older job is not the one awaited. */
            if ( me->job_seq == done->seq )
            {
                scan_done(me, done);
                scan_next(me);
            }
            status = Q_HANDLED();
            break;
        }
        case APP_RETRY_SIG:
        {
            me->resting = false;
            show_status(me);
            scan_next(me);
            status = Q_HANDLED();
            break;
        }
        case BUTTON_EVT_SIG:
        {
            on_button(me, Q_EVT_CAST(ButtonEvt));
            status = Q_HANDLED();
            break;
        }
        default:
        {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status;
}

/**
 * @brief Waiting for AO_Display's answer: LVGL is not started yet.
 * @param me The Active Object.
 * @param e  The event.
 * @return Q_HANDLED(), the transition to AppAo_drawing, or Q_SUPER().
 */
static QState AppAo_waiting(AppAo * const me, QEvt const * const e)
{
    QState status;

    switch ( e->sig )
    {
        case DISPLAY_STATE_SIG:
        {
            if ( KODE_OK == kscreen_open(&me->screen, Q_EVT_CAST(DisplayStateEvt)) )
            {
                status = Q_TRAN(&AppAo_drawing);
            }
            else
            {
                /* kscreen has said why. The scanning goes on, in the console's log. */
                status = Q_HANDLED();
            }
            break;
        }
        default:
        {
            status = Q_SUPER(&AppAo_running);
            break;
        }
    }
    return status;
}

/**
 * @brief Drawing: the screen is built and the render pass ticks.
 * @param me The Active Object.
 * @param e  The event.
 * @return Q_HANDLED() for what it handles, Q_SUPER() otherwise.
 */
static QState AppAo_drawing(AppAo * const me, QEvt const * const e)
{
    QState status;

    switch ( e->sig )
    {
        case Q_ENTRY_SIG:
        {
            app_ui_build(&tap_cb, me);
            (void)app_ui_check_glass();
            /* Badges read before the panel came up are on the card at once. */
            show_status(me);
            show_view(me);
            status = Q_HANDLED();
            break;
        }
        /* Always to kscreen_tick(), which re-arms the one-shot: a periodic tick
         * would fill the queue behind a slow frame, and a full queue asserts. */
        case APP_TICK_SIG:
        {
            kscreen_tick(&me->screen);
            status = Q_HANDLED();
            break;
        }
        case TOUCH_EVT_SIG:
        {
            kscreen_touch(&me->screen, Q_EVT_CAST(TouchEvt));
            status = Q_HANDLED();
            break;
        }
        case DISPLAY_REPAINT_SIG:
        {
            kscreen_repaint(&me->screen);
            status = Q_HANDLED();
            break;
        }
        default:
        {
            status = Q_SUPER(&AppAo_running);
            break;
        }
    }
    return status;
}
