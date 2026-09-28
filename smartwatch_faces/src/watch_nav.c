/**
 * @file watch_nav.c
 * Phase 1 navigation: LVGL's built-in screen load animations over
 * permanent screens (auto_del = false) and a small back stack.
 *
 * With the snapshot effects on (watch_fx.c, WATCH_FX=0 turns them off):
 * selecting a face ripples the picker away, removing one shatters it,
 * edit flips its card over, and apps open in a circle from their icon.
 *
 * Works in the app and in the editor preview: in the app lv_demo_watch()
 * creates the screens into the generated globals (screen_home, ...); in the
 * preview the XML runtime creates them, so they are looked up by name.
 */

#include "watch_nav.h"
#include "watch_compat.h"
#include "watch_fx.h"

#define NAV_ANIM_TIME   300
#define NAV_STACK_SIZE  8
#define SHADE_ANIM_TIME 320

typedef enum {
    NAV_FX_NONE,
    NAV_FX_REVEAL,      /* app: collapse back into its icon */
    NAV_FX_FLIP,        /* edit: flip back into the picker card */
} nav_fx_t;

typedef struct {
    lv_obj_t * screen;
    lv_screen_load_anim_t back_anim;   /* animation used when returning to it */
    nav_fx_t back_fx;                  /* snapshot effect used instead, if on */
    lv_obj_t * fx_obj;                 /* icon (reveal) or card (flip) */
} nav_entry_t;

static nav_entry_t nav_stack[NAV_STACK_SIZE];
static uint32_t nav_depth;
static lv_obj_t * nav_current;

/* The generated global if the app created the screens, else the screen the
 * XML runtime (editor preview) created under that name */
static lv_obj_t * get_screen(lv_obj_t * global, const char * name)
{
    if(global) return global;
    lv_obj_t * screen = lv_display_get_screen_by_name(lv_display_get_default(), name);
#if LV_USE_XML
    if(screen == NULL) screen = lv_xml_create(NULL, name, NULL);
#endif
    if(screen == NULL) LV_LOG_WARN("screen %s not found", name);
    return screen;
}

#define SCREEN(name) get_screen(name, #name)

static lv_obj_t * current(void)
{
    if(nav_current == NULL) nav_current = lv_screen_active();
    return nav_current;
}

static void load(lv_obj_t * screen, lv_screen_load_anim_t anim)
{
    if(screen == NULL) return;
    nav_current = screen;
    lv_screen_load_anim(screen, anim, NAV_ANIM_TIME, 0, false);
}

/* Load right away: an effect on the top layer covers the change */
static void load_now(lv_obj_t * screen)
{
    if(screen == NULL) return;
    nav_current = screen;
    lv_screen_load(screen);
}

static void load_now_cb(void * screen)
{
    load_now(screen);
}

/* Remember the current screen so watch_nav_back() can return to it */
static void push(lv_screen_load_anim_t back_anim, nav_fx_t back_fx, lv_obj_t * fx_obj)
{
    if(nav_depth >= NAV_STACK_SIZE) return;
    nav_stack[nav_depth].screen = current();
    nav_stack[nav_depth].back_anim = back_anim;
    nav_stack[nav_depth].back_fx = back_fx;
    nav_stack[nav_depth].fx_obj = fx_obj;
    nav_depth++;
}

static void push_and_load(lv_obj_t * screen, lv_screen_load_anim_t anim, lv_screen_load_anim_t back_anim)
{
    push(back_anim, NAV_FX_NONE, NULL);
    load(screen, anim);
}

static lv_point_t obj_center(lv_obj_t * obj)
{
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    lv_point_t p = {(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
    return p;
}

/* Where the finger is, or the centre of `fallback` */
static lv_point_t touch_point(lv_obj_t * fallback)
{
    lv_indev_t * indev = lv_indev_active();
    if(indev) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        return p;
    }
    return obj_center(fallback);
}

static lv_obj_t * picker_card_slot(int32_t face_index)
{
    lv_obj_t * picker = SCREEN(screen_picker);
    lv_obj_t * carousel = picker ? lv_obj_find_by_name(picker, "face_carousel") : NULL;
    lv_obj_t * card = carousel ? lv_obj_get_child(carousel, face_index) : NULL;
    return card ? lv_obj_find_by_name(card, "face_slot") : NULL;
}

void watch_nav_init(void)
{
    nav_depth = 0;
    nav_current = SCREEN(screen_home);
    lv_screen_load(nav_current);
}

void watch_nav_open_picker(void)
{
    if(watch_fx_busy()) return;

    /* Start the carousel on the active face */
    lv_obj_t * picker = SCREEN(screen_picker);
    if(picker == NULL) return;

    /* Start the carousel on the active face */
    lv_obj_t * carousel = lv_obj_find_by_name(picker, "face_carousel");
    lv_obj_t * card = carousel ? lv_obj_get_child(carousel, lv_subject_get_int(&subject_active_face)) : NULL;
    if(card && !watch_obj_is_hidden(card)) lv_obj_scroll_to_view(card, LV_ANIM_OFF);

    push_and_load(picker, LV_SCREEN_LOAD_ANIM_FADE_IN, LV_SCREEN_LOAD_ANIM_FADE_IN);
}

void watch_nav_select_face(int32_t face_index)
{
    if(watch_fx_busy()) return;
    lv_subject_set_int(&subject_active_face, face_index);
    nav_depth = 0;

    if(watch_fx_enabled()) {
        /* The picker ripples away from the tap, home shows through */
        watch_fx_ripple(touch_point(lv_screen_active()));
        load_now(SCREEN(screen_home));
    }
    else {
        load(SCREEN(screen_home), LV_SCREEN_LOAD_ANIM_FADE_IN);
    }
}

static void delete_done_cb(void * card)
{
    lv_obj_t * carousel = lv_obj_get_parent(card);
    watch_obj_set_hidden(card, true);
    lv_obj_update_snap(carousel, LV_ANIM_ON);
}

void watch_nav_delete_face(int32_t face_index)
{
    lv_obj_t * picker = SCREEN(screen_picker);
    lv_obj_t * carousel = picker ? lv_obj_find_by_name(picker, "face_carousel") : NULL;
    lv_obj_t * card = carousel ? lv_obj_get_child(carousel, face_index) : NULL;
    if(card == NULL) return;

    /* Keep at least one face */
    uint32_t visible = 0;
    uint32_t cnt = lv_obj_get_child_count(carousel);
    for(uint32_t i = 0; i < cnt; i++) {
        if(!watch_obj_is_hidden(lv_obj_get_child(carousel, i))) visible++;
    }
    if(visible <= 1 || watch_fx_busy()) return;

    lv_obj_t * slot = lv_obj_find_by_name(card, "face_slot");
    if(watch_fx_enabled() && slot) {
        /* The face flies apart, then the carousel closes the gap */
        watch_fx_shatter(slot, touch_point(slot), delete_done_cb, card);
    }
    else {
        delete_done_cb(card);
    }
}

void watch_nav_open_face_edit(int32_t face_index)
{
    if(watch_fx_busy()) return;
    lv_subject_set_int(&subject_edit_face, face_index);

    lv_obj_t * edit = SCREEN(screen_face_edit);
    lv_obj_t * slot = picker_card_slot(face_index);
    if(watch_fx_enabled() && slot && edit) {
        /* The card turns over into the edit screen */
        push(LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, NAV_FX_FLIP, slot);
        watch_fx_flip(slot, edit, false, load_now_cb, edit);
    }
    else {
        push_and_load(edit, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
    }
}

void watch_nav_open_gallery(void)
{
    if(watch_fx_busy()) return;
    push_and_load(SCREEN(screen_face_gallery), LV_SCREEN_LOAD_ANIM_MOVE_TOP, LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM);
}

void watch_nav_close_face_edit(void)
{
    watch_nav_back();
}

void watch_nav_open_launcher(void)
{
    if(watch_fx_busy()) return;
    push_and_load(SCREEN(screen_launcher), LV_SCREEN_LOAD_ANIM_MOVE_LEFT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
}

void watch_nav_open_app(const char * app_id, lv_obj_t * origin)
{
    if(watch_fx_busy()) return;

    static const struct {
        const char * id;
        lv_obj_t ** screen;
        const char * name;
    } apps[] = {
        {"heart",    &screen_app_heart,    "screen_app_heart"},
        {"workout",  &screen_app_workout,  "screen_app_workout"},
        {"weather",  &screen_app_weather,  "screen_app_weather"},
        {"music",    &screen_app_music,    "screen_app_music"},
        {"timer",    &screen_app_timer,    "screen_app_timer"},
        {"settings", &screen_app_settings, "screen_app_settings"},
    };

    if(app_id == NULL) return;
    for(size_t i = 0; i < sizeof(apps) / sizeof(apps[0]); i++) {
        if(lv_strcmp(app_id, apps[i].id) == 0) {
            lv_obj_t * app = get_screen(*apps[i].screen, apps[i].name);
            if(watch_fx_enabled() && origin && app) {
                /* The app opens in a circle growing from its icon */
                push(LV_SCREEN_LOAD_ANIM_OUT_BOTTOM, NAV_FX_REVEAL, origin);
                watch_fx_reveal(app, obj_center(origin), false, load_now_cb, app);
            }
            else {
                push_and_load(app, LV_SCREEN_LOAD_ANIM_OVER_TOP, LV_SCREEN_LOAD_ANIM_OUT_BOTTOM);
            }
            return;
        }
    }
    LV_LOG_WARN("unknown app id: %s", app_id);
}

void watch_nav_back(void)
{
    if(nav_depth == 0 || watch_fx_busy()) return;
    nav_depth--;
    nav_entry_t * e = &nav_stack[nav_depth];

    if(watch_fx_enabled() && e->back_fx == NAV_FX_REVEAL) {
        /* The app collapses back into its icon */
        watch_fx_reveal(lv_screen_active(), obj_center(e->fx_obj), true, NULL, NULL);
        load_now(e->screen);
    }
    else if(watch_fx_enabled() && e->back_fx == NAV_FX_FLIP) {
        /* The edit screen turns back into its picker card */
        watch_fx_flip(e->fx_obj, e->screen, true, NULL, NULL);
        load_now(e->screen);
    }
    else {
        load(e->screen, e->back_anim);
    }
}

static void shade_anim_cb(void * obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, v, 0);
}

/* The shade of the screen on display. In the editor preview the screen being
 * previewed can be another instance than the permanent screen_home, so
 * don't go through the global. */
static lv_obj_t * get_shade(void)
{
    lv_obj_t * shade = lv_obj_find_by_name(lv_screen_active(), "shade");
    if(shade == NULL) {
        lv_obj_t * home = SCREEN(screen_home);
        if(home) shade = lv_obj_find_by_name(home, "shade");
    }
    return shade;
}

/* Open means "open or opening": the target of a running slide counts */
static bool shade_is_open(lv_obj_t * shade)
{
    lv_anim_t * a = lv_anim_get(shade, shade_anim_cb);
    int32_t y = a ? a->end_value : lv_obj_get_style_translate_y(shade, 0);
    return y > -lv_obj_get_height(shade) / 2;
}

void watch_nav_shade_open(bool open)
{
    lv_obj_t * shade = get_shade();
    if(shade == NULL || shade_is_open(shade) == open) return;

    /* Slide from wherever it is, so reversing mid-way is smooth */
    lv_anim_delete(shade, shade_anim_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, shade);
    lv_anim_set_exec_cb(&a, shade_anim_cb);
    lv_anim_set_values(&a, lv_obj_get_style_translate_y(shade, 0), open ? 0 : -lv_obj_get_height(shade));
    lv_anim_set_duration(&a, SHADE_ANIM_TIME);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

lv_obj_t * watch_nav_get_current(void)
{
    return current();
}

bool watch_nav_shade_is_open(void)
{
    lv_obj_t * shade = get_shade();
    return shade ? shade_is_open(shade) : false;
}
