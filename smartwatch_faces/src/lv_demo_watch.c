/**
 * @file lv_demo_watch.c
 * Entry point of the smartwatch demo.
 */

#include "../lv_demo_watch.h"
#include "../smartwatch_faces.h"
#include "watch_nav.h"
#include "watch_time.h"
#include "watch_bench.h"
#include "watch_thumbs.h"
#include "watch_callbacks.h"

#include <stdlib.h>

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

    for(lv_indev_t * indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev)) {
        watch_callbacks_tune_indev(indev);
    }
    watch_time_init();
    watch_thumbs_init();
    watch_nav_init();

    /* WATCH_BENCH=1: record the phase 1 performance baseline */
    if(getenv("WATCH_BENCH")) watch_bench_start();
}
