/**
 * @file blog_demos.h
 *
 * Small self-contained demos used to record the GIFs for the LVGL v9.6
 * release blog post. Each one is a single entry point with no shared state,
 * so its body can be lifted straight into the post as a code snippet.
 *
 * Selected at runtime with the DEMO environment variable, e.g.
 *     DEMO=leading_trim ./build/bin/lvglsim -W 800 -H 480
 */

#ifndef BLOG_DEMOS_H
#define BLOG_DEMOS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run the blog demo matching `name`.
 * @param name  demo name, as given in the DEMO environment variable
 * @return      true if a demo matched and was started
 */
bool blog_demo_run(const char * name);

/** Cycle a label through the five LV_TEXT_LEADING_TRIM_* modes. */
void blog_demo_leading_trim(void);

/** Animate a variable font's weight axis from 100 to 900 and back. */
void blog_demo_var_weight(void);

/** Show one glTF model shared by four viewers, with live detach/re-attach. */
void blog_demo_gltf_share(void);

#ifdef __cplusplus
}
#endif

#endif /*BLOG_DEMOS_H*/
