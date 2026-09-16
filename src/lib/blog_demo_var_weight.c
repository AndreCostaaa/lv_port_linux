/**
 * @file blog_demo_var_weight.c
 *
 * LVGL v9.6 variable-weight FreeType fonts - blog GIF demo.
 *
 * Animates a word along the font's `wght` axis from 100 to 900 and back.
 *
 * Note on the approach: the weight is baked into the font at creation time -
 * it is part of the FreeType cache-node key, and FT_Set_Var_Design_Coordinates
 * is applied once when the face is opened. There is no setter to re-weight a
 * live lv_font_t. So we pre-create one font per weight step and swap the
 * label's font each frame, which is cheap. Creating a font per frame would
 * re-open the .ttf every time and is exactly what to avoid.
 *
 * Requires LV_USE_FREETYPE=1.
 */

#include <lvgl/lvgl.h>
#include <stdlib.h>
#include "blog_demos.h"

#if LV_USE_FREETYPE

/*Shipped with LVGL - the only variable font in the tree (wght axis 100-900)*/
#define VF_DEFAULT_PATH "lvgl/tests/src/test_files/fonts/Montserrat-VariableFont.ttf"

#define W_MIN    100
#define W_MAX    900
/*Step 25 gives 33 faces - fine grained enough to read as smooth, while
 *keeping the number of open FT_Faces (and their glyph caches) sane.*/
#define W_STEP    25
#define FONT_SIZE 72

#define N_FONTS (((W_MAX - W_MIN) / W_STEP) + 1)

typedef struct {
    lv_font_t * fonts[N_FONTS];
    lv_obj_t * word;
    lv_obj_t * readout;
    uint32_t created;
} ctx_t;

static ctx_t ctx;

static lv_font_t * make_font(const char * path, int32_t weight)
{
    lv_font_info_t info;
    lv_freetype_init_font_info(&info);
    info.name        = path;
    info.size        = FONT_SIZE;
    info.render_mode = LV_FREETYPE_FONT_RENDER_MODE_BITMAP;
    info.style       = LV_FREETYPE_FONT_STYLE_NORMAL;
    info.weight      = weight;   /*1..2000, clamped to the font's own wght axis*/
    return lv_freetype_font_create_with_info(&info);
}

static void weight_anim_cb(void * var, int32_t weight)
{
    LV_UNUSED(var);

    int32_t idx = (weight - W_MIN) / W_STEP;
    if(idx < 0) idx = 0;
    if(idx >= (int32_t)ctx.created) idx = (int32_t)ctx.created - 1;

    lv_obj_set_style_text_font(ctx.word, ctx.fonts[idx], 0);
    lv_label_set_text_fmt(ctx.readout, "wght %" LV_PRId32, (int32_t)(W_MIN + idx * W_STEP));
}

void blog_demo_var_weight(void)
{
    const char * path = getenv("LV_VF_PATH");
    if(path == NULL) path = VF_DEFAULT_PATH;

    lv_obj_t * scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(scr, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_cross_place(scr, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_track_place(scr, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_row(scr, 24, 0);

    ctx.created = 0;
    for(uint32_t i = 0; i < N_FONTS; i++) {
        ctx.fonts[i] = make_font(path, W_MIN + (int32_t)i * W_STEP);
        if(ctx.fonts[i] == NULL) break;
        ctx.created++;
    }

    if(ctx.created == 0) {
        lv_obj_t * err = lv_label_create(scr);
        lv_label_set_text_fmt(err, "Could not open variable font:\n%s\n"
                              "Set LV_VF_PATH to override.", path);
        lv_obj_set_style_text_color(err, lv_color_hex(0xEF5350), 0);
        lv_obj_set_style_text_align(err, LV_TEXT_ALIGN_CENTER, 0);
        LV_LOG_ERROR("variable font not loadable: %s", path);
        return;
    }

    ctx.word = lv_label_create(scr);
    lv_label_set_text(ctx.word, "Variable");
    lv_obj_set_style_text_font(ctx.word, ctx.fonts[0], 0);
    lv_obj_set_style_text_color(ctx.word, lv_color_hex(0xFFFFFF), 0);

    ctx.readout = lv_label_create(scr);
    lv_obj_set_style_text_font(ctx.readout, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ctx.readout, lv_color_hex(0x64B5F6), 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ctx.word);
    lv_anim_set_values(&a, W_MIN, W_MAX);
    lv_anim_set_duration(&a, 2200);
    lv_anim_set_reverse_duration(&a, 2200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, weight_anim_cb);
    lv_anim_start(&a);
}

#else /*LV_USE_FREETYPE*/

void blog_demo_var_weight(void)
{
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Enable LV_USE_FREETYPE to run this demo");
    lv_obj_center(label);
}

#endif /*LV_USE_FREETYPE*/
