/**
 * @file watch_nav.c
 * Phase 1 navigation: LVGL's built-in screen load animations over
 * permanent screens (auto_del = false) and a small back stack.
 *
 * Works in the app and in the editor preview: in the app lv_demo_watch()
 * creates the screens into the generated globals (screen_home, ...); in the
 * preview the XML runtime creates them, so they are looked up by name.
 */

#include "watch_nav.h"
#include "watch_compat.h"

#define NAV_ANIM_TIME   300
#define NAV_STACK_SIZE  8
#define SHADE_ANIM_TIME 320

typedef struct {
    lv_obj_t * screen;
    lv_screen_load_anim_t back_anim;   /* animation used when returning to it */
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

/* Remember the current screen so watch_nav_back() can return to it */
static void push_and_load(lv_obj_t * screen, lv_screen_load_anim_t anim, lv_screen_load_anim_t back_anim)
{
    if(nav_depth < NAV_STACK_SIZE) {
        nav_stack[nav_depth].screen = current();
        nav_stack[nav_depth].back_anim = back_anim;
        nav_depth++;
    }
    load(screen, anim);
}

void watch_nav_init(void)
{
    nav_depth = 0;
    nav_current = SCREEN(screen_home);
    lv_screen_load(nav_current);
}

void watch_nav_open_picker(void)
{
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
    lv_subject_set_int(&subject_active_face, face_index);
    nav_depth = 0;
    load(SCREEN(screen_home), LV_SCREEN_LOAD_ANIM_FADE_IN);
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
    if(visible <= 1) return;

    watch_obj_set_hidden(card, true);
    lv_obj_update_snap(carousel, LV_ANIM_ON);
}

void watch_nav_open_face_edit(int32_t face_index)
{
    lv_subject_set_int(&subject_edit_face, face_index);
    push_and_load(SCREEN(screen_face_edit), LV_SCREEN_LOAD_ANIM_MOVE_LEFT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
}

void watch_nav_open_gallery(void)
{
    push_and_load(SCREEN(screen_face_gallery), LV_SCREEN_LOAD_ANIM_MOVE_TOP, LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM);
}

void watch_nav_close_face_edit(void)
{
    watch_nav_back();
}

void watch_nav_open_launcher(void)
{
    push_and_load(SCREEN(screen_launcher), LV_SCREEN_LOAD_ANIM_MOVE_LEFT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
}

void watch_nav_open_app(const char * app_id, lv_obj_t * origin)
{
    LV_UNUSED(origin);  /* phase 2: origin of the circular reveal */

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
            push_and_load(get_screen(*apps[i].screen, apps[i].name), LV_SCREEN_LOAD_ANIM_OVER_TOP, LV_SCREEN_LOAD_ANIM_OUT_BOTTOM);
            return;
        }
    }
    LV_LOG_WARN("unknown app id: %s", app_id);
}

void watch_nav_back(void)
{
    if(nav_depth == 0) return;
    nav_depth--;
    load(nav_stack[nav_depth].screen, nav_stack[nav_depth].back_anim);
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
