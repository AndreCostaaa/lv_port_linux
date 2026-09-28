/**
 * @file watch_bench.h
 * Scripted baseline measurement: home face, static picker, scrolling
 * picker. Logs FPS, render time and CPU load. Enabled by lv_demo_watch()
 * when the WATCH_BENCH environment variable is set.
 */

#ifndef WATCH_BENCH_H
#define WATCH_BENCH_H

#ifdef __cplusplus
extern "C" {
#endif

void watch_bench_start(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WATCH_BENCH_H*/
