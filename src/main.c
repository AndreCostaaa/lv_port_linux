#include <render/lv_demo_render.h>

typedef struct
{
    const char * name;
    lv_color_format_t format;
} color_format_t;

void test_render(lv_display_t * display, const color_format_t * format)
{
    lv_display_set_color_format(display, format->format);

    lv_opa_t opa_values[2] = {0xff, 0x80};
    uint32_t opa;
    for(opa = 0; opa < 2; opa++) {
        uint32_t i;
        for(i = 0; i < LV_DEMO_RENDER_SCENE_NUM; i++) {

            /*Skip test with transformed indexed images if they are not loaded to RAM*/
            if(LV_BIN_DECODER_RAM_LOAD == 0 &&
               (i == LV_DEMO_RENDER_SCENE_IMAGE_NORMAL_2 || i == LV_DEMO_RENDER_SCENE_IMAGE_RECOLOR_2))
                continue;

            lv_demo_render(i, opa_values[opa]);

            char buf[128];
            lv_snprintf(buf, sizeof(buf), "draw/render/%s/demo_render_%s_opa_%d.png", format->name,
                        lv_demo_render_get_scene_name(i), opa_values[opa]);
            lv_test_screenshot_compare(buf);
        }
    }
}

/**
 * @brief entry point
 * @description start a demo
 * @param argc the count of arguments in argv
 * @param argv The arguments
 */
int main(int argc, char ** argv)
{
    color_format_t formats[] = {{.name = "rgb565", .format = LV_COLOR_FORMAT_RGB565},
                                {.name = "rgb888", .format = LV_COLOR_FORMAT_RGB888},
                                {.name = "xrgb8888", .format = LV_COLOR_FORMAT_XRGB8888}};
    lv_init();
    lv_display_t * display = lv_test_display_create(1024, 768);
    for(size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        test_render(display, &formats[i]);
    }

    return 0;
}
