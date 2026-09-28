/**
 * @file watch_bench.c
 * Every stage settles for STAGE_SETTLE_MS, then measures for its measure
 * time (STATIC_MEASURE_MS / SCROLL_MEASURE_MS). The numbers are LVGL's
 * sysmon: the perf monitor publishes FPS, CPU, process CPU and the average
 * refresh / render / flush times every LV_SYSMON_REFR_PERIOD_DEF ms, and a
 * stage reports the average of those. sysmon only has averages, so the
 * longest render of the stage is timed from the display events the same way
 * sysmon measures render time: RENDER_START -> RENDER_READY minus the
 * flushes inside it. Plus the snapshot statistics of watch_thumbs.c and the
 * process RSS.
 *
 * On Linux sysmon's CPU is the whole machine (/proc/stat) and the process
 * CPU is this process as a share of all CPUs; the report also shows the
 * latter in cores (x CPU count).
 */

/* sysconf() */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif

#include "watch_bench.h"
#include "watch_nav.h"
#include "watch_thumbs.h"

#include <stdio.h>
#include <stdlib.h>
#if defined(__linux__)
    #include <unistd.h>
#endif

#if LV_USE_SYSMON && LV_USE_PERF_MONITOR

#define STAGE_SETTLE_MS   1500
#define STATIC_MEASURE_MS  4000   /* home and static picker */
#define SCROLL_MEASURE_MS 10000   /* scrolling picker */
#define SCROLL_SWEEP_MS   2500
#define FLUSH_STALL_MS    100      /* average flush above this: display was blocked */

typedef enum {
    MODE_ANY,
    MODE_LIVE,
    MODE_SNAPSHOT,
} bench_mode_t;

typedef struct {
    const char * name;
    bench_mode_t mode;
    void (*enter)(void);
    uint32_t measure_ms;
} bench_stage_t;

/* sysmon values, averaged over its reports during the stage */
typedef struct {
    uint32_t fps;
    uint32_t cpu;               /* 100 - LV_SYSMON_GET_IDLE() */
    uint32_t cpu_proc;          /* 100 - LV_SYSMON_GET_PROC_IDLE(), % of all CPUs */
    uint32_t refr_ms, render_ms, flush_ms;
    uint32_t reports;
    uint32_t max_render_ms;     /* longest render, flush excluded */
    watch_thumbs_stats_t snap;
    uint32_t rss_kb;
} bench_result_t;

static void enter_home(void);
static void enter_picker_live(void);
static void enter_picker_snapshot(void);
static void enter_scrolling(void);

static const bench_stage_t stages[] = {
    {"home",             MODE_ANY,      enter_home,            STATIC_MEASURE_MS},
    {"picker static",    MODE_LIVE,     enter_picker_live,     STATIC_MEASURE_MS},
    {"picker scrolling", MODE_LIVE,     enter_scrolling,       SCROLL_MEASURE_MS},
    {"picker static",    MODE_SNAPSHOT, enter_picker_snapshot, STATIC_MEASURE_MS},
    {"picker scrolling", MODE_SNAPSHOT, enter_scrolling,       SCROLL_MEASURE_MS},
};
#define STAGE_CNT (sizeof(stages) / sizeof(stages[0]))

static bench_result_t results[STAGE_CNT];
static bool ran[STAGE_CNT];
static uint32_t stage;
static bool measuring;

/* sums of the sysmon reports */
static uint32_t sys_fps, sys_cpu, sys_cpu_proc, sys_refr, sys_render, sys_flush, sys_reports;

/* longest render of the stage */
static bool render_in_progress;
static uint32_t render_start;
static uint32_t flush_start;
static uint32_t flush_in_render;
static uint32_t max_render;

static lv_obj_t * carousel;
static long cpu_cnt = 1;

/**********************
 * Measuring
 **********************/

/* Like sysmon: the flush that happens inside a render is not render time */
static void render_event_cb(lv_event_t * e)
{
    switch(lv_event_get_code(e)) {
        case LV_EVENT_RENDER_START:
            render_in_progress = true;
            render_start = lv_tick_get();
            flush_in_render = 0;
            break;
        case LV_EVENT_FLUSH_START:
        case LV_EVENT_FLUSH_WAIT_START:
            if(render_in_progress) flush_start = lv_tick_get();
            break;
        case LV_EVENT_FLUSH_FINISH:
        case LV_EVENT_FLUSH_WAIT_FINISH:
            if(render_in_progress) flush_in_render += lv_tick_elaps(flush_start);
            break;
        case LV_EVENT_RENDER_READY: {
                render_in_progress = false;
                if(!measuring) break;
                uint32_t elapsed = lv_tick_elaps(render_start);
                uint32_t render = elapsed > flush_in_render ? elapsed - flush_in_render : 0;
                if(render > max_render) max_render = render;
                break;
            }
        default:
            break;
    }
}

/* Called by sysmon every LV_SYSMON_REFR_PERIOD_DEF ms with fresh averages */
static void sysmon_observer_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(observer);
    if(!measuring) return;
    const lv_sysmon_perf_info_t * perf = lv_subject_get_pointer(subject);
    if(perf == NULL) return;
    sys_fps += perf->calculated.fps;
    sys_cpu += perf->calculated.cpu;
#if LV_SYSMON_PROC_IDLE_AVAILABLE
    sys_cpu_proc += perf->calculated.cpu_proc;
#endif
    sys_refr += perf->calculated.refr_avg_time;
    sys_render += perf->calculated.render_avg_time;
    sys_flush += perf->calculated.flush_avg_time;
    sys_reports++;
}

static uint32_t read_rss_kb(void)
{
#if defined(__linux__)
    FILE * f = fopen("/proc/self/statm", "r");
    if(f == NULL) return 0;
    unsigned long size = 0, resident = 0;
    int n = fscanf(f, "%lu %lu", &size, &resident);
    fclose(f);
    if(n != 2) return 0;
    return (uint32_t)(resident * (unsigned long)sysconf(_SC_PAGESIZE) / 1024);
#else
    return 0;
#endif
}

static void measure_begin(void)
{
    max_render = 0;
    sys_fps = sys_cpu = sys_cpu_proc = sys_refr = sys_render = sys_flush = sys_reports = 0;
    watch_thumbs_reset_stats();
    measuring = true;
}

static void measure_end(bench_result_t * r)
{
    measuring = false;
    lv_memzero(r, sizeof(*r));

    if(sys_reports) {
        r->fps = sys_fps / sys_reports;
        r->cpu = sys_cpu / sys_reports;
        r->cpu_proc = sys_cpu_proc / sys_reports;
        r->refr_ms = sys_refr / sys_reports;
        r->render_ms = sys_render / sys_reports;
        r->flush_ms = sys_flush / sys_reports;
        r->reports = sys_reports;
    }
    r->max_render_ms = max_render;
    watch_thumbs_get_stats(&r->snap);
    r->rss_kb = read_rss_kb();
}

/**********************
 * Stages
 **********************/

static void scroll_exec_cb(void * var, int32_t v)
{
    lv_obj_scroll_to_x(var, v, LV_ANIM_OFF);
}

static void stop_scrolling(void)
{
    lv_anim_delete(carousel, scroll_exec_cb);
    lv_obj_scroll_to_x(carousel, 0, LV_ANIM_OFF);
}

static void enter_home(void)
{
    lv_subject_set_int(&subject_active_face, 0);
}

static void enter_picker_live(void)
{
    watch_thumbs_set_enabled(false);
    watch_nav_open_picker();
}

static void enter_picker_snapshot(void)
{
    stop_scrolling();
    watch_thumbs_set_enabled(true);
}

static void enter_scrolling(void)
{
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

static bool stage_enabled(uint32_t i)
{
    return stages[i].mode != MODE_SNAPSHOT || watch_thumbs_available();
}

/**********************
 * Report
 **********************/

static const char * mode_name(bench_mode_t m)
{
    return m == MODE_LIVE ? "live" : m == MODE_SNAPSHOT ? "snapshot" : "-";
}

/* sysmon's process CPU is a share of all CPUs; x CPU count = cores busy */
static double proc_cores(uint32_t cpu_proc)
{
    return cpu_proc * (double)cpu_cnt / 100.0;
}

static void snapshot_summary(const watch_thumbs_stats_t * st, char * buf, size_t size)
{
    if(st->snapshot_cnt == 0) {
        snprintf(buf, size, "-");
        return;
    }
    snprintf(buf, size, "%u, avg %u ms, max %u ms", (unsigned)st->snapshot_cnt,
             (unsigned)(st->snapshot_ms / st->snapshot_cnt), (unsigned)st->snapshot_max_ms);
}

static void print_report(void)
{
    lv_display_t * disp = lv_display_get_default();

    printf("\n[watch_bench] ==== smartwatch demo performance (LVGL sysmon) ====\n");
    printf("[watch_bench] LVGL %d.%d.%d%s%s, %dx%d, %d bpp, %ld CPUs, refresh period %u ms\n",
           LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
           LVGL_VERSION_INFO[0] ? "-" : "", LVGL_VERSION_INFO,
           (int)lv_display_get_horizontal_resolution(disp), (int)lv_display_get_vertical_resolution(disp),
           LV_COLOR_DEPTH, cpu_cnt, (unsigned)lv_display_get_refr_timer(disp)->period);
    printf("[watch_bench]\n");
    printf("[watch_bench] %-17s %-8s %5s %8s %14s %11s %13s %12s\n",
           "scene", "mode", "FPS", "CPU (%)", "proc (cores)", "refr (ms)", "render (ms)", "flush (ms)");

    for(uint32_t i = 0; i < STAGE_CNT; i++) {
        if(!ran[i]) continue;
        const bench_result_t * r = &results[i];
        char snaps[64], proc[24];
        snapshot_summary(&r->snap, snaps, sizeof(snaps));
        snprintf(proc, sizeof(proc), "%u%% (%.1f)", (unsigned)r->cpu_proc, proc_cores(r->cpu_proc));
        printf("[watch_bench] %-17s %-8s %5u %8u %14s %11u %13u %12u\n",
               stages[i].name, mode_name(stages[i].mode), (unsigned)r->fps, (unsigned)r->cpu, proc,
               (unsigned)r->refr_ms, (unsigned)r->render_ms, (unsigned)r->flush_ms
              );
    }

    /* Scrolling: live vs snapshot */
    if(watch_thumbs_available()) {
        watch_thumbs_stats_t st;
        watch_thumbs_get_stats(&st);
        printf("[watch_bench] snapshot buffers: %.2f MB\n", st.buffer_bytes / (1024.0 * 1024.0));
    }
    printf("[watch_bench] ====================================\n");
    fflush(stdout);
}

static void write_csv(const char * path)
{
    FILE * f = fopen(path, "w");
    if(f == NULL) {
        printf("[watch_bench] can't write %s\n", path);
        return;
    }
    fprintf(f, "stage,mode,fps,cpu_pct,proc_cpu_pct,proc_cores,refr_ms,render_ms,flush_ms,max_render_ms,"
            "snapshots,snapshot_avg_ms,snapshot_max_ms,rss_kb\n");
    for(uint32_t i = 0; i < STAGE_CNT; i++) {
        if(!ran[i]) continue;
        const bench_result_t * r = &results[i];
        fprintf(f, "%s,%s,%u,%u,%u,%.2f,%u,%u,%u,%u,%u,%u,%u,%u\n",
                stages[i].name, mode_name(stages[i].mode), (unsigned)r->fps, (unsigned)r->cpu,
                (unsigned)r->cpu_proc, proc_cores(r->cpu_proc), (unsigned)r->refr_ms, (unsigned)r->render_ms,
                (unsigned)r->flush_ms, (unsigned)r->max_render_ms, (unsigned)r->snap.snapshot_cnt,
                r->snap.snapshot_cnt ? (unsigned)(r->snap.snapshot_ms / r->snap.snapshot_cnt) : 0u,
                (unsigned)r->snap.snapshot_max_ms, (unsigned)r->rss_kb);
    }
    fclose(f);
    printf("[watch_bench] results written to %s\n", path);
}

/**********************
 * Sequencing
 **********************/

static void next_stage(lv_timer_t * t)
{
    while(stage < STAGE_CNT && !stage_enabled(stage)) stage++;
    if(stage >= STAGE_CNT) {
        lv_timer_delete(t);
        stop_scrolling();
        print_report();
        const char * csv = getenv("WATCH_BENCH_CSV");
        if(csv && csv[0]) write_csv(csv);
        exit(0);
    }
    printf("[watch_bench] %s (%s)...\n", stages[stage].name, mode_name(stages[stage].mode));
    fflush(stdout);
    stages[stage].enter();
    lv_timer_set_period(t, STAGE_SETTLE_MS);
}

static void stage_cb(lv_timer_t * t)
{
    if(measuring) {
        measure_end(&results[stage]);
        ran[stage] = true;
        stage++;
        next_stage(t);
    }
    else {
        measure_begin();
        lv_timer_set_period(t, stages[stage].measure_ms);
    }
}

void watch_bench_start(void)
{
    carousel = lv_obj_find_by_name(screen_picker, "face_carousel");
#if defined(__linux__)
    cpu_cnt = sysconf(_SC_NPROCESSORS_ONLN);
    if(cpu_cnt < 1) cpu_cnt = 1;
#endif

    lv_display_t * disp = lv_display_get_default();
    if(disp->perf_sysmon_backend.subject == NULL) {
        printf("[watch_bench] the perf monitor is not running, can't benchmark\n");
        return;
    }
    lv_subject_add_observer(disp->perf_sysmon_backend.subject, sysmon_observer_cb, NULL);
    static const lv_event_code_t render_events[] = {
        LV_EVENT_RENDER_START, LV_EVENT_RENDER_READY,
        LV_EVENT_FLUSH_START, LV_EVENT_FLUSH_FINISH,
        LV_EVENT_FLUSH_WAIT_START, LV_EVENT_FLUSH_WAIT_FINISH,
    };
    for(size_t i = 0; i < sizeof(render_events) / sizeof(render_events[0]); i++) {
        lv_display_add_event_cb(disp, render_event_cb, render_events[i], NULL);
    }

    stage = 0;
    measuring = false;
    lv_timer_t * t = lv_timer_create(stage_cb, STAGE_SETTLE_MS, NULL);
    next_stage(t);
}

#else /* LV_USE_SYSMON && LV_USE_PERF_MONITOR */

#include <stdio.h>

void watch_bench_start(void)
{
    printf("[watch_bench] needs LV_USE_SYSMON and LV_USE_PERF_MONITOR\n");
}

#endif
