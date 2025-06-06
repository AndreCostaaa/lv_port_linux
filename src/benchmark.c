#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <demos/lv_demos.h>

#define H_RES 4096
#define V_RES 2160
#define BPP 2

static uint8_t buf1[H_RES * V_RES * BPP];
static uint8_t buf2[H_RES * V_RES * BPP];
static struct timespec start_time = {0};

void on_benchmark_end(const lv_demo_benchmark_summary_t * summary)
{
    lv_demo_benchmark_summary_display(summary);
    exit(0);
}

static void dummy_fb_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}
static uint32_t tick_cb(void)
{
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    uint64_t elapsed_ms;
    if(current_time.tv_nsec >= start_time.tv_nsec) {
        elapsed_ms =
            (current_time.tv_sec - start_time.tv_sec) * 1000 + (current_time.tv_nsec - start_time.tv_nsec) / 1000000;
    } else {
        elapsed_ms = (current_time.tv_sec - start_time.tv_sec - 1) * 1000 +
                     (1000000000 + current_time.tv_nsec - start_time.tv_nsec) / 1000000;
    }
    return (uint32_t)elapsed_ms;
}

int main(void)
{

    lv_init();

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    lv_tick_set_cb(tick_cb);
    lv_display_t * display = lv_display_create(H_RES, V_RES);
    lv_display_set_buffers(display, buf1, buf2, H_RES * V_RES * BPP, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, dummy_fb_cb);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    lv_demo_benchmark();
    lv_demo_benchmark_set_end_cb(on_benchmark_end);

    while(1) {
        usleep(lv_timer_handler() * 1000);
    }
}
