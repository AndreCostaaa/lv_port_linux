/**
 * @file watch_callbacks.c
 * The watch_on_* callbacks referenced by <event_cb> in the XML. They only
 * decode the event (user_data, gesture direction) and call watch_nav_*;
 * there is no navigation logic here.
 */

#include "watch_nav.h"
#include "watch_callbacks.h"

#include <stdlib.h>

/* LVGL's defaults (50 px, reset below 3 px per read) miss slow or short
 * swipes. A watch screen is small: accept any steady 40 px swipe. */
#define GESTURE_MIN_DISTANCE  40
#define GESTURE_MIN_VELOCITY  1

void watch_callbacks_tune_indev(lv_indev_t * indev)
{
    if(indev == NULL || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) return;
    lv_indev_set_gesture_min_distance(indev, GESTURE_MIN_DISTANCE);
    lv_indev_set_gesture_min_velocity(indev, GESTURE_MIN_VELOCITY);
}

static int32_t user_data_int(lv_event_t * e)
{
    const char * s = lv_event_get_user_data(e);
    return s ? (int32_t)strtol(s, NULL, 10) : 0;
}

static lv_dir_t gesture_dir(void)
{
    lv_indev_t * indev = lv_indev_active();
    if(indev == NULL) return LV_DIR_NONE;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    /* Don't let the release that ends a swipe also click something */
    lv_indev_wait_release(indev);
    return dir;
}

/* screen_home: swipe down = shade, swipe up = close shade, swipe left = launcher */
void watch_on_home_gesture(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_dir_t dir = gesture_dir();

    if(watch_nav_shade_is_open()) {
        if(dir == LV_DIR_TOP) watch_nav_shade_open(false);
        return;
    }

    if(dir == LV_DIR_BOTTOM) watch_nav_shade_open(true);
    else if(dir == LV_DIR_LEFT) watch_nav_open_launcher();
}

/* A long-press only counts if the finger stayed put; otherwise it is the
 * start of a slow swipe */
#define LONG_PRESS_SLOP  16

static lv_point_t press_point;

/* screen_home: press on the face, remembered for the long-press check.
 * Also tunes the input device: in the editor preview lv_demo_watch() does
 * not run, and this press is before any movement, so it still applies. */
void watch_on_face_pressed(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_indev_t * indev = lv_indev_active();
    if(indev == NULL) return;
    watch_callbacks_tune_indev(indev);
    lv_indev_get_point(indev, &press_point);
}

/* screen_home: long-press on the face */
void watch_on_face_long_press(lv_event_t * e)
{
    LV_UNUSED(e);
    if(watch_nav_shade_is_open()) return;

    lv_indev_t * indev = lv_indev_active();
    if(indev) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        if(LV_ABS(p.x - press_point.x) > LONG_PRESS_SLOP || LV_ABS(p.y - press_point.y) > LONG_PRESS_SLOP) return;
    }
    watch_nav_open_picker();
}

/* launcher and apps: swipe right = back */
void watch_on_back_gesture(lv_event_t * e)
{
    LV_UNUSED(e);
    if(gesture_dir() == LV_DIR_RIGHT) watch_nav_back();
}

/* face_card_*: tap on face_slot, user_data = face index */
void watch_on_face_select(lv_event_t * e)
{
    watch_nav_select_face(user_data_int(e));
}

/* face_card_*: long-press on face_slot, user_data = face index */
void watch_on_face_delete(lv_event_t * e)
{
    watch_nav_delete_face(user_data_int(e));
}

/* face_card_base: edit_btn, user_data = face index */
void watch_on_face_edit(lv_event_t * e)
{
    watch_nav_open_face_edit(user_data_int(e));
}

/* screen_face_edit: Done */
void watch_on_face_edit_done(lv_event_t * e)
{
    LV_UNUSED(e);
    watch_nav_close_face_edit();
}

/* app_row: tap on app_icon_<id>, user_data = app id */
void watch_on_app_open(lv_event_t * e)
{
    watch_nav_open_app(lv_event_get_user_data(e), lv_event_get_target(e));
}
