#include "../ui.h"
#include "lv_lottie.h"
#include "esp_random.h"

lv_obj_t *ui_fish_screen;
lv_obj_t *ui_fish_page;
lv_obj_t *ui_fish_canvas;
#define LOTTIE_SIZE_HOR_MIN (60)
#define LOTTIE_SIZE_VER_MIN (60)
#define LOTTIE_SIZE_HOR (120)
#define LOTTIE_SIZE_VER (120)

static lv_timer_t *timer;
static uint8_t *lottie_buff = NULL;
static int now_x = 0;
static uint16_t default_width = LOTTIE_SIZE_VER * 0.5;
static uint16_t default_height = LOTTIE_SIZE_HOR * 0.5;
#define TAG "UI_FISH"
static void my_timer(lv_timer_t *timer)
{

    now_x += 2;
    if (now_x > 120 + default_width / 2)
    {
        now_x = -120 - default_width / 2;
        uint32_t random_num = esp_random();

        int scaled_num = (int)((random_num % 161) - 80);
        lv_obj_set_y(ui_fish_canvas, scaled_num);
    }
    lv_obj_set_x(ui_fish_canvas, now_x);

}
void ui_event_fish(lv_event_t *e)
{

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    if (event_code == LV_EVENT_SCREEN_UNLOAD_START)
    {
        lv_timer_del(timer);
    }
    if (event_code == LV_EVENT_SCREEN_LOAD_START)
    {
        lv_obj_set_parent(ui_title_page, ui_fish_page);

        const void *data = mmap_assets_get_mem(asset_lottie, MMAP_LOTTIE_ASSETS_FISH_JSON);
        size_t size = mmap_assets_get_size(asset_lottie, MMAP_LOTTIE_ASSETS_FISH_JSON);
        lv_lottie_set_src_data(ui_fish_canvas, data, size);
        lv_lottie_set_segment(ui_fish_canvas, 1, 36 - 1, true);
        // timer_fish = lv_timer_create(fish_lottie_timer_player_cb, 10, NULL);
        now_x = -120 - default_width / 2;
        timer = lv_timer_create(my_timer, 30, NULL);
    }
    if (event_code == LV_EVENT_UP_CLICK)
    {
        default_width *= 1.1;
        default_height *= 1.1;
        if (default_width >= LOTTIE_SIZE_HOR || default_height > LOTTIE_SIZE_VER)
        {
            default_width = LOTTIE_SIZE_HOR;
            default_height = LOTTIE_SIZE_VER;
        }
        ESP_LOGI(TAG, "Resize Fish:[%d, %d]", default_width, default_height);
        lv_lottie_resize(ui_fish_canvas, default_width, default_height);
        lv_lottie_set_segment(ui_fish_canvas, 40, 96 - 1, true);
    }
    if (event_code == LV_EVENT_DOWN_CLICK)
    {
        default_width *= 0.9;
        default_height *= 0.9;
        if (default_width <= LOTTIE_SIZE_HOR_MIN || default_height < LOTTIE_SIZE_VER_MIN)
        {
            default_width = LOTTIE_SIZE_HOR_MIN;
            default_height = LOTTIE_SIZE_VER_MIN;
        }
        ESP_LOGI(TAG, "Resize Fish:[%d, %d]", default_width, default_height);
        lv_lottie_resize(ui_fish_canvas, default_width, default_height);
        lv_lottie_set_segment(ui_fish_canvas, 40, 96 - 1, true);
    }
    if (event_code == LV_EVENT_LEFT_CLICK)
    {
        _ui_screen_change(&ui_face_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_face_screen_init);
    }
    if (event_code == LV_EVENT_RIGHT_CLICK)
    {
        _ui_screen_change(&ui_game_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_game_screen_init);
    }

}

void ui_fish_screen_init(void)
{
    ui_fish_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_fish_screen, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    
    ui_fish_page = lv_obj_create(ui_fish_screen);
    lv_obj_set_width(ui_fish_page, 240);
    lv_obj_set_height(ui_fish_page, 240);
    lv_obj_set_align(ui_fish_page, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_fish_page, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_radius(ui_fish_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_fish_page, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_fish_page, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_fish_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_fish_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_fish_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_fish_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_fish_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_fish_canvas = lv_lottie_create(ui_fish_page);
    lv_obj_align(ui_fish_canvas, LV_ALIGN_CENTER, -240, 0);

    static uint8_t *fb = NULL;
    ;
    if (fb == NULL)
    {
        fb = malloc(LOTTIE_SIZE_HOR_MIN * LOTTIE_SIZE_VER_MIN * 4);
        assert(fb);
        lv_lottie_set_buffer(ui_fish_canvas, LOTTIE_SIZE_HOR_MIN, LOTTIE_SIZE_VER_MIN, fb);
    }

    lv_obj_add_event_cb(ui_fish_screen, ui_event_fish, LV_EVENT_ALL, NULL);
}
