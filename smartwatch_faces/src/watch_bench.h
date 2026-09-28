/**
 * @file watch_bench.h
 * Scripted performance benchmark: home face, then the picker static and
 * scrolling, in live mode and (if compiled in) in snapshot mode. Prints a
 * report and exits. Enabled by lv_demo_watch() when WATCH_BENCH is set;
 * WATCH_BENCH_CSV=<file> also writes the results as CSV. Needs
 * LV_USE_SYSMON and LV_USE_PERF_MONITOR (the numbers come from sysmon).
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
