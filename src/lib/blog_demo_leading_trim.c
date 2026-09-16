/**
 * @file blog_demo_leading_trim.c
 *
 * LVGL v9.6 `text_leading_trim` - blog GIF demo.
 *
 * Cycles a specimen word through the five LV_TEXT_LEADING_TRIM_* modes.
 * Every label carries a translucent background, so what you see shrinking
 * is the *text box* collapsing onto the glyphs as the trim tightens.
 *
 * The button below is the practical payoff: sized with LV_SIZE_CONTENT, its
 * height is driven by that same text box. With NONE it carries the font's
 * phantom leading and the label sits optically low; with CAPITAL_BASELINE
 * it hugs the glyphs.
 */

#include <lvgl/core/lv_area.h>
#include <lvgl/core/lv_obj_pos.h>
#include <lvgl/lvgl.h>
#include "blog_demos.h"

/* "Handgloves" is the classic type specimen: H gives cap-height, d/l give
 * ascenders, g gives a descender, and the rest sit at x-height. Every edge
 * the five trim modes reference is visible in one word. */
#define SPECIMEN "Handgloves"

#define STEP_MS 1600

typedef struct {
    lv_text_leading_trim_t mode;
    const char * name;
    const char * desc;
} trim_step_t;

static const trim_step_t steps[] = {
    { LV_TEXT_LEADING_TRIM_NONE,             "NONE",             "default - full line height" },
    { LV_TEXT_LEADING_TRIM_CAPITAL,          "CAPITAL",          "top to cap-height" },
    { LV_TEXT_LEADING_TRIM_LOWER,            "LOWER",            "top to x-height" },
    { LV_TEXT_LEADING_TRIM_CAPITAL_BASELINE, "CAPITAL_BASELINE", "cap-height to baseline" },
    { LV_TEXT_LEADING_TRIM_LOWER_BASELINE,   "LOWER_BASELINE",   "x-height to baseline" },
};

#define STEP_CNT (sizeof(steps) / sizeof(steps[0]))

typedef struct {
    lv_obj_t * specimen;
    lv_obj_t * btn_label;
    lv_obj_t * mode_label;
    lv_obj_t * desc_label;
    uint32_t idx;
} ctx_t;

static void apply_step(ctx_t * ctx)
{
    const trim_step_t * s = &steps[ctx->idx];

    lv_obj_set_style_text_leading_trim(ctx->specimen, s->mode, LV_PART_MAIN);
    lv_obj_set_style_text_leading_trim(ctx->btn_label, s->mode, LV_PART_MAIN);

    lv_label_set_text(ctx->mode_label, s->name);
    lv_label_set_text(ctx->desc_label, s->desc);
}

static void step_timer_cb(lv_timer_t * t)
{
    ctx_t * ctx = lv_timer_get_user_data(t);
    ctx->idx = (ctx->idx + 1) % STEP_CNT;
    apply_step(ctx);
}

void blog_demo_leading_trim(void)
{
    static ctx_t ctx;

    lv_obj_t * scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    // lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    // lv_obj_set_style_flex_main_place(scr, LV_FLEX_ALIGN_CENTER, 0);
    // lv_obj_set_style_flex_cross_place(scr, LV_FLEX_ALIGN_CENTER, 0);
    // lv_obj_set_style_pad_row(scr, 20, 0);
    // lv_obj_set_style_pad_all(scr, 28, 0);

    lv_obj_t * title_container = lv_obj_create(scr);
    lv_obj_set_style_bg_opa(title_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(title_container, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(title_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(title_container, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_cross_place(title_container, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_align(title_container, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_width(title_container, LV_SIZE_CONTENT);
    lv_obj_set_height(title_container, 200);
    /*Name the mode in the property's own vocabulary*/
    ctx.mode_label = lv_label_create(title_container);
    lv_obj_set_style_text_font(ctx.mode_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ctx.mode_label, lv_color_hex(0x1565C0), 0);

    ctx.desc_label = lv_label_create(title_container);
    lv_obj_set_style_text_font(ctx.desc_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ctx.desc_label, lv_color_hex(0x757575), 0);

    lv_obj_t * content_container = lv_obj_create(scr);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_align(content_container, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(content_container, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_cross_place(content_container, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_width(content_container, LV_SIZE_CONTENT);
    lv_obj_set_height(content_container, 200);
    /*The specimen. The translucent box IS the text box - it is what moves.*/
    ctx.specimen = lv_label_create(content_container);
    lv_label_set_text(ctx.specimen, SPECIMEN);
    lv_obj_set_style_text_font(ctx.specimen, &lv_font_montserrat_40, 0);
    lv_obj_set_style_bg_color(ctx.specimen, lv_color_hex(0xF06292), 0);
    lv_obj_set_style_bg_opa(ctx.specimen, LV_OPA_30, 0);

    /*The payoff: a button whose height follows the trimmed text box*/
    lv_obj_t * btn = lv_button_create(content_container);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(btn, 24, 0);
    lv_obj_set_style_pad_ver(btn, 12, 0);

    ctx.btn_label = lv_label_create(btn);
    lv_label_set_text(ctx.btn_label, "Continue");
    lv_obj_set_style_text_font(ctx.btn_label, &lv_font_montserrat_20, 0);

    ctx.idx = 0;
    apply_step(&ctx);

    lv_timer_create(step_timer_cb, STEP_MS, &ctx);
}
