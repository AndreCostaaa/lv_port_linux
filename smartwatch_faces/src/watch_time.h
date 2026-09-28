/**
 * @file watch_time.h
 * Clock and simulated sensors: updates the time/health subjects and
 * rotates the named clock hands of every live face instance.
 */

#ifndef WATCH_TIME_H
#define WATCH_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../smartwatch_faces.h"

/**
 * Collect the hands of all created screens and start the timers.
 * Call after every screen has been created.
 */
void watch_time_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WATCH_TIME_H*/
