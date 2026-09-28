/**
 * @file watch_nav.c
 * Phase 1 navigation: LVGL's built-in screen load animations over
 * permanent screens (auto_del = false) and a small back stack.
 */

#include "watch_nav.h"
#include "watch_compat.h"

#define NAV_ANIM_TIME   300
#define NAV_STACK_SIZE  8

typedef struct {
    lv_obj_t * screen;
    lv_screen_load_anim_t back_anim;   /* animation used when returning to it */
} nav_entry_t;

static nav_entry_t nav_stack[NAV_STACK_SIZE];
static uint32_t nav_depth;
static lv_obj_t * nav_current;
static bool shade_open;

static void load(lv_obj_t * screen, lv_screen_load_anim_t anim)
{
    nav_current = screen;
    lv_screen_load_anim(screen, anim, NAV_ANIM_TIME, 0, false);
}

/* Remember the current screen so watch_nav_back() can return to it */
static void push_and_load(lv_obj_t * screen, lv_screen_load_anim_t anim, lv_screen_load_anim_t back_anim)
{
    if(nav_depth < NAV_STACK_SIZE) {
        nav_stack[nav_depth].screen = nav_current;
        nav_stack[nav_depth].back_anim = back_anim;
        nav_depth++;
    }
    load(screen, anim);
}

void watch_nav_init(void)
{
    nav_depth = 0;
    nav_current = screen_home;
    shade_open = false;
    lv_screen_load(screen_home);
}

void watch_nav_open_picker(void)
{
    /* Start the carousel on the active face */
    lv_obj_t * carousel = lv_obj_find_by_name(screen_picker, "face_carousel");
    lv_obj_t * card = lv_obj_get_child(carousel, lv_subject_get_int(&subject_active_face));
    if(card && !watch_obj_is_hidden(card)) lv_obj_scroll_to_view(card, LV_ANIM_OFF);

    push_and_load(screen_picker, LV_SCREEN_LOAD_ANIM_FADE_IN, LV_SCREEN_LOAD_ANIM_FADE_IN);
}

void watch_nav_select_face(int32_t face_index)
{
    lv_subject_set_int(&subject_active_face, face_index);
    nav_depth = 0;
    load(screen_home, LV_SCREEN_LOAD_ANIM_FADE_IN);
}

void watch_nav_delete_face(int32_t face_index)
{
    lv_obj_t * carousel = lv_obj_find_by_name(screen_picker, "face_carousel");
    lv_obj_t * card = lv_obj_get_child(carousel, face_index);
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
    push_and_load(screen_face_edit, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
}

void watch_nav_close_face_edit(void)
{
    watch_nav_back();
}

void watch_nav_open_launcher(void)
{
    push_and_load(screen_launcher, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
}

void watch_nav_open_app(const char * app_id, lv_obj_t * origin)
{
    LV_UNUSED(origin);  /* phase 2: origin of the circular reveal */

    static const struct {
        const char * id;
        lv_obj_t ** screen;
    } apps[] = {
        {"heart",    &screen_app_heart},
        {"workout",  &screen_app_workout},
        {"weather",  &screen_app_weather},
        {"music",    &screen_app_music},
        {"timer",    &screen_app_timer},
        {"settings", &screen_app_settings},
    };

    if(app_id == NULL) return;
    for(size_t i = 0; i < sizeof(apps) / sizeof(apps[0]); i++) {
        if(lv_strcmp(app_id, apps[i].id) == 0) {
            push_and_load(*apps[i].screen, LV_SCREEN_LOAD_ANIM_OVER_TOP, LV_SCREEN_LOAD_ANIM_OUT_BOTTOM);
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

void watch_nav_shade_open(bool open)
{
    if(open == shade_open) return;
    shade_open = open;

    lv_anim_timeline_t * at = screen_home_get_timeline(screen_home, SCREEN_HOME_TIMELINE_SHADE_SLIDE);
    if(at == NULL) return;
    lv_anim_timeline_set_reverse(at, !open);
    lv_anim_timeline_start(at);
}

lv_obj_t * watch_nav_get_current(void)
{
    return nav_current;
}

bool watch_nav_shade_is_open(void)
{
    return shade_open;
}
