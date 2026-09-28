/**
 * @file watch_time.c
 * Every face names its hands hand_hour / hand_min / hand_sec. They are
 * collected once from all permanent screens (home, picker, edit preview)
 * and rotated whenever the time changes, so every live instance ticks.
 */

/* localtime_r() under strict C99 */
#ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
#endif

#include "watch_time.h"
#include "watch_thumbs.h"

#include <time.h>

#define TICK_PERIOD_MS  200    /* poll the clock, act when the second changes */
#define HANDS_MAX       64     /* per hand kind: 6 faces x 3 screens + margin */

typedef enum {
    HAND_HOUR,
    HAND_MIN,
    HAND_SEC,
    HAND_KIND_CNT
} hand_kind_t;

static const char * const hand_names[HAND_KIND_CNT] = {"hand_hour", "hand_min", "hand_sec"};
static lv_obj_t * hands[HAND_KIND_CNT][HANDS_MAX];
static uint32_t hand_cnt[HAND_KIND_CNT];

static lv_obj_t * hr_chart;
static int last_sec = -1;
static uint32_t tick_cnt;

static void collect_hands(lv_obj_t * obj)
{
    const char * name = lv_obj_get_name(obj);
    if(name) {
        for(int k = 0; k < HAND_KIND_CNT; k++) {
            if(lv_strcmp(name, hand_names[k]) == 0 && hand_cnt[k] < HANDS_MAX) {
                hands[k][hand_cnt[k]++] = obj;
                break;
            }
        }
    }

    uint32_t cnt = lv_obj_get_child_count(obj);
    for(uint32_t i = 0; i < cnt; i++) collect_hands(lv_obj_get_child(obj, i));
}

/* Rotation in 0.1 deg, clockwise from 12 o'clock */
static void rotate_hands(int h, int m, int s)
{
    int32_t angle[HAND_KIND_CNT];
    angle[HAND_HOUR] = ((h % 12) * 60 + m) * 5 + s / 12;
    angle[HAND_MIN] = m * 60 + s;
    angle[HAND_SEC] = s * 60;

    for(int k = 0; k < HAND_KIND_CNT; k++) {
        for(uint32_t i = 0; i < hand_cnt[k]; i++) {
            lv_obj_set_style_transform_rotation(hands[k][i], angle[k], 0);
        }
    }
}

static void set_int(lv_subject_t * subject, int32_t v)
{
    if(lv_subject_get_int(subject) != v) lv_subject_set_int(subject, v);
}

static void set_string(lv_subject_t * subject, const char * s)
{
    if(lv_strcmp(lv_subject_get_string(subject), s) != 0) lv_subject_copy_string(subject, s);
}

static void update_clock(const struct tm * t)
{
    static const char * const weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static const char * const months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
                                         };

    set_int(&subject_hour, t->tm_hour);
    set_int(&subject_minute, t->tm_min);
    set_int(&subject_second, t->tm_sec);
    set_int(&subject_day, t->tm_mday);
    set_string(&subject_weekday, weekdays[t->tm_wday]);
    set_string(&subject_month, months[t->tm_mon]);

    rotate_hands(t->tm_hour, t->tm_min, t->tm_sec);
}

/* Heart rate: +-3 bpm random walk, appended to the chart */
static void update_heart_rate(void)
{
    int32_t hr = lv_subject_get_int(&subject_heart_rate) + lv_rand(0, 6) - 3;
    hr = LV_CLAMP(58, hr, 96);
    set_int(&subject_heart_rate, hr);

    if(hr_chart == NULL) return;
    lv_chart_series_t * ser = lv_chart_get_series_next(hr_chart, NULL);
    if(ser == NULL) return;
    lv_chart_set_next_value(hr_chart, ser, hr);

    /* Min / avg / max of the chart window */
    const int32_t * y = lv_chart_get_series_y_array(hr_chart, ser);
    uint32_t n = lv_chart_get_point_count(hr_chart);
    int32_t min = INT32_MAX, max = INT32_MIN, sum = 0, valid = 0;
    for(uint32_t i = 0; i < n; i++) {
        if(y[i] == LV_CHART_POINT_NONE) continue;
        min = LV_MIN(min, y[i]);
        max = LV_MAX(max, y[i]);
        sum += y[i];
        valid++;
    }
    if(valid == 0) return;
    set_int(&subject_hr_min, min);
    set_int(&subject_hr_max, max);
    set_int(&subject_hr_avg, sum / valid);
}

/* A few steps now and then, nudging the move ring along */
static void update_activity(void)
{
    if(lv_rand(0, 3) != 0) return;
    int32_t steps = lv_subject_get_int(&subject_steps) + lv_rand(1, 12);
    set_int(&subject_steps, steps);

    int32_t goal = LV_MAX(1, lv_subject_get_int(&subject_steps_goal));
    set_int(&subject_move, LV_MIN(100, steps * 100 / goal));
}

static void tick_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    if(t.tm_sec == last_sec) return;
    last_sec = t.tm_sec;
    tick_cnt++;

    update_clock(&t);
    update_heart_rate();
    update_activity();
    watch_thumbs_tick();    /* after the hands moved */

    /* Battery drains 1% every 3 minutes */
    if(tick_cnt % 180 == 0) set_int(&subject_battery, LV_MAX(5, lv_subject_get_int(&subject_battery) - 1));
}

void watch_time_init(void)
{
    lv_obj_t * screens[] = {screen_home, screen_picker, screen_face_edit};
    for(size_t i = 0; i < sizeof(screens) / sizeof(screens[0]); i++) {
        if(screens[i]) collect_hands(screens[i]);
    }

    hr_chart = screen_app_heart ? lv_obj_find_by_name(screen_app_heart, "hr_chart") : NULL;

    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);
    tick_cb(NULL);
}
