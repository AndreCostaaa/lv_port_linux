/**
 * @file watch_compat.h
 * The hand-written C builds against LVGL v10 (lv_port_linux) and v9.5 (the
 * editor preview). v10 deprecates the generic flag API in favour of
 * per-flag setters; these wrappers pick the right one.
 */

#ifndef WATCH_COMPAT_H
#define WATCH_COMPAT_H

#include "../smartwatch_faces.h"

static inline void watch_obj_set_hidden(lv_obj_t * obj, bool en)
{
#if LVGL_VERSION_MAJOR >= 10
    lv_obj_set_hidden(obj, en);
#else
    lv_obj_set_flag(obj, LV_OBJ_FLAG_HIDDEN, en);
#endif
}

static inline bool watch_obj_is_hidden(const lv_obj_t * obj)
{
#if LVGL_VERSION_MAJOR >= 10
    return lv_obj_is_hidden(obj);
#else
    return lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
#endif
}

static inline void watch_obj_set_clickable(lv_obj_t * obj, bool en)
{
#if LVGL_VERSION_MAJOR >= 10
    lv_obj_set_clickable(obj, en);
#else
    lv_obj_set_flag(obj, LV_OBJ_FLAG_CLICKABLE, en);
#endif
}

static inline void watch_obj_set_overflow_visible(lv_obj_t * obj, bool en)
{
#if LVGL_VERSION_MAJOR >= 10
    lv_obj_set_overflow_visible(obj, en);
#else
    lv_obj_set_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE, en);
#endif
}

#endif /*WATCH_COMPAT_H*/
