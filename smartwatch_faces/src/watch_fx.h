/**
 * @file watch_fx.h
 * Snapshot-based transition effects used by watch_nav.c:
 * - shatter: a face flies apart when it is removed from the picker;
 * - ripple:  the picker ripples away when a face is selected;
 * - reveal:  an app opens in a circle growing from its launcher icon
 *            (and closes back into it);
 * - flip:    a picker card turns over into the edit screen (and back).
 *
 * Every effect draws snapshot images on an overlay on lv_layer_top(); the
 * widget trees involved are not redrawn while it plays. On by default,
 * WATCH_FX=0 falls back to LVGL's built-in screen load animations.
 */

#ifndef WATCH_FX_H
#define WATCH_FX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../smartwatch_faces.h"

typedef void (*watch_fx_done_cb_t)(void * user_data);

/** Work done in the effects' animation steps, outside the display render */
typedef struct {
    uint32_t frames;        /* animation steps */
    uint32_t work_ms;       /* total time in them (live mode: incl. re-rendering the sources) */
    uint32_t work_max_ms;   /* slowest step */
} watch_fx_stats_t;

/** Effects compiled in (LV_USE_SNAPSHOT) and not disabled with WATCH_FX=0 */
bool watch_fx_enabled(void);

/** The effects are compiled in (LV_USE_SNAPSHOT) */
bool watch_fx_available(void);

/** Re-render the effect sources every frame (see above) */
void watch_fx_set_live(bool en);
bool watch_fx_is_live(void);

void watch_fx_get_stats(watch_fx_stats_t * stats);
void watch_fx_reset_stats(void);

/** An effect is playing; navigation should wait */
bool watch_fx_busy(void);

/**
 * Break `obj` (e.g. a face_slot) into flying pieces from `origin` (screen
 * coordinates). `obj` is hidden while it plays and shown again at the end,
 * after `done` ran.
 */
void watch_fx_shatter(lv_obj_t * obj, lv_point_t origin, watch_fx_done_cb_t done, void * user_data);

/**
 * Ripple the active screen away from `origin` while it fades out. Load the
 * next screen right after calling this: it shows through as the ripple fades.
 */
void watch_fx_ripple(lv_point_t origin);

/**
 * Circular reveal of `screen` from `origin`. `done` runs at the end, when the
 * screen should be loaded. With `reverse`, `screen` (the one on display) is
 * captured first and collapses into `origin`: load the next screen right after
 * calling it.
 */
void watch_fx_reveal(lv_obj_t * screen, lv_point_t origin, bool reverse, watch_fx_done_cb_t done,
                     void * user_data);

/**
 * Flip `card` (on the active screen) over into `screen`; `done` runs at the
 * end, when `screen` should be loaded. With `reverse`, the active screen
 * flips back into `card` (on `screen`): load `screen` right after calling it.
 */
void watch_fx_flip(lv_obj_t * card, lv_obj_t * screen, bool reverse, watch_fx_done_cb_t done,
                   void * user_data);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WATCH_FX_H*/
