/**
 * @file watch_thumbs.c
 * Snapshot mode of the face picker, see watch_thumbs.h.
 *
 * Refresh policy:
 * - entering the picker (screen load start): background and every card;
 * - once per second, while the carousel is not scrolling: the background
 *   and the cards that are on screen (their hands move);
 * - the carousel stopped scrolling for SCROLL_QUIET_MS: the cards on screen,
 *   so a card that scrolled in is not stale.
 * Nothing is re-rendered while the carousel scrolls: that is the point.
 */

#include "watch_thumbs.h"
#include "watch_compat.h"

#include <stdlib.h>

#if WATCH_USE_SNAPSHOTS && LV_USE_SNAPSHOT

#define CARD_CNT          6
#define SCROLL_QUIET_MS   250    /* no refresh until the carousel is quiet this long */

typedef struct {
    lv_obj_t * card;
    lv_obj_t * slot;
    lv_obj_t * face;      /* the live face tree inside the slot */
    lv_obj_t * image;     /* shows the snapshot */
    lv_draw_buf_t * buf;
} thumb_t;

static thumb_t thumbs[CARD_CNT];
static lv_obj_t * bg_holder;
static lv_obj_t * bg_live;
static lv_obj_t * bg_image;
static lv_draw_buf_t * bg_buf;
static lv_obj_t * carousel;

static lv_timer_t * quiet_timer;   /* fires once the carousel stopped scrolling */
static bool enabled;
static uint32_t last_scroll;
static watch_thumbs_stats_t stats;

/* Render `target` into `buf` with `live` shown and `image` hidden, then
 * swap them back: the live tree is only drawn inside the snapshot. */
static void take(lv_obj_t * target, lv_obj_t * live, lv_obj_t * image, lv_draw_buf_t * buf,
                 lv_color_format_t cf)
{
    uint32_t t0 = lv_tick_get();

    watch_obj_set_hidden(live, false);
    watch_obj_set_hidden(image, true);
    lv_obj_update_layout(target);
    lv_snapshot_take_to_draw_buf(target, cf, buf);
    watch_obj_set_hidden(live, true);
    watch_obj_set_hidden(image, false);

    /* Same buffer, new pixels: drop any cached decode and redraw */
    lv_image_cache_drop(buf);
    lv_image_set_src(image, buf);
    lv_obj_invalidate(image);

    uint32_t elapsed = lv_tick_elaps(t0);
    stats.snapshot_cnt++;
    stats.snapshot_ms += elapsed;
    if(elapsed > stats.snapshot_max_ms) stats.snapshot_max_ms = elapsed;
}

static void refresh_card(thumb_t * t)
{
    if(watch_obj_is_hidden(t->card)) return;   /* deleted face */
    take(t->slot, t->face, t->image, t->buf, LV_COLOR_FORMAT_ARGB8888);
}

/* ARGB8888: in LVGL 10.0-dev a blurred layer drawn into an XRGB8888
 * snapshot comes out black */
static void refresh_bg(void)
{
    take(bg_holder, bg_live, bg_image, bg_buf, LV_COLOR_FORMAT_ARGB8888);
}

static bool card_on_screen(const thumb_t * t)
{
    lv_area_t a, screen, res;
    lv_obj_get_coords(t->slot, &a);
    lv_display_t * disp = lv_obj_get_display(t->slot);
    lv_area_set(&screen, 0, 0, lv_display_get_horizontal_resolution(disp) - 1,
                lv_display_get_vertical_resolution(disp) - 1);
    return lv_area_intersect(&res, &a, &screen);
}

static void refresh_visible_cards(void)
{
    for(int i = 0; i < CARD_CNT; i++) {
        if(card_on_screen(&thumbs[i])) refresh_card(&thumbs[i]);
    }
}

static void refresh_all(void)
{
    refresh_bg();
    for(int i = 0; i < CARD_CNT; i++) refresh_card(&thumbs[i]);
}

static void picker_load_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(enabled) refresh_all();
}

/* Any scroll (drag, throw, snap, programmatic) restarts the quiet period */
static void carousel_scroll_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    last_scroll = lv_tick_get();
    if(!enabled) return;
    lv_timer_reset(quiet_timer);
    lv_timer_resume(quiet_timer);
}

static void quiet_timer_cb(lv_timer_t * t)
{
    lv_timer_pause(t);
    if(enabled) refresh_visible_cards();
}

/* Show either the snapshot images or the live trees */
static void apply_mode(void)
{
    for(int i = 0; i < CARD_CNT; i++) {
        watch_obj_set_hidden(thumbs[i].face, enabled);
        watch_obj_set_hidden(thumbs[i].image, !enabled);
    }
    watch_obj_set_hidden(bg_live, enabled);
    watch_obj_set_hidden(bg_image, !enabled);
}

void watch_thumbs_init(void)
{
    carousel = lv_obj_find_by_name(screen_picker, "face_carousel");
    bg_holder = lv_obj_find_by_name(screen_picker, "picker_bg_holder");
    bg_live = lv_obj_find_by_name(screen_picker, "picker_bg");
    if(carousel == NULL || bg_holder == NULL || bg_live == NULL) return;

    lv_obj_update_layout(screen_picker);

    for(int i = 0; i < CARD_CNT; i++) {
        thumb_t * t = &thumbs[i];
        t->card = lv_obj_get_child(carousel, i);
        t->slot = lv_obj_find_by_name(t->card, "face_slot");
        t->face = lv_obj_get_child(t->slot, 0);
        t->image = lv_image_create(t->slot);
        lv_obj_set_name(t->image, "face_thumb");
        lv_obj_center(t->image);
        t->buf = lv_snapshot_create_draw_buf(t->slot, LV_COLOR_FORMAT_ARGB8888);
        stats.buffer_bytes += t->buf->data_size;
    }

    bg_image = lv_image_create(bg_holder);
    lv_obj_set_name(bg_image, "picker_bg_snap");
    lv_obj_center(bg_image);
    bg_buf = lv_snapshot_create_draw_buf(bg_holder, LV_COLOR_FORMAT_ARGB8888);
    stats.buffer_bytes += bg_buf->data_size;

    lv_obj_add_event_cb(screen_picker, picker_load_cb, LV_EVENT_SCREEN_LOAD_START, NULL);
    lv_obj_add_event_cb(carousel, carousel_scroll_cb, LV_EVENT_SCROLL, NULL);
    quiet_timer = lv_timer_create(quiet_timer_cb, SCROLL_QUIET_MS, NULL);
    lv_timer_pause(quiet_timer);

    const char * env = getenv("WATCH_SNAPSHOTS");
    watch_thumbs_set_enabled(env == NULL || env[0] != '0');
}

bool watch_thumbs_available(void)
{
    return true;
}

void watch_thumbs_set_enabled(bool en)
{
    if(bg_image == NULL) return;   /* not initialized */
    enabled = en;
    if(enabled) refresh_all();
    apply_mode();
}

bool watch_thumbs_is_enabled(void)
{
    return enabled;
}

void watch_thumbs_tick(void)
{
    if(!enabled || lv_screen_active() != screen_picker) return;
    if(lv_tick_elaps(last_scroll) < SCROLL_QUIET_MS) return;

    refresh_bg();
    refresh_visible_cards();
}

void watch_thumbs_get_stats(watch_thumbs_stats_t * s)
{
    *s = stats;
}

void watch_thumbs_reset_stats(void)
{
    uint32_t bytes = stats.buffer_bytes;
    lv_memzero(&stats, sizeof(stats));
    stats.buffer_bytes = bytes;
}

#else /* snapshot mode not compiled in */

void watch_thumbs_init(void) {}
bool watch_thumbs_available(void) { return false; }
void watch_thumbs_set_enabled(bool en) { LV_UNUSED(en); }
bool watch_thumbs_is_enabled(void) { return false; }
void watch_thumbs_tick(void) {}
void watch_thumbs_get_stats(watch_thumbs_stats_t * s) { lv_memzero(s, sizeof(*s)); }
void watch_thumbs_reset_stats(void) {}

#endif
