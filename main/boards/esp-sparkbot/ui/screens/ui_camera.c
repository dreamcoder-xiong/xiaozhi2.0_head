#include "../ui.h"
#include "esp_camera.h"

lv_obj_t *ui_camera_screen;
lv_obj_t *ui_camera_page;
lv_obj_t *ui_camera_canvas;
static lv_timer_t *timer_camera = NULL;
static const char *TAG = "ui_camera";
static void win_camera_timer_cb(lv_timer_t *tmr)
{

    camera_fb_t *pic = esp_camera_fb_get();
    if (pic)
    {
        esp_camera_fb_return(pic);
        lv_canvas_set_buffer(ui_camera_canvas, pic->buf, DISPLAY_WIDTH, DISPLAY_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    }

}
void ui_event_camera(lv_event_t *e)
{

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (event_code == LV_EVENT_SCREEN_UNLOAD_START)
    {
        if (timer_camera)
        {
            lv_timer_del(timer_camera);
            timer_camera = NULL;
        }
    }
    if (event_code == LV_EVENT_SCREEN_LOAD_START)
    {
        ESP_LOGI(TAG, "start camera");
        // lv_obj_set_parent(ui_title_page, ui_camera_page);
        timer_camera = lv_timer_create(win_camera_timer_cb, 10, NULL);
    }
    if (event_code == LV_EVENT_RIGHT_CLICK)
    {
        _ui_screen_change(&ui_weather_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_weather_screen_init);
    }
        if (event_code == LV_EVENT_LEFT_CLICK)
    {
       // _ui_screen_change(&ui_dice_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_dice_screen_init);
    }

}
void ui_camera_screen_init(void)
{
    ui_camera_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_camera_screen, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_camera_page = lv_obj_create(ui_camera_screen);
    lv_obj_set_width(ui_camera_page, 240);
    lv_obj_set_height(ui_camera_page, 240);
    lv_obj_set_align(ui_camera_page, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_camera_page, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_radius(ui_camera_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_camera_page, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_camera_page, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_camera_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_camera_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_camera_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_camera_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_camera_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_camera_canvas = lv_canvas_create(ui_camera_page);
    lv_obj_align(ui_camera_canvas, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(ui_camera_canvas, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_add_event_cb(ui_camera_screen, ui_event_camera, LV_EVENT_ALL, NULL);
}