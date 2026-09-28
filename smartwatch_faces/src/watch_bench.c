/**
 * @file watch_bench.c
 * Phase 1 baseline for the snapshot comparison: every stage runs for a
 * fixed time while the display's render events are counted.
 *   1. home       - the classic face ticking
 *   2. picker     - picker shown, nothing moving but the live faces
 *   3. scrolling  - the carousel scrolled back and forth continuously
 */

#include "watch_bench.h"
#include "watch_nav.h"

#include <stdio.h>

#define STAGE_SETTLE_MS   1000
#define STAGE_MEASURE_MS  8000
#define SCROLL_SWEEP_MS   2500

typedef struct {
    const char * name;
    void (*enter)(void);
} bench_stage_t;

static void enter_home(void);
static void enter_picker(void);
static void enter_scrolling(void);

static const bench_stage_t stages[] = {
    {"home (classic face)", enter_home},
    {"picker, static", enter_picker},
    {"picker, scrolling", enter_scrolling},
};

static uint32_t stage;
static uint32_t render_cnt;
static uint32_t render_ms;
static uint32_t render_start;
static uint32_t idle_sum;
static uint32_t idle_samples;
static bool measuring;

static void render_event_cb(lv_event_t * e)
{
    if(!measuring) return;
    if(lv_event_get_code(e) == LV_EVENT_RENDER_START) {
        render_start = lv_tick_get();
    }
    else {
        render_ms += lv_tick_elaps(render_start);
        render_cnt++;
    }
}

static void idle_sample_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if(!measuring) return;
    idle_sum += lv_timer_get_idle();
    idle_samples++;
}

static void scroll_exec_cb(void * var, int32_t v)
{
    lv_obj_scroll_to_x(var, v, LV_ANIM_OFF);
}

static void enter_home(void)
{
    lv_subject_set_int(&subject_active_face, 0);
}

static void enter_picker(void)
{
    watch_nav_open_picker();
}

static void enter_scrolling(void)
{
    lv_obj_t * carousel = lv_obj_find_by_name(screen_picker, "face_carousel");
    int32_t span = lv_obj_get_scroll_x(carousel) + lv_obj_get_scroll_right(carousel);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, carousel);
    lv_anim_set_exec_cb(&a, scroll_exec_cb);
    lv_anim_set_values(&a, 0, span);
    lv_anim_set_duration(&a, SCROLL_SWEEP_MS);
    lv_anim_set_reverse_duration(&a, SCROLL_SWEEP_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void stage_cb(lv_timer_t * t)
{
    if(measuring) {
        /* Close the current stage */
        measuring = false;
        uint32_t idle = idle_samples ? idle_sum / idle_samples : 0;
        printf("[watch_bench] %-22s  %5.1f FPS  %5.1f ms/frame  LVGL CPU %3u%%\n",
               stages[stage].name,
               render_cnt * 1000.0 / STAGE_MEASURE_MS,
               render_cnt ? (double)render_ms / render_cnt : 0.0,
               (unsigned)(100 - idle));
        fflush(stdout);
        stage++;
        if(stage >= sizeof(stages) / sizeof(stages[0])) {
            printf("[watch_bench] done\n");
            fflush(stdout);
            lv_timer_delete(t);
            return;
        }
        stages[stage].enter();
        lv_timer_set_period(t, STAGE_SETTLE_MS);
    }
    else {
        /* Settled: start measuring */
        render_cnt = render_ms = idle_sum = idle_samples = 0;
        measuring = true;
        lv_timer_set_period(t, STAGE_MEASURE_MS);
    }
}

void watch_bench_start(void)
{
    lv_display_t * disp = lv_display_get_default();
    lv_display_add_event_cb(disp, render_event_cb, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(disp, render_event_cb, LV_EVENT_RENDER_READY, NULL);
    lv_timer_create(idle_sample_cb, 500, NULL);

    stage = 0;
    measuring = false;
    stages[0].enter();
    lv_timer_create(stage_cb, STAGE_SETTLE_MS, NULL);
}
