/**
 * @file watch_depth.c
 * Depth-of-field carousel, see watch_depth.h. Updated on every scroll event
 * of face_carousel, so it follows drags, throws and the snap animation.
 */

#include "watch_depth.h"
#include "watch_thumbs.h"
#include "watch_compat.h"

#include <stdlib.h>

#define CARD_CNT         18
#define CARD_PITCH       300    /* distance between card centres (card width) */
/* Blur radius on a live card at half depth and beyond, like the snapshot
 * mode's blurred copy (~24 px on screen) */
#define LIVE_BLUR        24
#define DEPTH_SHRINK     56     /* scale lost at full depth, of 256 */
#define DEPTH_DIM        150    /* darkening at full depth, of 255 */
#define FACE_SCALE       143    /* style_face_thumb: 260 / 466 * 256 */
#define TITLE_FADE       200    /* title opacity lost at full depth */
#define PARALLAX_DIV     8      /* background moves 1/8 of the scroll */
#define PARALLAX_MAX     40

typedef struct {
    lv_obj_t * card;
    lv_obj_t * slot;    /* face_slot */
    lv_obj_t * face;    /* the live face tree in face_slot */
    lv_obj_t * frost;   /* live mode: blurs the face drawn under it */
    lv_obj_t * title;
} depth_card_t;

static depth_card_t cards[CARD_CNT];
static lv_obj_t * carousel;
static lv_obj_t * bg_holder;
static int32_t scroll_base;     /* scroll position when the picker was entered */
static bool enabled;
static bool live_styled;        /* live faces carry depth styles */

/* Live mode, every frame: the face tree is rendered and scaled, the frost on
 * top of it blurs the result (a blur works on what is already drawn under
 * the widget, not on its children), and the slot darkens it all. */
static void live_face_depth(depth_card_t * c, int32_t d)
{
    int32_t scale = FACE_SCALE * (256 - d * DEPTH_SHRINK / WATCH_DEPTH_MAX) / 256;
    lv_obj_set_style_transform_scale(c->face, scale, 0);
    /* Same curve as the snapshot cross-fade: fully defocused at half depth */
    int32_t blur = LV_MIN(LIVE_BLUR, d * LIVE_BLUR * 2 / WATCH_DEPTH_MAX);
    lv_obj_set_style_blur_radius(c->frost, blur, 0);
    watch_obj_set_hidden(c->frost, blur == 0);
    lv_obj_set_style_recolor(c->slot, lv_color_black(), 0);
    lv_obj_set_style_recolor_opa(c->slot, (lv_opa_t)(d * DEPTH_DIM / WATCH_DEPTH_MAX), 0);
}

static void live_face_reset(depth_card_t * c)
{
    lv_obj_remove_local_style_prop(c->face, LV_STYLE_TRANSFORM_SCALE_X, 0);
    lv_obj_remove_local_style_prop(c->face, LV_STYLE_TRANSFORM_SCALE_Y, 0);
    watch_obj_set_hidden(c->frost, true);
    lv_obj_remove_local_style_prop(c->slot, LV_STYLE_RECOLOR, 0);
    lv_obj_remove_local_style_prop(c->slot, LV_STYLE_RECOLOR_OPA, 0);
}

/* 0 when the card is centred, WATCH_DEPTH_MAX one card pitch away */
static int32_t card_depth(lv_obj_t * card)
{
    lv_area_t a;
    lv_obj_get_coords(card, &a);
    int32_t centre = (a.x1 + a.x2) / 2;
    int32_t screen_centre = lv_display_get_horizontal_resolution(lv_obj_get_display(card)) / 2;
    int32_t dist = LV_ABS(centre - screen_centre);
    return LV_MIN(WATCH_DEPTH_MAX, dist * WATCH_DEPTH_MAX / CARD_PITCH);
}

void watch_depth_update(void)
{
    if(carousel == NULL) return;
    bool snapshots = watch_thumbs_is_enabled();

    for(int i = 0; i < CARD_CNT; i++) {
        depth_card_t * c = &cards[i];
        int32_t d = enabled ? card_depth(c->card) : 0;

        watch_thumbs_set_depth((uint32_t)i, d);
        if(enabled && !snapshots) live_face_depth(c, d);
        else if(live_styled) live_face_reset(c);

        lv_obj_set_style_text_opa(c->title, (lv_opa_t)(255 - d * TITLE_FADE / WATCH_DEPTH_MAX), 0);
    }
    live_styled = enabled && !snapshots;

    /* Parallax: the background drifts against the scroll direction */
    int32_t shift = 0;
    if(enabled) {
        shift = -(lv_obj_get_scroll_x(carousel) - scroll_base) / PARALLAX_DIV;
        shift = LV_CLAMP(-PARALLAX_MAX, shift, PARALLAX_MAX);
    }
    lv_obj_set_style_translate_x(bg_holder, shift, 0);
}

static void scroll_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(enabled) watch_depth_update();
}

static void picker_load_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    scroll_base = lv_obj_get_scroll_x(carousel);
    watch_depth_update();
}

void watch_depth_init(void)
{
    carousel = lv_obj_find_by_name(screen_picker, "face_carousel");
    bg_holder = lv_obj_find_by_name(screen_picker, "picker_bg_holder");
    if(carousel == NULL || bg_holder == NULL) return;

    for(int i = 0; i < CARD_CNT; i++) {
        depth_card_t * c = &cards[i];
        c->card = lv_obj_get_child(carousel, i);
        c->slot = lv_obj_find_by_name(c->card, "face_slot");
        c->face = lv_obj_get_child(c->slot, 0);

        /* Last child of the slot, so it is drawn on top of the face */
        c->frost = lv_obj_create(c->slot);
        lv_obj_remove_style_all(c->frost);
        lv_obj_set_name(c->frost, "face_frost");
        lv_obj_set_size(c->frost, lv_pct(100), lv_pct(100));
        lv_obj_set_style_radius(c->frost, LV_RADIUS_CIRCLE, 0);
        watch_obj_set_clickable(c->frost, false);
        watch_obj_set_hidden(c->frost, true);
        c->title = lv_obj_get_child(c->card, 0);
    }

    lv_obj_add_event_cb(carousel, scroll_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(screen_picker, picker_load_cb, LV_EVENT_SCREEN_LOAD_START, NULL);

    /* Off by default; WATCH_DEPTH=1 turns it on */
    const char * env = getenv("WATCH_DEPTH");
    watch_depth_set_enabled(env != NULL && env[0] != '0');
}

void watch_depth_set_enabled(bool en)
{
    enabled = en;
    if(en) watch_thumbs_set_depth_mode(true);   /* make the blurred copies first */
    watch_depth_update();
    if(!en) watch_thumbs_set_depth_mode(false); /* cards are back at depth 0 */
}

bool watch_depth_is_enabled(void)
{
    return enabled;
}
