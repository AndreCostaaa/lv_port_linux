/**
 * @file watch_fx.c
 * Snapshot-based transition effects, see watch_fx.h. Shatter and ripple are
 * adapted from the ripple / shatter demo in lv_port_linux's main.c.
 *
 * Each effect: take the snapshot(s), build an overlay of images on
 * lv_layer_top(), drive it with one lv_anim (progress 0..FX_P_MAX), then
 * call `done`, delete the overlay and free the snapshots.
 *
 * Debugging:
 * - WATCH_FX_SLOW=<n>  play every effect n times slower (the motion is a
 *                      function of the progress, so it follows the same path);
 * - WATCH_FX_DEBUG=1   give every piece (shatter tile, ripple band, reveal
 *                      circle, flip image) its own coloured border.
 */

#include "watch_fx.h"
#include "watch_compat.h"

#include <stdlib.h>

#if LV_USE_SNAPSHOT

#define FX_P_MAX            1024

/* Shatter */
#define SHATTER_GRID        7       /* tiles per side */
#define SHATTER_DURATION    1000    /* ms */
#define SHATTER_BLAST       55      /* % - outward speed vs. distance from the impact point */
#define SHATTER_LIFT        90      /* initial upward kick, 1/16 px per frame */
#define SHATTER_GRAVITY     6       /* 1/16 px per frame per frame */
#define SHATTER_SPIN        60      /* max spin, 0.1 deg per frame */
#define FRAME_MS            16

/* Ripple */
#define RIPPLE_SLICE_H      4       /* px per horizontal band */
#define RIPPLE_DURATION     900     /* ms */
#define RIPPLE_AMPLITUDE    24      /* px, max horizontal displacement */
#define RIPPLE_SPEED        700     /* px/s, wavefront speed */
#define RIPPLE_WAVELENGTH   60      /* px between two crests */
#define RIPPLE_TRAIL        200     /* px the wave keeps rippling behind the front */

/* Reveal and flip */
#define REVEAL_DURATION     450     /* ms */
#define FLIP_DURATION       520     /* ms */
#define FLIP_DIM            220     /* backdrop darkness at the end of the flip */
#define FLIP_MIN_SCALE      8       /* narrower than this: hide instead (tiny scales overflow the transform) */

/**********************
 * Common scaffolding
 **********************/

typedef struct fx_t fx_t;
typedef void (*fx_step_cb_t)(fx_t * fx, int32_t p);

struct fx_t {
    lv_obj_t * overlay;
    lv_draw_buf_t * snap_a;
    lv_draw_buf_t * snap_b;
    lv_obj_t * src_a;             /* what snap_a / snap_b show; re-rendered in live mode */
    lv_obj_t * src_b;
    bool slot_a;                  /* src_a is a face slot: render its live face */
    bool slot_b;
    lv_obj_t * restore;           /* hidden while playing, shown at the end */
    fx_step_cb_t step;
    watch_fx_done_cb_t done;
    void * done_ud;
    void * data;                  /* effect specific, lv_free()-d at the end */
};

static bool busy;
static bool live_mode;
static watch_fx_stats_t stats;
static bool env_read;
static uint32_t slow_factor = 1;
static bool debug_borders;
static uint32_t rnd_state = 0x2545F491u;

static int32_t rnd_range(int32_t min, int32_t max)
{
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;
    if(max <= min) return min;
    return min + (int32_t)(rnd_state % (uint32_t)(max - min + 1));
}

static void read_env(void)
{
    if(env_read) return;
    env_read = true;
    const char * slow = getenv("WATCH_FX_SLOW");
    if(slow && atoi(slow) > 1) slow_factor = (uint32_t)atoi(slow);
    const char * dbg = getenv("WATCH_FX_DEBUG");
    debug_borders = dbg && dbg[0] != '0';
    const char * live = getenv("WATCH_FX_LIVE");
    live_mode = live && live[0] != '0';
}

/* WATCH_FX_DEBUG: a border in a colour of its own, drawn over the content
 * (border_post), so each window onto the snapshot is visible */
static void debug_border(lv_obj_t * obj, uint32_t index, int32_t width)
{
    if(!debug_borders) return;
    lv_obj_set_style_border_width(obj, width, 0);
    lv_obj_set_style_border_color(obj, lv_color_hsv_to_rgb((uint16_t)((index * 47) % 360), 90, 100), 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_post(obj, true, 0);
}

static int32_t isqrt(int32_t v)
{
    int32_t r = 0;
    while((r + 1) * (r + 1) <= v) r++;
    return r;
}

static lv_draw_buf_t * fx_snapshot(lv_obj_t * obj)
{
    lv_obj_update_layout(obj);
    return lv_snapshot_take(obj, LV_COLOR_FORMAT_ARGB8888);
}

/* Full-screen overlay that swallows input while the effect plays */
static lv_obj_t * fx_overlay_create(void)
{
    lv_obj_t * o = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, lv_pct(100), lv_pct(100));
    watch_obj_set_scrollable(o, false);
    watch_obj_set_clickable(o, true);
    return o;
}

static lv_obj_t * fx_image_create(lv_obj_t * parent, lv_draw_buf_t * snap)
{
    lv_obj_t * img = lv_image_create(parent);
    lv_image_set_src(img, snap);
    return img;
}

/* A clipping window onto `snap`: shows the (x, y, w, h) region of it */
static lv_obj_t * fx_slice_create(lv_obj_t * parent, lv_draw_buf_t * snap, int32_t x, int32_t y, int32_t w,
                               int32_t h)
{
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    watch_obj_set_scrollable(cont, false);
    watch_obj_set_clickable(cont, false);
    lv_obj_set_size(cont, w, h);
    lv_obj_t * img = fx_image_create(cont, snap);
    lv_obj_set_pos(img, -x, -y);
    return cont;
}

/* Render `src` into `buf` as it looks live. The effect hides the objects it
 * replaces, so they are shown just for the render. For a face slot, render
 * the live face (child 0) and not the snapshot thumbnails next to it. */
static void render_live(lv_obj_t * src, lv_draw_buf_t * buf, bool slot)
{
    bool was_hidden = watch_obj_is_hidden(src);
    watch_obj_set_hidden(src, false);

    uint32_t cnt = slot ? lv_obj_get_child_count(src) : 0;
    uint32_t hidden_mask = 0;
    for(uint32_t i = 0; i < cnt && i < 32; i++) {
        lv_obj_t * child = lv_obj_get_child(src, (int32_t)i);
        if(watch_obj_is_hidden(child)) hidden_mask |= 1u << i;
        watch_obj_set_hidden(child, i != 0);
    }

    lv_snapshot_take_to_draw_buf(src, LV_COLOR_FORMAT_ARGB8888, buf);
    lv_image_cache_drop(buf);

    for(uint32_t i = 0; i < cnt && i < 32; i++) {
        watch_obj_set_hidden(lv_obj_get_child(src, (int32_t)i), (hidden_mask >> i) & 1u);
    }
    watch_obj_set_hidden(src, was_hidden);
}

static void anim_exec_cb(void * var, int32_t v)
{
    fx_t * fx = var;
    uint32_t t0 = lv_tick_get();
    if(live_mode) {
        if(fx->src_a && fx->snap_a) render_live(fx->src_a, fx->snap_a, fx->slot_a);
        if(fx->src_b && fx->snap_b) render_live(fx->src_b, fx->snap_b, fx->slot_b);
        lv_obj_invalidate(fx->overlay);
    }
    fx->step(fx, v);

    uint32_t elapsed = lv_tick_elaps(t0);
    stats.frames++;
    stats.work_ms += elapsed;
    if(elapsed > stats.work_max_ms) stats.work_max_ms = elapsed;
}

static void anim_completed_cb(lv_anim_t * a)
{
    fx_t * fx = lv_anim_get_user_data(a);
    fx->step(fx, FX_P_MAX);

    /* `done` first (it usually loads a screen), so the overlay is only
     * removed once the next screen is in place */
    if(fx->done) fx->done(fx->done_ud);
    if(fx->restore) watch_obj_set_hidden(fx->restore, false);
    lv_obj_delete(fx->overlay);
    /* Drop them from the image cache too: a later snapshot can get the same
     * address and would otherwise be drawn from a stale cache entry */
    if(fx->snap_a) {
        lv_image_cache_drop(fx->snap_a);
        lv_draw_buf_destroy(fx->snap_a);
    }
    if(fx->snap_b) {
        lv_image_cache_drop(fx->snap_b);
        lv_draw_buf_destroy(fx->snap_b);
    }
    lv_free(fx->data);
    lv_free(fx);
    busy = false;
}

static fx_t * fx_create(fx_step_cb_t step, watch_fx_done_cb_t done, void * done_ud)
{
    fx_t * fx = lv_malloc_zeroed(sizeof(fx_t));
    LV_ASSERT_MALLOC(fx);
    fx->step = step;
    fx->done = done;
    fx->done_ud = done_ud;
    fx->overlay = fx_overlay_create();
    read_env();
    busy = true;
    return fx;
}

static void fx_start(fx_t * fx, uint32_t duration, lv_anim_path_cb_t path)
{
    fx->step(fx, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fx);
    lv_anim_set_user_data(&a, fx);
    lv_anim_set_exec_cb(&a, anim_exec_cb);
    lv_anim_set_values(&a, 0, FX_P_MAX);
    lv_anim_set_duration(&a, duration * slow_factor);
    lv_anim_set_path_cb(&a, path);
    lv_anim_set_completed_cb(&a, anim_completed_cb);
    lv_anim_start(&a);
}

/* Couldn't start (no memory for a snapshot): skip straight to the end */
static void fx_skip(watch_fx_done_cb_t done, void * user_data)
{
    if(done) done(user_data);
}

/**********************
 * Shatter
 **********************/

typedef struct {
    lv_obj_t * cont;
    int32_t x0, y0;       /* start position, px */
    int32_t vx, vy;       /* velocity, 1/16 px per frame */
    int32_t vrot;         /* spin, 0.1 deg per frame */
} tile_t;

typedef struct {
    tile_t tiles[SHATTER_GRID * SHATTER_GRID];
    uint32_t cnt;
} shatter_t;

static void shatter_step(fx_t * fx, int32_t p)
{
    shatter_t * sh = fx->data;
    /* Closed-form ballistics, in frames since the start */
    int32_t t = p * SHATTER_DURATION / FX_P_MAX / FRAME_MS;
    int32_t scale = LV_SCALE_NONE - p * 90 / FX_P_MAX;
    lv_opa_t opa = p < FX_P_MAX / 2 ? LV_OPA_COVER : (lv_opa_t)(255 - (p - FX_P_MAX / 2) * 255 / (FX_P_MAX / 2));

    for(uint32_t i = 0; i < sh->cnt; i++) {
        tile_t * tl = &sh->tiles[i];
        int32_t x = tl->x0 + (tl->vx * t) / 16;
        int32_t y = tl->y0 + (tl->vy * t + SHATTER_GRAVITY * t * t / 2) / 16;
        lv_obj_set_pos(tl->cont, x, y);
        lv_obj_set_style_transform_rotation(tl->cont, tl->vrot * t, 0);
        lv_obj_set_style_transform_scale(tl->cont, scale, 0);
        lv_obj_set_style_opa(tl->cont, opa, 0);
    }
}

void watch_fx_shatter(lv_obj_t * obj, lv_point_t origin, watch_fx_done_cb_t done, void * user_data)
{
    lv_draw_buf_t * snap = fx_snapshot(obj);
    if(snap == NULL) {
        fx_skip(done, user_data);
        return;
    }

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int32_t w = lv_area_get_width(&a);
    int32_t h = lv_area_get_height(&a);
    int32_t tw = (w + SHATTER_GRID - 1) / SHATTER_GRID;
    int32_t th = (h + SHATTER_GRID - 1) / SHATTER_GRID;
    int32_t r = LV_MIN(w, h) / 2;

    fx_t * fx = fx_create(shatter_step, done, user_data);
    fx->snap_a = snap;
    fx->src_a = obj;
    fx->slot_a = true;
    fx->restore = obj;
    shatter_t * sh = lv_malloc_zeroed(sizeof(shatter_t));
    LV_ASSERT_MALLOC(sh);
    fx->data = sh;

    for(int32_t row = 0; row < SHATTER_GRID; row++) {
        for(int32_t col = 0; col < SHATTER_GRID; col++) {
            int32_t tx = col * tw;
            int32_t ty = row * th;
            int32_t cw = LV_MIN(tw, w - tx);
            int32_t ch = LV_MIN(th, h - ty);
            int32_t cx = tx + cw / 2 - w / 2;
            int32_t cy = ty + ch / 2 - h / 2;
            /* The faces are round: tiles fully outside the disc are empty */
            if(cx * cx + cy * cy > (r + tw / 2) * (r + tw / 2)) continue;

            tile_t * tl = &sh->tiles[sh->cnt++];
            tl->cont = fx_slice_create(fx->overlay, snap, tx, ty, cw, ch);
            debug_border(tl->cont, sh->cnt, 2);
            lv_obj_set_style_transform_pivot_x(tl->cont, cw / 2, 0);
            lv_obj_set_style_transform_pivot_y(tl->cont, ch / 2, 0);
            tl->x0 = a.x1 + tx;
            tl->y0 = a.y1 + ty;

            /* Away from the impact point, with a bit of chaos */
            int32_t dx = tl->x0 + cw / 2 - origin.x;
            int32_t dy = tl->y0 + ch / 2 - origin.y;
            tl->vx = dx * SHATTER_BLAST / 100 + rnd_range(-24, 24);
            tl->vy = dy * SHATTER_BLAST / 100 - SHATTER_LIFT + rnd_range(-24, 24);
            tl->vrot = rnd_range(-SHATTER_SPIN, SHATTER_SPIN);
        }
    }

    watch_obj_set_hidden(obj, true);
    fx_start(fx, SHATTER_DURATION, lv_anim_path_linear);
}

/**********************
 * Ripple
 **********************/

typedef struct {
    uint32_t cnt;
    int32_t origin_y;
    lv_obj_t * img[];     /* the image inside each band */
} ripple_t;

static void ripple_step(fx_t * fx, int32_t p)
{
    ripple_t * rp = fx->data;
    int32_t elapsed = p * RIPPLE_DURATION / FX_P_MAX;
    int32_t front = elapsed * RIPPLE_SPEED / 1000;
    /* Fade out over the second two thirds, the next screen shows through */
    int32_t fade_p = LV_CLAMP(0, (p - FX_P_MAX / 3) * 3 / 2, FX_P_MAX);
    lv_opa_t opa = (lv_opa_t)(255 - fade_p * 255 / FX_P_MAX);

    for(uint32_t i = 0; i < rp->cnt; i++) {
        int32_t y = (int32_t)i * RIPPLE_SLICE_H + RIPPLE_SLICE_H / 2;
        int32_t dist = LV_ABS(y - rp->origin_y);
        int32_t dx = 0;
        if(dist <= front) {
            int32_t behind = front - dist;
            int32_t trail = 256 - behind * 256 / RIPPLE_TRAIL;
            if(trail > 0) {
                int32_t angle = (behind * 360 / RIPPLE_WAVELENGTH) % 360;
                dx = RIPPLE_AMPLITUDE * trail / 256 * lv_trigo_sin((int16_t)angle) / LV_TRIGO_SIN_MAX;
            }
        }
        /* Only the image moves; the band's window stays put */
        lv_obj_set_x(rp->img[i], dx);
        lv_obj_set_style_image_opa(rp->img[i], opa, 0);
    }
}

void watch_fx_ripple(lv_point_t origin)
{
    lv_obj_t * screen = lv_screen_active();
    lv_draw_buf_t * snap = fx_snapshot(screen);
    if(snap == NULL) return;

    int32_t w = lv_obj_get_width(screen);
    int32_t h = lv_obj_get_height(screen);
    uint32_t cnt = (uint32_t)((h + RIPPLE_SLICE_H - 1) / RIPPLE_SLICE_H);

    fx_t * fx = fx_create(ripple_step, NULL, NULL);
    fx->snap_a = snap;
    fx->src_a = screen;
    ripple_t * rp = lv_malloc_zeroed(sizeof(ripple_t) + cnt * sizeof(lv_obj_t *));
    LV_ASSERT_MALLOC(rp);
    fx->data = rp;
    rp->cnt = cnt;
    rp->origin_y = origin.y;

    for(uint32_t i = 0; i < cnt; i++) {
        int32_t sy = (int32_t)i * RIPPLE_SLICE_H;
        lv_obj_t * band = fx_slice_create(fx->overlay, snap, 0, sy, w, LV_MIN(RIPPLE_SLICE_H, h - sy));
        lv_obj_set_y(band, sy);
        debug_border(band, i, 1);
        rp->img[i] = lv_obj_get_child(band, 0);
    }

    fx_start(fx, RIPPLE_DURATION, lv_anim_path_linear);
}

/**********************
 * Circular reveal
 **********************/

typedef struct {
    lv_obj_t * circle;
    lv_obj_t * img;
    int32_t cx, cy, r_max;
    bool reverse;
} reveal_t;

static void reveal_step(fx_t * fx, int32_t p)
{
    reveal_t * rv = fx->data;
    int32_t r = rv->r_max * (rv->reverse ? FX_P_MAX - p : p) / FX_P_MAX;
    lv_obj_set_pos(rv->circle, rv->cx - r, rv->cy - r);
    lv_obj_set_size(rv->circle, 2 * r, 2 * r);
    /* The image stays aligned to the screen while its window grows */
    lv_obj_set_pos(rv->img, r - rv->cx, r - rv->cy);
}

void watch_fx_reveal(lv_obj_t * screen, lv_point_t origin, bool reverse, watch_fx_done_cb_t done,
                     void * user_data)
{
    lv_draw_buf_t * snap = fx_snapshot(screen);
    if(snap == NULL) {
        fx_skip(done, user_data);
        return;
    }

    int32_t w = lv_obj_get_width(screen);
    int32_t h = lv_obj_get_height(screen);

    fx_t * fx = fx_create(reveal_step, done, user_data);
    fx->snap_a = snap;
    fx->src_a = screen;
    reveal_t * rv = lv_malloc_zeroed(sizeof(reveal_t));
    LV_ASSERT_MALLOC(rv);
    fx->data = rv;
    rv->cx = origin.x;
    rv->cy = origin.y;
    rv->reverse = reverse;
    /* Far enough to cover the farthest corner */
    int32_t fx_ = LV_MAX(origin.x, w - origin.x);
    int32_t fy_ = LV_MAX(origin.y, h - origin.y);
    rv->r_max = isqrt(fx_ * fx_ + fy_ * fy_) + 1;

    rv->circle = lv_obj_create(fx->overlay);
    lv_obj_remove_style_all(rv->circle);
    watch_obj_set_scrollable(rv->circle, false);
    watch_obj_set_clickable(rv->circle, false);
    lv_obj_set_style_radius(rv->circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(rv->circle, true, 0);
    rv->img = fx_image_create(rv->circle, snap);
    debug_border(rv->circle, 0, 3);

    fx_start(fx, REVEAL_DURATION, lv_anim_path_ease_in_out);
}

/**********************
 * Card flip
 **********************/

typedef struct {
    lv_obj_t * card_img;      /* the card, turning away / back */
    lv_obj_t * screen_img;    /* the screen on the other side */
    int32_t card_w;
    int32_t screen_w;
    bool reverse;
} flip_t;

static void flip_step(fx_t * fx, int32_t p)
{
    flip_t * fl = fx->data;
    int32_t q = fl->reverse ? FX_P_MAX - p : p;      /* 0 = card, FX_P_MAX = screen */
    int32_t half = FX_P_MAX / 2;
    int32_t card_scale = fl->card_w * LV_SCALE_NONE / fl->screen_w;   /* screen image at card size */

    if(q < half) {
        /* The card turns edge-on */
        int32_t sx = LV_SCALE_NONE * (half - q) / half;
        lv_image_set_scale_x(fl->card_img, (uint32_t)LV_MAX(FLIP_MIN_SCALE, sx));
        watch_obj_set_hidden(fl->card_img, sx < FLIP_MIN_SCALE);
        watch_obj_set_hidden(fl->screen_img, true);
    }
    else {
        /* The other side turns towards us and grows to full screen */
        int32_t k = q - half;
        int32_t s = card_scale + (LV_SCALE_NONE - card_scale) * k / half;
        int32_t sx = s * k / half;
        lv_image_set_scale_x(fl->screen_img, (uint32_t)LV_MAX(FLIP_MIN_SCALE, sx));
        lv_image_set_scale_y(fl->screen_img, (uint32_t)s);
        watch_obj_set_hidden(fl->card_img, true);
        watch_obj_set_hidden(fl->screen_img, sx < FLIP_MIN_SCALE);
    }
    lv_obj_set_style_bg_opa(fx->overlay, (lv_opa_t)(FLIP_DIM * q / FX_P_MAX), 0);
}

void watch_fx_flip(lv_obj_t * card, lv_obj_t * screen, bool reverse, watch_fx_done_cb_t done, void * user_data)
{
    /* Forward: the card is on display, `screen` comes next. Reverse: the
     * display shows the screen, `card` is on the screen that comes next. */
    lv_obj_t * other = reverse ? lv_screen_active() : screen;
    lv_draw_buf_t * card_snap = fx_snapshot(card);
    lv_draw_buf_t * screen_snap = card_snap ? fx_snapshot(other) : NULL;
    if(screen_snap == NULL) {
        if(card_snap) lv_draw_buf_destroy(card_snap);
        fx_skip(done, user_data);
        return;
    }

    fx_t * fx = fx_create(flip_step, done, user_data);
    fx->snap_a = card_snap;
    fx->snap_b = screen_snap;
    fx->src_a = card;
    fx->slot_a = true;
    fx->src_b = other;
    fx->restore = card;
    lv_obj_set_style_bg_color(fx->overlay, lv_color_black(), 0);
    flip_t * fl = lv_malloc_zeroed(sizeof(flip_t));
    LV_ASSERT_MALLOC(fl);
    fx->data = fl;
    fl->reverse = reverse;
    fl->card_w = lv_obj_get_width(card);
    fl->screen_w = lv_obj_get_width(other);

    /* Both images are centred on the card */
    lv_area_t a;
    lv_obj_get_coords(card, &a);
    int32_t cx = (a.x1 + a.x2) / 2;
    int32_t cy = (a.y1 + a.y2) / 2;
    fl->card_img = fx_image_create(fx->overlay, card_snap);
    lv_obj_set_pos(fl->card_img, cx - fl->card_w / 2, cy - lv_obj_get_height(card) / 2);
    fl->screen_img = fx_image_create(fx->overlay, screen_snap);
    debug_border(fl->card_img, 0, 3);
    debug_border(fl->screen_img, 3, 3);
    lv_obj_set_pos(fl->screen_img, cx - fl->screen_w / 2, cy - lv_obj_get_height(other) / 2);

    watch_obj_set_hidden(card, true);
    fx_start(fx, FLIP_DURATION, lv_anim_path_ease_in_out);
}

/**********************
 * State
 **********************/

bool watch_fx_enabled(void)
{
    const char * env = getenv("WATCH_FX");
    return env == NULL || env[0] != '0';
}

bool watch_fx_available(void)
{
    return true;
}

void watch_fx_set_live(bool en)
{
    read_env();
    live_mode = en;
}

bool watch_fx_is_live(void)
{
    read_env();
    return live_mode;
}

void watch_fx_get_stats(watch_fx_stats_t * s)
{
    *s = stats;
}

void watch_fx_reset_stats(void)
{
    lv_memzero(&stats, sizeof(stats));
}

bool watch_fx_busy(void)
{
    return busy;
}

#else /* LV_USE_SNAPSHOT */

bool watch_fx_enabled(void) { return false; }
bool watch_fx_available(void) { return false; }
void watch_fx_set_live(bool en) { LV_UNUSED(en); }
bool watch_fx_is_live(void) { return false; }
void watch_fx_get_stats(watch_fx_stats_t * s) { lv_memzero(s, sizeof(*s)); }
void watch_fx_reset_stats(void) {}
bool watch_fx_busy(void) { return false; }
void watch_fx_shatter(lv_obj_t * obj, lv_point_t origin, watch_fx_done_cb_t done, void * user_data)
{
    LV_UNUSED(obj); LV_UNUSED(origin);
    if(done) done(user_data);
}
void watch_fx_ripple(lv_point_t origin) { LV_UNUSED(origin); }
void watch_fx_reveal(lv_obj_t * screen, lv_point_t origin, bool reverse, watch_fx_done_cb_t done, void * user_data)
{
    LV_UNUSED(screen); LV_UNUSED(origin); LV_UNUSED(reverse);
    if(done) done(user_data);
}
void watch_fx_flip(lv_obj_t * card, lv_obj_t * screen, bool reverse, watch_fx_done_cb_t done, void * user_data)
{
    LV_UNUSED(card); LV_UNUSED(screen); LV_UNUSED(reverse);
    if(done) done(user_data);
}

#endif /* LV_USE_SNAPSHOT */
