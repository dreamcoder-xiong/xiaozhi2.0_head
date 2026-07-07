#include "../ui.h"
// #include "app_wifi.h"
lv_obj_t *ui_title_screen;
lv_obj_t *ui_title_page;
lv_obj_t *ui_title_wifistate;
lv_obj_t *ui_title_powerstate;
lv_obj_t *ui_title_batterybor;
lv_obj_t *ui_title_timestate;
lv_obj_t *ui_title_batterytxt;
lv_obj_t *ui_title_state;
lv_obj_t *wifi_ip_text;
void ui_title_screen_init(void)
{
    ui_title_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_title_screen, LV_OBJ_FLAG_SCROLLABLE);

    ui_title_page = lv_obj_create(ui_title_screen);
    lv_obj_set_width(ui_title_page, 240);
    lv_obj_set_height(ui_title_page, 25);
    lv_obj_set_align(ui_title_page, LV_ALIGN_TOP_MID);
    /* 标题栏往上移加入*/
    lv_obj_set_y(ui_title_page, -2);   // 或 -1 / -2


    lv_obj_clear_flag(ui_title_page, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_radius(ui_title_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_title_page, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_title_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    //lv_obj_set_style_border_color(ui_title_page, lv_color_hex(0xF00000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_title_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    //lv_obj_set_style_border_opa(ui_title_page, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    //lv_obj_set_style_border_side(ui_title_page, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_title_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_title_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_title_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_title_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_title_state = lv_label_create(ui_title_page);
    lv_obj_set_width(ui_title_state, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_title_state, LV_SIZE_CONTENT); /// 1

    lv_obj_set_x(ui_title_state, -90);
    lv_obj_set_y(ui_title_state, 7);
    lv_obj_set_align(ui_title_state, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_title_state, "");
    lv_obj_set_style_text_color(ui_title_state, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_title_state, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_title_state, &font_puhui_14_1, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_title_wifistate = lv_img_create(ui_title_page);
    lv_img_set_src(ui_title_wifistate, &ui_img_wifi_disconnection_png);
    lv_obj_set_width(ui_title_wifistate, LV_SIZE_CONTENT);  /// 10
    lv_obj_set_height(ui_title_wifistate, LV_SIZE_CONTENT); /// 10
    lv_obj_set_x(ui_title_wifistate, 50);
    lv_obj_set_y(ui_title_wifistate, 6);
    lv_obj_set_align(ui_title_wifistate, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_title_wifistate, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_title_wifistate, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_img_set_zoom(ui_title_wifistate, 400);
    lv_obj_set_style_img_recolor(ui_title_wifistate, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_title_wifistate, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    wifi_ip_text = lv_label_create(ui_title_page);
    lv_obj_set_width(wifi_ip_text, 120);  /// 1
    lv_obj_set_height(wifi_ip_text, 20); /// 1
   
    lv_obj_set_x(wifi_ip_text, 3);
    lv_obj_set_y(wifi_ip_text, 30);
    //lv_obj_set_align(wifi_ip_text, LV_ALIGN_TOP_LEFT);
    //lv_obj_set_x(wifi_ip_text, 0);
    //lv_obj_set_y(wifi_ip_text, 7);
    lv_obj_set_align(wifi_ip_text, LV_ALIGN_TOP_MID);
    lv_label_set_text(wifi_ip_text, "");
    lv_obj_set_style_text_color(wifi_ip_text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(wifi_ip_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(wifi_ip_text, &font_puhui_14_1, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_title_powerstate = lv_slider_create(ui_title_page);
    lv_slider_set_value(ui_title_powerstate, 50, LV_ANIM_OFF);
    if (lv_slider_get_mode(ui_title_powerstate) == LV_SLIDER_MODE_RANGE)
    {
        lv_slider_set_left_value(ui_title_powerstate, 0, LV_ANIM_OFF);
    }
    lv_obj_set_width(ui_title_powerstate, 15);
    lv_obj_set_height(ui_title_powerstate, 4);
    lv_obj_set_x(ui_title_powerstate, 104);
    lv_obj_set_y(ui_title_powerstate, 9);
    lv_obj_set_align(ui_title_powerstate, LV_ALIGN_TOP_MID);
    lv_obj_set_style_radius(ui_title_powerstate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_title_powerstate, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_title_powerstate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_opa(ui_title_powerstate, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_title_powerstate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_title_powerstate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_title_powerstate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_title_powerstate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_title_powerstate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_title_powerstate, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_title_powerstate, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_title_powerstate, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_title_powerstate, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_title_powerstate, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    ui_title_batterybor = lv_img_create(ui_title_page);
    lv_img_set_src(ui_title_batterybor, &ui_img_battery_png);
    lv_obj_set_width(ui_title_batterybor, LV_SIZE_CONTENT);  /// 20
    lv_obj_set_height(ui_title_batterybor, LV_SIZE_CONTENT); /// 20
    lv_obj_set_x(ui_title_batterybor, 106);
    lv_obj_set_y(ui_title_batterybor, 1);
    lv_obj_set_align(ui_title_batterybor, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_title_batterybor, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_title_batterybor, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_img_set_zoom(ui_title_batterybor, 400);
    lv_obj_set_style_img_recolor(ui_title_batterybor, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_title_batterybor, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_title_timestate = lv_label_create(ui_title_page);
    lv_obj_set_width(ui_title_timestate, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_title_timestate, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_title_timestate, -97);
    lv_obj_set_y(ui_title_timestate, 3);
    lv_obj_set_align(ui_title_timestate, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_title_timestate, "");
    lv_obj_set_style_text_color(ui_title_timestate, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_title_timestate, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_title_timestate, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_title_batterytxt = lv_label_create(ui_title_page);
    lv_obj_set_width(ui_title_batterytxt, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_title_batterytxt, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_title_batterytxt, 77);
    lv_obj_set_y(ui_title_batterytxt, 3);
    lv_obj_set_align(ui_title_batterytxt, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_title_batterytxt, "--%%");
    lv_obj_set_style_text_color(ui_title_batterytxt, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_title_batterytxt, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_title_batterytxt, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
}
