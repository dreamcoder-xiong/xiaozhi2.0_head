#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
extern "C" {
#endif

void ui_face_set_status_text(const char *text);

#ifdef __cplusplus
}
#endif

#include "sdkconfig.h"
#include <string.h>
#include "lvgl.h"
// #include "lv_lottie.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include "ui_helpers.h"
#include "../app_animation.h"
#include "../lottie_player/lv_lottie.h"
#include "../config.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"


#define LV_EVENT_LEFT_CLICK             (_LV_EVENT_LAST + 10)
#define LV_EVENT_RIGHT_CLICK            (_LV_EVENT_LAST + 11)
#define LV_EVENT_UP_CLICK               (_LV_EVENT_LAST + 12)
#define LV_EVENT_DOWN_CLICK             (_LV_EVENT_LAST + 13)

#define LV_EVENT_LEFT_LONG_CLICK        (_LV_EVENT_LAST + 14)
#define LV_EVENT_RIGHT_LONG_CLICK       (_LV_EVENT_LAST + 15)
#define LV_EVENT_UP_LONG_CLICK          (_LV_EVENT_LAST + 16)
#define LV_EVENT_DOWN_LONG_CLICK        (_LV_EVENT_LAST + 17)

enum{
    UI_EVENT_AUDIO_PLAY_MUYU = 1,
    UI_EVENT_AUDIO_WAKENET_START,
    UI_EVENT_AUDIO_UP,
    UI_EVENT_AUDIO_DOWN,
    UI_EVENT_WIFI_AP_MODE,
    UI_EVENT_XIAOZHI_LEAVE,
    UI_EVENT_XIAOZHI_DOWN,
};

LV_IMG_DECLARE(ui_img_clody_png);    // assets/clody.png
LV_IMG_DECLARE(ui_img_wifi_png);    // assets/wifi.png
LV_IMG_DECLARE(ui_img_battery_png);    // assets/battery.png
LV_IMG_DECLARE(ui_img_muyu_png);    // assets/muyu.png
LV_IMG_DECLARE(ui_img_wifi_disconnection_png);   // assets/ui_img_wifi_disconnection.png

LV_FONT_DECLARE(ui_font_ComfortaaBold75);
LV_FONT_DECLARE(ui_font_ComfortaaBold9);
LV_FONT_DECLARE(ui_font_ComfortaaLight70);
LV_FONT_DECLARE(ui_font_ComfortaaBold40);
LV_FONT_DECLARE(ui_font_ComfortaaBold20);
LV_FONT_DECLARE(ui_font_OPPOSansH18);
LV_FONT_DECLARE(ui_font_OPPOSansH25);
LV_FONT_DECLARE(ui_font_OPPOSansL70);
LV_FONT_DECLARE(font_puhui_14_1);

void muyushow_Animation(lv_obj_t *TargetObject, int delay);
void blink_Animation(lv_obj_t *TargetObject, int delay);
void falldown_Animation(lv_obj_t *TargetObject, int delay);
void weather_Animation(lv_obj_t *TargetObject, int delay);
void riseup_Animation(lv_obj_t *TargetObject, int delay);
void opaon_Animation(lv_obj_t *TargetObject, int delay);
void gonde_txt_Animation(lv_obj_t *TargetObject, int delay);

// 对话表情页面
extern lv_obj_t *ui_face_screen;
extern lv_obj_t *ui_face_page;
extern lv_obj_t *ui_face_canvas;
void ui_face_screen_init(void);

// 天气页面
extern lv_obj_t *ui_weather_screen;
extern lv_obj_t *ui_weather_page;
extern lv_obj_t *ui_weather_hour;
extern lv_obj_t *ui_weather_min;
extern lv_obj_t *ui_weather_date;
extern lv_obj_t *ui_weather_weekday;
extern lv_obj_t *ui_weather_temp;
extern lv_obj_t *ui_weather_weather;
extern lv_obj_t *ui_weather_colon;
extern lv_obj_t *ui_weather_weathershow;
void ui_weather_screen_init(void);

// 木鱼页面
void ui_muyu_screen_init(void);
extern lv_obj_t *ui_muyu_screen;
extern lv_obj_t *ui_muyu_page;
extern lv_obj_t *ui_muyu_img;
extern lv_obj_t *ui_muyu_gongdetxt;
extern lv_obj_t *ui_muyu_gongde_sum;

// 摸鱼页面
void ui_fish_screen_init(void);
extern lv_obj_t *ui_fish_screen;
extern lv_obj_t *ui_fish_page;
extern lv_obj_t *ui_fish_canvas;

// 2048游戏页面
extern lv_obj_t *ui_game_screen;
extern lv_obj_t *ui_game_page;
extern lv_obj_t *ui_game_2048;
extern lv_obj_t *ui_game_score;
void ui_game_screen_init(void);

// 摄像头页面
extern lv_obj_t *ui_camera_screen;
extern lv_obj_t *ui_camera_page;
extern lv_obj_t *ui_camera_canvas;
void ui_camera_screen_init(void);

// 标题栏
extern lv_obj_t *ui_title_screen;
extern lv_obj_t *ui_title_page;
extern lv_obj_t *ui_title_wifistate;
extern lv_obj_t *ui_title_powerstate;
extern lv_obj_t *ui_title_batterybor;
extern lv_obj_t *ui_title_timestate;
extern lv_obj_t *ui_title_batterytxt;
extern lv_obj_t *ui_title_state;
extern lv_obj_t *wifi_ip_text;
void ui_title_screen_init(void);

// 骰子页面
extern lv_obj_t *ui_dice_screen;
extern lv_obj_t *ui_dice_page;
extern lv_obj_t *ui_dice_shaizibut;
extern lv_obj_t *ui_dice_shaizitxt;
extern lv_obj_t *ui_dice_canvas;
//void ui_dice_screen_init(void);

// 小智页面
extern lv_obj_t *ui_xiaozhi;

void set_wifi_ip_text(const char *text);

typedef struct
{
    lv_obj_t *page;
    char *name;
} ui_page_name_t;
void ui_Initialize(void);
void ui_send_sys_event(lv_obj_t *obj, lv_event_code_t event_code, void *user_data);

extern QueueHandle_t ui_send_event;
#ifdef __cplusplus
} /*extern "C"*/
#endif