/**
 * @file watch_callbacks.h
 * The watch_on_* event callbacks are declared by the generated code; this
 * header only exposes the input tuning they share with lv_demo_watch().
 */

#ifndef WATCH_CALLBACKS_H
#define WATCH_CALLBACKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../smartwatch_faces.h"

/** Gesture thresholds for a small round screen (pointer devices only) */
void watch_callbacks_tune_indev(lv_indev_t * indev);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WATCH_CALLBACKS_H*/
