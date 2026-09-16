/**
 * @file blog_demos.c
 *
 * Runtime dispatcher for the LVGL v9.6 blog GIF demos.
 *
 * Kept separate from the Kconfig-driven demo chain in main.c so that
 * recording a GIF needs no reconfigure - just a different DEMO value.
 */

#include <lvgl/lvgl.h>
#include <stdio.h>
#include <string.h>
#include "blog_demos.h"

typedef struct {
    const char * name;
    void (*fn)(void);
} blog_demo_entry_t;

static const blog_demo_entry_t entries[] = {
    { "leading_trim", blog_demo_leading_trim },
    { "var_weight",   blog_demo_var_weight },
    { "gltf_share",   blog_demo_gltf_share },
};

#define ENTRY_CNT (sizeof(entries) / sizeof(entries[0]))

bool blog_demo_run(const char * name)
{
    if(name == NULL || name[0] == '\0') return false;

    for(uint32_t i = 0; i < ENTRY_CNT; i++) {
        if(strcmp(entries[i].name, name) == 0) {
            entries[i].fn();
            return true;
        }
    }

    /*A typo here would otherwise silently record the wrong demo*/
    fprintf(stderr, "DEMO=\"%s\" is not a blog demo. Available:\n", name);
    for(uint32_t i = 0; i < ENTRY_CNT; i++) {
        fprintf(stderr, "  %s\n", entries[i].name);
    }

    return false;
}
