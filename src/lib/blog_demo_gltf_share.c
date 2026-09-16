/**
 * @file blog_demo_gltf_share.c
 *
 * LVGL v9.6 shared glTF models - blog GIF demo.
 *
 * One .glb is parsed once and handed to four independent lv_gltf viewers, each
 * looking at it from a different angle. Animating a single node of that one
 * model moves all four panels in lockstep, which is the visible proof that
 * they really are the same model and not four copies.
 *
 * The API this rests on is new in v9.6:
 *
 *  - lv_gltf_data_load_from_file() loads a model that is *not* owned by any
 *    viewer. lv_gltf_load_model_from_file() (v9.5) also works, but the viewer
 *    you loaded into owns the result, so it cannot outlive that viewer.
 *
 *  - lv_gltf_add_model() attaches a caller-owned model to a viewer, and may be
 *    called on as many viewers as you like. Ownership stays with the caller.
 *
 *  - lv_gltf_remove_model() detaches it again without destroying it. The demo
 *    cycles one panel through detach/re-attach so the GIF also shows that the
 *    model survives being dropped by a viewer.
 *
 * The GPU-side cost - textures, meshes, shaders - is paid once, not per viewer.
 * The IBL environment is shared the same way: built once and handed to all four,
 * since a viewer left without one builds its own.
 *
 * Requires LV_USE_GLTF=1.
 */

#include <lvgl/lvgl.h>
#include <stdlib.h>
#include "blog_demos.h"

#if LV_USE_GLTF

/*"A:" is LV_FS_STDIO; the path is relative to the working directory.*/
#define MODEL_DEFAULT_PATH "A:3d/lvgl_logo.glb"

#define VIEWER_CNT   4
#define SPIN_MS      15000
/*How long the fourth panel stays detached, and then re-attached.*/
#define TOGGLE_MS    2500

/*Per-viewer camera, so the same model reads differently in every panel.*/
static const struct {
    lv_align_t align;
    float pitch;
    float yaw;
} viewer_cfg[VIEWER_CNT] = {
    { LV_ALIGN_TOP_LEFT,     -30.0f, -30.0f },
    { LV_ALIGN_TOP_RIGHT,    -30.0f,  30.0f },
    { LV_ALIGN_BOTTOM_LEFT,   30.0f, -30.0f },
    { LV_ALIGN_BOTTOM_RIGHT,  30.0f,  30.0f },
};

static lv_obj_t * viewer[VIEWER_CNT];
static lv_gltf_model_t * shared_model;
static lv_obj_t * caption;
static bool last_attached;

static void spin_cb(void * var, int32_t v)
{
    lv_gltf_model_node_t * node = var;

    /*One node of one model - every viewer holding that model redraws.*/
    lv_gltf_model_node_set_rotation_y(node, (float)v);
}

static void toggle_cb(lv_timer_t * t)
{
    LV_UNUSED(t);

    last_attached = !last_attached;

    if(last_attached) {
        lv_gltf_add_model(viewer[VIEWER_CNT - 1], shared_model);
        lv_label_set_text(caption, "1 model  -  4 viewers");
    }
    else {
        /*The model itself is untouched; only this viewer lets go of it.*/
        lv_gltf_remove_model(viewer[VIEWER_CNT - 1], shared_model);
        lv_label_set_text(caption, "lv_gltf_remove_model()  -  model still alive");
    }
}

void blog_demo_gltf_share(void)
{
    const char * path = getenv("LV_GLTF_MODEL");
    if(path == NULL) path = MODEL_DEFAULT_PATH;

    lv_obj_t * scr = lv_screen_active();
    lv_obj_clean(scr);
    /*Dark ground, so the gaps between the four panels read as a grid.*/
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101014), 0);

    /*Parsed once. Not owned by any viewer - we own it.*/
    shared_model = lv_gltf_data_load_from_file(path, NULL);
    if(shared_model == NULL) {
        lv_obj_t * err = lv_label_create(scr);
        lv_label_set_text_fmt(err, "Could not load model:\n%s\n"
                              "Set LV_GLTF_MODEL to override.", path);
        lv_obj_set_style_text_align(err, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(err, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(err);
        LV_LOG_ERROR("glTF model not loadable: %s", path);
        return;
    }

    /*Built once too - four viewers left to their own devices would each build
     *their own copy of the same cube maps.*/
    lv_gltf_ibl_sampler_t * sampler = lv_gltf_ibl_sampler_create();
    lv_gltf_environment_t * env = lv_gltf_environment_create(sampler, NULL);
    lv_gltf_ibl_sampler_delete(sampler);

    for(uint32_t i = 0; i < VIEWER_CNT; i++) {
        viewer[i] = lv_gltf_create(scr);
        /*49% leaves a 2% seam down the middle of each axis.*/
        lv_obj_set_size(viewer[i], LV_PCT(49), LV_PCT(49));
        lv_obj_align(viewer[i], viewer_cfg[i].align, 0, 0);
        lv_obj_set_scrollable(viewer[i], false);

        lv_gltf_set_pitch(viewer[i], viewer_cfg[i].pitch);
        lv_gltf_set_yaw(viewer[i], viewer_cfg[i].yaw);
        lv_gltf_set_distance(viewer[i], 0.5f);
        lv_gltf_set_environment(viewer[i], env);
        lv_gltf_set_background_mode(viewer[i], LV_GLTF_BG_MODE_ENVIRONMENT);

        /*The same pointer, four times.*/
        lv_gltf_add_model(viewer[i], shared_model);
    }

    caption = lv_label_create(scr);
    lv_label_set_text(caption, "1 model  -  4 viewers");
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(caption, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(caption, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(caption, LV_OPA_50, 0);
    lv_obj_set_style_pad_all(caption, 8, 0);
    lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, -16);

    /*".0" is the first root node of the first scene - the whole logo here.*/
    lv_gltf_model_node_t * root = lv_gltf_model_node_get_by_numeric_path(shared_model, ".0");

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, root);
    lv_anim_set_exec_cb(&a, spin_cb);
    lv_anim_set_values(&a, 0, 360);
    lv_anim_set_duration(&a, SPIN_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    last_attached = true;
    lv_timer_create(toggle_cb, TOGGLE_MS, NULL);
}

#else /*LV_USE_GLTF*/

void blog_demo_gltf_share(void)
{
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Enable LV_USE_GLTF to run this demo");
    lv_obj_center(label);
}

#endif /*LV_USE_GLTF*/
