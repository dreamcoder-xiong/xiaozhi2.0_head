#include "../ui.h"
#include "lv_100ask_2048.h"
bool game_screen_flag = 0;
lv_obj_t *ui_game_screen;
lv_obj_t *ui_game_page;
lv_obj_t *ui_game_2048;
lv_obj_t *ui_game_score;
void ui_event_game(lv_event_t *e)
{

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *game_handle = lv_event_get_user_data(e);
    if (event_code == LV_EVENT_SCREEN_LOAD_START)
    {
        // ESP_LOGI(TAG, "### Load Game ###");
        lv_obj_set_parent(ui_title_page, ui_game_page);
    }
    if ((event_code == LV_EVENT_RIGHT_LONG_CLICK))
    {
        _ui_screen_change(&ui_weather_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_weather_screen_init);
        game_screen_flag = 0;
    }
    if ((event_code == LV_EVENT_LEFT_LONG_CLICK))
    {
        _ui_screen_change(&ui_muyu_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_muyu_screen_init);
        game_screen_flag = 0;
    }
    if (event_code == LV_EVENT_UP_LONG_CLICK || event_code == LV_EVENT_DOWN_LONG_CLICK)
    {
        lv_100ask_2048_set_new_game(game_handle);
    }
    if (event_code == LV_EVENT_UP_CLICK)
    {
        uint8_t key = LV_KEY_UP;
        lv_event_send(ui_game_2048, LV_EVENT_KEY, &key);
        game_screen_flag = 1;
    }
    if (event_code == LV_EVENT_DOWN_CLICK)
    {
        uint8_t key = LV_KEY_DOWN;
        lv_event_send(ui_game_2048, LV_EVENT_KEY, &key);
        game_screen_flag = 1;
    }
    if (event_code == LV_EVENT_LEFT_CLICK)
    {
        if (game_screen_flag)
        {
            uint8_t key = LV_KEY_LEFT;
            lv_event_send(ui_game_2048, LV_EVENT_KEY, &key);
        }
        else
        {
            _ui_screen_change(&ui_muyu_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_muyu_screen_init);
        }
    }
    if (event_code == LV_EVENT_RIGHT_CLICK)
    {
        if (game_screen_flag)
        {
            uint8_t key = LV_KEY_RIGHT;
            lv_event_send(ui_game_2048, LV_EVENT_KEY, &key);
        }
        else
        {
            _ui_screen_change(&ui_weather_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_weather_screen_init);
        }
    }

}
void ui_event_game_score(lv_event_t *e)
{

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *label = lv_event_get_user_data(e);

    if (event_code == LV_EVENT_VALUE_CHANGED)
    {
        if (lv_100ask_2048_get_best_tile(target) >= 2048)
        {
            lv_label_set_text(label, "#00b329 YOU WIN! #");
        }
        else if (lv_100ask_2048_get_status(target))
        {
            lv_label_set_text(label, "#ff0000 GAME OVER! #");
        }
        else
        {
            lv_label_set_text_fmt(label, "#FFFFFF SCORE:# #FF0000 %d#", lv_100ask_2048_get_score(target));
        }
    }
}
void ui_game_screen_init(void)
{
    game_screen_flag = 0;
    ui_game_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_game_screen, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_game_page = lv_obj_create(ui_game_screen);
    lv_obj_set_width(ui_game_page, 240);
    lv_obj_set_height(ui_game_page, 240);
    lv_obj_set_align(ui_game_page, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_game_page, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_radius(ui_game_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_game_page, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_game_page, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_game_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_game_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_game_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_game_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_game_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_game_2048 = lv_100ask_2048_create(ui_game_page);
    lv_obj_set_size(ui_game_2048, 240, 200);
    lv_obj_set_align(ui_game_2048, LV_ALIGN_BOTTOM_MID);

    ui_game_score = lv_label_create(ui_game_page);
    lv_label_set_recolor(ui_game_score, true);
    lv_label_set_text_fmt(ui_game_score, "#FFFFFF SCORE:# #FF0000 %d#", lv_100ask_2048_get_score(ui_game_2048));
    lv_obj_align_to(ui_game_score, ui_game_2048, LV_ALIGN_TOP_MID, 0, -15);

    lv_obj_add_event_cb(ui_game_screen, ui_event_game, LV_EVENT_ALL, ui_game_2048);
    lv_obj_add_event_cb(ui_game_2048, ui_event_game_score, LV_EVENT_ALL, ui_game_score);
}