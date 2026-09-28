/**
 * @file watch_thumbs.c
 * Snapshot mode of the face picker and the face gallery, see watch_thumbs.h.
 *
 * Both screens are a "thumbnail set": a scroller full of round slots, each
 * holding a live face tree. In snapshot mode every slot shows its own
 * snapshot instead (even when two tiles show the same face, they are
 * snapshotted separately). Refresh policy, per set:
 * - entering the screen (screen load start): every slot (and the picker's
 *   background);
 * - once per second, while the scroller is not scrolling: the slots that are
 *   on screen (their hands move), and the picker's background;
 * - the scroller stopped scrolling for SCROLL_QUIET_MS: the slots on screen,
 *   so a slot that scrolled in is not stale.
 * Nothing is re-rendered while a scroller scrolls: that is the point.
 *
 * Depth effect (watch_depth.c, picker only): every card also gets a blurred
 * copy of its snapshot, made at half resolution right after the snapshot
 * (blurred content hides the lower resolution). watch_thumbs_set_depth()
 * then only scales, cross-fades and darkens images: no blur per frame.
 */

#include "watch_thumbs.h"
#include "watch_compat.h"

#include <stdlib.h>

#if WATCH_USE_SNAPSHOTS && LV_USE_SNAPSHOT

#define CARD_CNT          18
#define TILE_CNT          18
#define BLUR_BOX          150    /* blurred copy: half-res face (130) + room for the halo */
#define BLUR_RADIUS       12     /* in half-res pixels, ~24 px on screen */
#define DEPTH_SHRINK      56     /* scale lost at full depth, of 256 */
#define DEPTH_DIM         150    /* darkening at full depth, of 255 */
#define SCROLL_QUIET_MS   250    /* no refresh until the scroller is quiet this long */

typedef struct {
    lv_obj_t * card;      /* hidden = deleted face (picker) */
    lv_obj_t * slot;
    lv_obj_t * face;      /* the live face tree inside the slot */
    lv_obj_t * image;     /* shows the snapshot */
    lv_draw_buf_t * buf;
    lv_obj_t * blur_image;    /* depth effect: shows the blurred copy */
    lv_draw_buf_t * blur_buf;
    int32_t depth;            /* 0 (in focus) .. WATCH_DEPTH_MAX */
} thumb_t;

typedef struct {
    lv_obj_t * screen;
    lv_obj_t * scroller;
    thumb_t * thumbs;
    uint32_t cnt;
    bool depth;               /* keeps blurred copies for the depth effect */
    lv_timer_t * quiet_timer; /* fires once the scroller stopped scrolling */
    uint32_t last_scroll;
} thumb_set_t;

static thumb_t picker_thumbs[CARD_CNT];
static thumb_t gallery_thumbs[TILE_CNT];
static thumb_set_t picker_set;
static thumb_set_t gallery_set;
static thumb_set_t * const sets[] = {&picker_set, &gallery_set};
#define SET_CNT (sizeof(sets) / sizeof(sets[0]))

/* Picker background */
static lv_obj_t * bg_holder;
static lv_obj_t * bg_live;
static lv_obj_t * bg_frost;     /* blurs bg_live; only needed while taking the snapshot */
static lv_obj_t * bg_image;
static lv_draw_buf_t * bg_buf;

/* Off-screen helper that renders a snapshot half size and blurred */
static lv_obj_t * blur_holder;
static lv_obj_t * blur_src;
static bool blur_bufs_ready;    /* allocated the first time the depth effect is on */

static bool enabled;
static watch_thumbs_stats_t stats;

static void count_snapshot(uint32_t t0)
{
    uint32_t elapsed = lv_tick_elaps(t0);
    stats.snapshot_cnt++;
    stats.snapshot_ms += elapsed;
    if(elapsed > stats.snapshot_max_ms) stats.snapshot_max_ms = elapsed;
}

/* Render `target` into `buf` with `live` shown and `image` hidden, then
 * swap them back: the live tree is only drawn inside the snapshot. */
static void take(lv_obj_t * target, lv_obj_t * live, lv_obj_t * image, lv_draw_buf_t * buf)
{
    uint32_t t0 = lv_tick_get();

    watch_obj_set_hidden(live, false);
    watch_obj_set_hidden(image, true);
    lv_obj_update_layout(target);
    /* ARGB8888: in LVGL 10.0-dev a blurred layer drawn into an XRGB8888
     * snapshot comes out black */
    lv_snapshot_take_to_draw_buf(target, LV_COLOR_FORMAT_ARGB8888, buf);
    watch_obj_set_hidden(live, true);
    watch_obj_set_hidden(image, false);

    /* Same buffer, new pixels: drop any cached decode and redraw */
    lv_image_cache_drop(buf);
    lv_image_set_src(image, buf);
    lv_obj_invalidate(image);

    count_snapshot(t0);
}

/* Show a card at `depth`: images only, so this is cheap per frame */
static void apply_depth(thumb_t * t)
{
    int32_t d = t->depth;
    int32_t scale = 256 - d * DEPTH_SHRINK / WATCH_DEPTH_MAX;
    lv_opa_t dim = (lv_opa_t)(d * DEPTH_DIM / WATCH_DEPTH_MAX);
    /* Fully defocused at half depth: a long cross-fade reads as a glow */
    lv_opa_t blur_opa = (lv_opa_t)LV_MIN(255, d * 255 * 2 / WATCH_DEPTH_MAX);

    lv_image_set_scale(t->image, (uint32_t)scale);
    lv_image_set_scale(t->blur_image, (uint32_t)scale * 2);   /* half-res source */
    lv_obj_set_style_image_recolor_opa(t->image, dim, 0);
    lv_obj_set_style_image_recolor_opa(t->blur_image, dim, 0);
    lv_obj_set_style_image_opa(t->blur_image, blur_opa, 0);

    /* Skip the images that can't be seen */
    watch_obj_set_hidden(t->blur_image, blur_opa < LV_OPA_MIN);
    watch_obj_set_hidden(t->image, blur_opa >= LV_OPA_MAX);
}

/* Half-size, blurred copy of the card's snapshot */
static void make_blur(thumb_t * t)
{
    uint32_t t0 = lv_tick_get();

    lv_image_cache_drop(t->buf);
    lv_image_set_src(blur_src, t->buf);
    watch_obj_set_hidden(blur_holder, false);
    lv_obj_update_layout(blur_holder);
    lv_snapshot_take_to_draw_buf(blur_holder, LV_COLOR_FORMAT_ARGB8888, t->blur_buf);
    watch_obj_set_hidden(blur_holder, true);

    lv_image_cache_drop(t->blur_buf);
    lv_image_set_src(t->blur_image, t->blur_buf);
    lv_obj_invalidate(t->blur_image);

    count_snapshot(t0);
}

static void refresh_thumb(thumb_set_t * set, thumb_t * t)
{
    if(watch_obj_is_hidden(t->card)) return;   /* deleted face */
    if(set->depth) watch_obj_set_hidden(t->blur_image, true);
    take(t->slot, t->face, t->image, t->buf);
    if(set->depth) {
        make_blur(t);
        apply_depth(t);
    }
}

static void refresh_bg(void)
{
    watch_obj_set_hidden(bg_frost, false);
    take(bg_holder, bg_live, bg_image, bg_buf);
    watch_obj_set_hidden(bg_frost, true);
}

static bool on_screen(lv_obj_t * obj)
{
    lv_area_t a, screen, res;
    lv_obj_get_coords(obj, &a);
    lv_display_t * disp = lv_obj_get_display(obj);
    lv_area_set(&screen, 0, 0, lv_display_get_horizontal_resolution(disp) - 1,
                lv_display_get_vertical_resolution(disp) - 1);
    return lv_area_intersect(&res, &a, &screen);
}

static void refresh_visible(thumb_set_t * set)
{
    for(uint32_t i = 0; i < set->cnt; i++) {
        if(on_screen(set->thumbs[i].slot)) refresh_thumb(set, &set->thumbs[i]);
    }
}

static void refresh_all(thumb_set_t * set)
{
    if(set == &picker_set) refresh_bg();
    for(uint32_t i = 0; i < set->cnt; i++) refresh_thumb(set, &set->thumbs[i]);
}

static void screen_load_cb(lv_event_t * e)
{
    if(enabled) refresh_all(lv_event_get_user_data(e));
}

/* Any scroll (drag, throw, snap, programmatic) restarts the quiet period */
static void scroll_cb(lv_event_t * e)
{
    thumb_set_t * set = lv_event_get_user_data(e);
    set->last_scroll = lv_tick_get();
    if(!enabled) return;
    lv_timer_reset(set->quiet_timer);
    lv_timer_resume(set->quiet_timer);
}

static void quiet_timer_cb(lv_timer_t * t)
{
    lv_timer_pause(t);
    if(enabled) refresh_visible(lv_timer_get_user_data(t));
}

/* In snapshot mode the images are round already: the slots need no corner
 * clipping (which costs a mask per frame), and the depth effect's scaled-up
 * blurred copy may reach past the slot a little. */
static void style_slots(thumb_set_t * set)
{
    for(uint32_t i = 0; i < set->cnt; i++) {
        lv_obj_t * slot = set->thumbs[i].slot;
        if(enabled) lv_obj_set_style_clip_corner(slot, false, 0);
        else lv_obj_remove_local_style_prop(slot, LV_STYLE_CLIP_CORNER, 0);
        if(set->depth) watch_obj_set_overflow_visible(slot, enabled);
    }
}

/* Show either the snapshot images or the live trees */
static void apply_mode(thumb_set_t * set)
{
    for(uint32_t i = 0; i < set->cnt; i++) {
        thumb_t * t = &set->thumbs[i];
        watch_obj_set_hidden(t->face, enabled);
        watch_obj_set_hidden(t->image, !enabled);
        if(set->depth) {
            watch_obj_set_hidden(t->blur_image, true);
            if(enabled) apply_depth(t);
        }
    }
    if(set == &picker_set) {
        watch_obj_set_hidden(bg_live, enabled);
        watch_obj_set_hidden(bg_frost, enabled);
        watch_obj_set_hidden(bg_image, !enabled);
    }
}

static void add_thumb(thumb_t * t, lv_obj_t * card, lv_obj_t * slot)
{
    t->card = card;
    t->slot = slot;
    t->face = lv_obj_get_child(slot, 0);
    t->image = lv_image_create(slot);
    lv_obj_set_name(t->image, "face_thumb");
    lv_obj_center(t->image);
    t->buf = lv_snapshot_create_draw_buf(slot, LV_COLOR_FORMAT_ARGB8888);
    stats.buffer_bytes += t->buf->data_size;
}

static void init_set(thumb_set_t * set, lv_obj_t * screen, lv_obj_t * scroller, thumb_t * thumbs,
                     uint32_t cnt, bool depth)
{
    set->screen = screen;
    set->scroller = scroller;
    set->thumbs = thumbs;
    set->cnt = cnt;
    set->depth = depth;
    set->quiet_timer = lv_timer_create(quiet_timer_cb, SCROLL_QUIET_MS, set);
    lv_timer_pause(set->quiet_timer);
    lv_obj_add_event_cb(screen, screen_load_cb, LV_EVENT_SCREEN_LOAD_START, set);
    lv_obj_add_event_cb(scroller, scroll_cb, LV_EVENT_SCROLL, set);
}

static bool init_picker(void)
{
    lv_obj_t * carousel = lv_obj_find_by_name(screen_picker, "face_carousel");
    bg_holder = lv_obj_find_by_name(screen_picker, "picker_bg_holder");
    bg_live = lv_obj_find_by_name(screen_picker, "picker_bg");
    bg_frost = lv_obj_find_by_name(screen_picker, "picker_bg_frost");
    if(carousel == NULL || bg_holder == NULL || bg_live == NULL || bg_frost == NULL) return false;

    lv_obj_update_layout(screen_picker);

    for(int i = 0; i < CARD_CNT; i++) {
        thumb_t * t = &picker_thumbs[i];
        lv_obj_t * card = lv_obj_get_child(carousel, i);
        add_thumb(t, card, lv_obj_find_by_name(card, "face_slot"));

        t->blur_image = lv_image_create(t->slot);
        lv_obj_set_name(t->blur_image, "face_thumb_blur");
        lv_obj_center(t->blur_image);
        lv_obj_set_style_image_recolor(t->image, lv_color_black(), 0);
        lv_obj_set_style_image_recolor(t->blur_image, lv_color_black(), 0);
        watch_obj_set_hidden(t->blur_image, true);
    }

    /* Blur helper: the source image carries the blur, the plain holder is
     * what gets snapshotted (a snapshot skips the target's own blur) */
    blur_holder = lv_obj_create(screen_picker);
    lv_obj_remove_style_all(blur_holder);
    lv_obj_set_size(blur_holder, BLUR_BOX, BLUR_BOX);
    watch_obj_set_clickable(blur_holder, false);
    watch_obj_set_hidden(blur_holder, true);
    blur_src = lv_image_create(blur_holder);
    lv_obj_center(blur_src);
    lv_image_set_scale(blur_src, 128);
    lv_obj_set_style_blur_radius(blur_src, BLUR_RADIUS, 0);
    lv_obj_update_layout(blur_holder);

    bg_image = lv_image_create(bg_holder);
    lv_obj_set_name(bg_image, "picker_bg_snap");
    lv_obj_center(bg_image);
    bg_buf = lv_snapshot_create_draw_buf(bg_holder, LV_COLOR_FORMAT_ARGB8888);
    stats.buffer_bytes += bg_buf->data_size;

    /* The blurred copies are only made while the depth effect is on */
    init_set(&picker_set, screen_picker, carousel, picker_thumbs, CARD_CNT, false);
    return true;
}

static void init_gallery(void)
{
    if(screen_face_gallery == NULL) return;
    lv_obj_t * grid = lv_obj_find_by_name(screen_face_gallery, "face_grid");
    if(grid == NULL) return;

    lv_obj_update_layout(screen_face_gallery);

    uint32_t cnt = 0;
    for(uint32_t i = 0; i < TILE_CNT; i++) {
        char name[16];
        lv_snprintf(name, sizeof(name), "tile_%u", (unsigned)i);
        lv_obj_t * tile = lv_obj_find_by_name(grid, name);
        if(tile == NULL) break;
        add_thumb(&gallery_thumbs[cnt++], tile, tile);   /* the tile is its own slot */
    }

    init_set(&gallery_set, screen_face_gallery, grid, gallery_thumbs, cnt, false);
}

void watch_thumbs_init(void)
{
    if(!init_picker()) return;
    init_gallery();

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
    for(uint32_t i = 0; i < SET_CNT; i++) {
        thumb_set_t * set = sets[i];
        if(set->screen == NULL) continue;
        style_slots(set);
        if(enabled) refresh_all(set);
        apply_mode(set);
    }
}

bool watch_thumbs_is_enabled(void)
{
    return enabled;
}

void watch_thumbs_tick(void)
{
    if(!enabled) return;
    for(uint32_t i = 0; i < SET_CNT; i++) {
        thumb_set_t * set = sets[i];
        if(set->screen == NULL || lv_screen_active() != set->screen) continue;
        if(lv_tick_elaps(set->last_scroll) < SCROLL_QUIET_MS) continue;
        if(set == &picker_set) refresh_bg();
        refresh_visible(set);
    }
}

void watch_thumbs_set_depth_mode(bool en)
{
    if(bg_image == NULL || en == picker_set.depth) return;

    if(en && !blur_bufs_ready) {
        for(int i = 0; i < CARD_CNT; i++) {
            picker_thumbs[i].blur_buf = lv_snapshot_create_draw_buf(blur_holder, LV_COLOR_FORMAT_ARGB8888);
            stats.buffer_bytes += picker_thumbs[i].blur_buf->data_size;
        }
        blur_bufs_ready = true;
    }

    picker_set.depth = en;
    style_slots(&picker_set);
    for(int i = 0; i < CARD_CNT; i++) {
        thumb_t * t = &picker_thumbs[i];
        if(en && enabled && !watch_obj_is_hidden(t->card)) make_blur(t);
        if(!en) watch_obj_set_hidden(t->blur_image, true);
        if(enabled) apply_depth(t);
    }
}

void watch_thumbs_set_depth(uint32_t card, int32_t depth)
{
    if(card >= CARD_CNT || bg_image == NULL) return;
    thumb_t * t = &picker_thumbs[card];
    depth = LV_CLAMP(0, depth, WATCH_DEPTH_MAX);
    if(depth == t->depth) return;
    t->depth = depth;
    if(enabled) apply_depth(t);
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
void watch_thumbs_set_depth_mode(bool en) { LV_UNUSED(en); }
void watch_thumbs_set_depth(uint32_t card, int32_t depth) { LV_UNUSED(card); LV_UNUSED(depth); }
void watch_thumbs_tick(void) {}
void watch_thumbs_get_stats(watch_thumbs_stats_t * s) { lv_memzero(s, sizeof(*s)); }
void watch_thumbs_reset_stats(void) {}

#endif
