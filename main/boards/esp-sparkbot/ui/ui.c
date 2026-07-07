// 屏幕切换
#include "ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define TAG "ui"
ui_page_name_t ui_pages[7];

lv_obj_t *ui_xiaozhi;
extern lv_obj_t *wifi_ip_label;
typedef struct
{
    lv_obj_t *obj;
    lv_event_code_t event_code;
    void *user_data;
    void *param;
} lv_sys_event_t;
QueueHandle_t ui_sys_event = NULL;
QueueHandle_t ui_send_event = NULL;
void ui_system_update(lv_timer_t *timer)
{

    lv_sys_event_t event;
    if (pdPASS == xQueueReceive(ui_sys_event, &event, 0))
    {
        lvgl_port_lock(0);

        // ESP_LOGI(TAG, "event_code:%d", event.event_code);
        lv_event_send(event.obj, event.event_code, event.user_data);

        lvgl_port_unlock();
    }
}
void ui_event_xiaozhi(lv_event_t *e)
{
    lvgl_port_lock(0);

    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_SCREEN_LOAD_START)
    {
        lv_obj_clear_flag(ui_title_state, LV_OBJ_FLAG_HIDDEN);   /// Flags
        lv_obj_add_flag(ui_title_timestate, LV_OBJ_FLAG_HIDDEN); /// Flags

        lv_obj_set_parent(ui_title_page, ui_xiaozhi);
    }
    if (event_code == LV_EVENT_SCREEN_UNLOAD_START)
    {
        lv_obj_add_flag(ui_title_state, LV_OBJ_FLAG_HIDDEN);       /// Flags
        lv_obj_clear_flag(ui_title_timestate, LV_OBJ_FLAG_HIDDEN); /// Flags
    }
    if (event_code == LV_EVENT_RIGHT_CLICK)
    {
        uint8_t event_code = UI_EVENT_AUDIO_UP;
        xQueueSend(ui_send_event, &event_code, 0);
        // _ui_screen_change(&ui_face_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_face_screen_init);
    }
    if (event_code == LV_EVENT_LEFT_CLICK)
    {
        uint8_t event_code = UI_EVENT_AUDIO_DOWN;
        xQueueSend(ui_send_event, &event_code, 0);
        // _ui_screen_change(&ui_weather_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_weather_screen_init);
    }
    // if (event_code == LV_EVENT_UP_CLICK)
    // {
    //     uint8_t event_code = UI_EVENT_XIAOZHI_UP;
    //     xQueueSend(ui_send_event, &event_code, 0);
    // }
    if (event_code == LV_EVENT_DOWN_CLICK)
    {
        uint8_t event_code = UI_EVENT_XIAOZHI_DOWN;
        xQueueSend(ui_send_event, &event_code, 0);
    }

    lvgl_port_unlock();
}
void muyushow_Animation(lv_obj_t *TargetObject, int delay);
void blink_Animation(lv_obj_t *TargetObject, int delay);
void falldown_Animation(lv_obj_t *TargetObject, int delay);
void weather_Animation(lv_obj_t *TargetObject, int delay);
void riseup_Animation(lv_obj_t *TargetObject, int delay);
void opaon_Animation(lv_obj_t *TargetObject, int delay);
void gonde_txt_Animation(lv_obj_t *TargetObject, int delay);
void muyushow_Animation(lv_obj_t *TargetObject, int delay)
{
    ui_anim_user_data_t *PropertyAnimation_0_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 300);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, _ui_anim_callback_set_image_zoom);
    lv_anim_set_values(&PropertyAnimation_0, 150, 200);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_overshoot);
    lv_anim_set_delay(&PropertyAnimation_0, delay);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, _ui_anim_callback_free_user_data);
    lv_anim_start(&PropertyAnimation_0);
}
void gonde_txt_Animation(lv_obj_t *TargetObject, int delay)
{
    ui_anim_user_data_t *opacity_anim_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    opacity_anim_user_data->target = TargetObject;

    lv_anim_t opacity_anim;
    lv_anim_init(&opacity_anim);
    lv_anim_set_time(&opacity_anim, 600);
    lv_anim_set_user_data(&opacity_anim, opacity_anim_user_data);
    lv_anim_set_custom_exec_cb(&opacity_anim, _ui_anim_callback_set_text_opacity);
    lv_anim_set_values(&opacity_anim, 255, 0);
    lv_anim_set_path_cb(&opacity_anim, lv_anim_path_linear);
    lv_anim_set_delay(&opacity_anim, delay);
    lv_anim_set_deleted_cb(&opacity_anim, _ui_anim_callback_free_user_data);
    lv_anim_start(&opacity_anim);

    ui_anim_user_data_t *position_anim_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    position_anim_user_data->target = TargetObject;

    lv_anim_t position_anim;
    lv_anim_init(&position_anim);
    lv_anim_set_time(&position_anim, 600);
    lv_anim_set_user_data(&position_anim, position_anim_user_data);
    lv_anim_set_custom_exec_cb(&position_anim, _ui_anim_callback_set_y);
    lv_anim_set_values(&position_anim, -70, -90);
    lv_anim_set_path_cb(&position_anim, lv_anim_path_linear);
    lv_anim_set_delay(&position_anim, delay);
    lv_anim_set_deleted_cb(&position_anim, _ui_anim_callback_free_user_data);
    lv_anim_start(&position_anim);
}

void blink_Animation(lv_obj_t *TargetObject, int delay)
{
    ui_anim_user_data_t *PropertyAnimation_0_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    PropertyAnimation_0_user_data->val = -1;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 1000);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, _ui_anim_callback_set_opacity);
    lv_anim_set_values(&PropertyAnimation_0, 255, 60);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_ease_in);
    lv_anim_set_delay(&PropertyAnimation_0, delay);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, _ui_anim_callback_free_user_data);
    lv_anim_set_playback_time(&PropertyAnimation_0, 0);
    lv_anim_set_playback_delay(&PropertyAnimation_0, 0);
    lv_anim_set_repeat_count(&PropertyAnimation_0, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&PropertyAnimation_0, 0);
    lv_anim_set_early_apply(&PropertyAnimation_0, false);
    lv_anim_set_get_value_cb(&PropertyAnimation_0, &_ui_anim_callback_get_opacity);
    lv_anim_start(&PropertyAnimation_0);
}
void falldown_Animation(lv_obj_t *TargetObject, int delay)
{
    ui_anim_user_data_t *PropertyAnimation_0_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 500);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, _ui_anim_callback_set_y);
    lv_anim_set_values(&PropertyAnimation_0, -50, 0);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_overshoot);
    lv_anim_set_delay(&PropertyAnimation_0, delay);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, _ui_anim_callback_free_user_data);
    lv_anim_start(&PropertyAnimation_0);
}
void weather_Animation(lv_obj_t *TargetObject, int delay)
{
    ui_anim_user_data_t *PropertyAnimation_0_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 500);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, _ui_anim_callback_set_x);
    lv_anim_set_values(&PropertyAnimation_0, -200, -60);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_overshoot);
    lv_anim_set_delay(&PropertyAnimation_0, delay);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, _ui_anim_callback_free_user_data);
    lv_anim_start(&PropertyAnimation_0);
}
void riseup_Animation(lv_obj_t *TargetObject, int delay)
{
    ui_anim_user_data_t *PropertyAnimation_0_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 500);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, _ui_anim_callback_set_y);
    lv_anim_set_values(&PropertyAnimation_0, 100, 0);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_overshoot);
    lv_anim_set_delay(&PropertyAnimation_0, delay);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, _ui_anim_callback_free_user_data);
    lv_anim_start(&PropertyAnimation_0);
}
void opaon_Animation(lv_obj_t *TargetObject, int delay)
{
    ui_anim_user_data_t *PropertyAnimation_0_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 500);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, _ui_anim_callback_set_text_opacity);
    lv_anim_set_values(&PropertyAnimation_0, 0, 255);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_linear);
    lv_anim_set_delay(&PropertyAnimation_0, delay);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, _ui_anim_callback_free_user_data);
    lv_anim_start(&PropertyAnimation_0);
}
void ui_Initialize(void)
{
    lvgl_port_lock(0);

    ui_xiaozhi = lv_disp_get_scr_act(NULL);
    lv_obj_add_event_cb(ui_xiaozhi, ui_event_xiaozhi, LV_EVENT_ALL, NULL);

    ui_sys_event = xQueueCreate(4, sizeof(lv_sys_event_t));
    ESP_ERROR_CHECK_WITHOUT_ABORT((ui_sys_event) ? ESP_OK : ESP_FAIL);

    lv_timer_create(ui_system_update, 10, NULL);

    ui_title_screen_init();
    ui_weather_screen_init();
    lv_obj_set_parent(ui_title_page, ui_xiaozhi);
    ui_init_timer();

    lvgl_port_unlock();
    // ui_face_screen_init();
    // lv_screen_load(ui_face_screen);
}
void ui_send_sys_event(lv_obj_t *obj, lv_event_code_t event_code, void *user_data)
{
    lvgl_port_lock(0);

    lv_sys_event_t eventOut;

    eventOut.obj = obj;
    eventOut.event_code = event_code;
    eventOut.user_data = user_data;
    xQueueSend(ui_sys_event, &eventOut, 0);

    lvgl_port_unlock();
    return;
}
void set_wifi_ip_text(const char *text)
{
    lvgl_port_lock(0);

    lv_label_set_text(wifi_ip_text, text);
    lv_label_set_text(wifi_ip_label, text);
    lvgl_port_unlock();
}