/**
 * @file lv_demo_watch.c
 * Entry point of the smartwatch demo.
 */

#include "../lv_demo_watch.h"
#include "../smartwatch_faces.h"
#include "watch_nav.h"
#include "watch_time.h"
#include "watch_compat.h"
#include "watch_bench.h"

#include <stdlib.h>

/* Faces are decoration inside their holder (face stack, picker slot,
 * edit preview): make their trees non-clickable so presses, long-presses
 * and gestures land on the holder instead of on some inner box. */
static void make_passive(lv_obj_t * obj)
{
    watch_obj_set_clickable(obj, false);
    uint32_t cnt = lv_obj_get_child_count(obj);
    for(uint32_t i = 0; i < cnt; i++) make_passive(lv_obj_get_child(obj, i));
}

static void make_children_passive(lv_obj_t * holder)
{
    if(holder == NULL) return;
    uint32_t cnt = lv_obj_get_child_count(holder);
    for(uint32_t i = 0; i < cnt; i++) make_passive(lv_obj_get_child(holder, i));
}

/* LVGL's defaults (50 px, reset below 3 px per read) miss slow or short
 * swipes. A watch screen is small: accept any steady 40 px swipe. */
#define GESTURE_MIN_DISTANCE  40
#define GESTURE_MIN_VELOCITY  1

static void tune_gestures(void)
{
    for(lv_indev_t * indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev)) {
        if(lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) continue;
        lv_indev_set_gesture_min_distance(indev, GESTURE_MIN_DISTANCE);
        lv_indev_set_gesture_min_velocity(indev, GESTURE_MIN_VELOCITY);
    }
}

void lv_demo_watch(void)
{
    smartwatch_faces_init("");

    /* Every screen is created once and kept (permanent, never auto-deleted) */
    screen_home_create();
    screen_picker_create();
    screen_face_edit_create();
    screen_launcher_create();
    screen_app_heart_create();
    screen_app_workout_create();
    screen_app_weather_create();
    screen_app_music_create();
    screen_app_timer_create();
    screen_app_settings_create();

    make_children_passive(lv_obj_find_by_name(screen_home, "faces"));
    make_children_passive(lv_obj_find_by_name(screen_picker, "picker_bg"));
    make_children_passive(lv_obj_find_by_name(screen_face_edit, "edit_preview"));

    lv_obj_t * carousel = lv_obj_find_by_name(screen_picker, "face_carousel");
    uint32_t card_cnt = lv_obj_get_child_count(carousel);
    for(uint32_t i = 0; i < card_cnt; i++) {
        make_children_passive(lv_obj_find_by_name(lv_obj_get_child(carousel, i), "face_slot"));
    }

    tune_gestures();
    watch_time_init();
    watch_nav_init();

    /* WATCH_BENCH=1: record the phase 1 performance baseline */
    if(getenv("WATCH_BENCH")) watch_bench_start();
}
