/**
 * @file watch_nav.h
 * The single navigation seam of the watch demo. Every screen change and
 * the shade go through these functions; phase 2 swaps their bodies for
 * snapshot-based transitions without touching the XML.
 */

#ifndef WATCH_NAV_H
#define WATCH_NAV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../smartwatch_faces.h"

void watch_nav_init(void);

void watch_nav_open_picker(void);
void watch_nav_select_face(int32_t face_index);        /* sets subject, back home */
void watch_nav_delete_face(int32_t face_index);        /* phase 1: hide card     */
void watch_nav_open_face_edit(int32_t face_index);
void watch_nav_open_gallery(void);                     /* picker -> face gallery  */
void watch_nav_close_face_edit(void);
void watch_nav_open_launcher(void);
void watch_nav_open_app(const char * app_id, lv_obj_t * origin);
void watch_nav_back(void);                             /* app -> launcher -> home */
void watch_nav_shade_open(bool open);

/** The screen watch_nav last loaded (or is loading) */
lv_obj_t * watch_nav_get_current(void);
bool watch_nav_shade_is_open(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WATCH_NAV_H*/
