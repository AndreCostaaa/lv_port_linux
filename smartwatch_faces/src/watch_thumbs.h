/**
 * @file watch_thumbs.h
 * Snapshot mode of the face picker and the face gallery (compile-time:
 * WATCH_USE_SNAPSHOTS, set by CONFIG_LV_DEMO_WATCH_USE_SNAPSHOTS).
 *
 * The live picker re-renders six scaled face trees and a blurred full-screen
 * face on every frame while it scrolls. In snapshot mode every face_slot shows
 * an image snapshot of its face and picker_bg is a single pre-blurred
 * snapshot; the live trees stay in place, hidden, and are only drawn when a
 * snapshot is refreshed (once per second while the carousel is at rest, so
 * the faces keep ticking; never while it scrolls).
 */

#ifndef WATCH_THUMBS_H
#define WATCH_THUMBS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../smartwatch_faces.h"

#ifndef WATCH_USE_SNAPSHOTS
    #define WATCH_USE_SNAPSHOTS 0
#endif

typedef struct {
    uint32_t snapshot_cnt;     /* snapshots taken */
    uint32_t snapshot_ms;      /* total time spent taking them */
    uint32_t snapshot_max_ms;  /* slowest one */
    uint32_t buffer_bytes;     /* memory held by the snapshot buffers (incl. blurred copies) */
} watch_thumbs_stats_t;

/** Call once after the screens are created. Starts in snapshot mode unless WATCH_SNAPSHOTS=0. */
void watch_thumbs_init(void);

/** true if the snapshot mode is compiled in */
bool watch_thumbs_available(void);

/** Switch between snapshot and live rendering at runtime */
void watch_thumbs_set_enabled(bool en);
bool watch_thumbs_is_enabled(void);

/** Depth of a card in the depth effect: 0 = in focus .. WATCH_DEPTH_MAX */
#define WATCH_DEPTH_MAX 256

/** Keep blurred copies of the picker cards (depth effect on) or not */
void watch_thumbs_set_depth_mode(bool en);

/**
 * Depth effect in snapshot mode: scale, cross-fade to the pre-blurred copy
 * and darken card `card` (index in the carousel). No-op in live mode.
 */
void watch_thumbs_set_depth(uint32_t card, int32_t depth);

/** Called once per second, after the clock hands moved */
void watch_thumbs_tick(void);

void watch_thumbs_get_stats(watch_thumbs_stats_t * stats);
void watch_thumbs_reset_stats(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WATCH_THUMBS_H*/
