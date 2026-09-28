/**
 * @file watch_depth.h
 * Depth-of-field carousel for the face picker: the centred card is sharp,
 * cards further from the centre get blurrier, smaller and darker, and the
 * background slides slowly the other way (parallax).
 *
 * The same effect runs in both picker modes, which is the point of it:
 * - live mode: blur_radius + transform_scale + recolor on every face tree,
 *   re-rendered and re-blurred every frame;
 * - snapshot mode: images scaled and cross-faded with a pre-blurred copy
 *   (watch_thumbs_set_depth()), no blur computed per frame.
 *
 * Off by default; WATCH_DEPTH=1 starts with it on.
 */

#ifndef WATCH_DEPTH_H
#define WATCH_DEPTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../smartwatch_faces.h"

/** Call once after the screens and watch_thumbs_init() */
void watch_depth_init(void);

void watch_depth_set_enabled(bool en);
bool watch_depth_is_enabled(void);

/** Re-apply the effect, e.g. after switching the snapshot mode */
void watch_depth_update(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WATCH_DEPTH_H*/
