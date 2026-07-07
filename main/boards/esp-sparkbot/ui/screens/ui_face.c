#include "../ui.h"
#include "app_ui_bridge.h"
lv_obj_t *ui_face_screen;
lv_obj_t *ui_face_page;
lv_obj_t *ui_face_canvas;
/* 新增表情*/
static lv_obj_t *ui_face_status_label = NULL;

static void ui_event_face(lv_event_t *e)
{

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (event_code == LV_EVENT_SCREEN_LOAD_START)
    {
        const void *data = mmap_assets_get_mem(asset_lottie, MMAP_LOTTIE_ASSETS_LOOK_JSON);
        size_t size = mmap_assets_get_size(asset_lottie, MMAP_LOTTIE_ASSETS_LOOK_JSON);
        lv_lottie_set_src_data(ui_face_canvas, data, size);
    }
    if (event_code == LV_EVENT_LEFT_CLICK)
    {
        //uint8_t event_code = UI_EVENT_XIAOZHI_DOWN;
        //xQueueSend(ui_send_event, &event_code, 0);
        //_ui_screen_change(&ui_weather_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_weather_screen_init);
        xz_ui_state_t st = app_get_xz_ui_state();

         if (st == XZ_UI_STATE_SPEAKING)
         {
             uint8_t msg = UI_EVENT_XIAOZHI_DOWN;   // 打断并进入聆听
             xQueueSend(ui_send_event, &msg, 0);
             return;
         }
           // LISTENING（以及 WAIT）时切屏
            app_force_to_wait();
            _ui_screen_change(&ui_weather_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_weather_screen_init);
            return;
    }
    if (event_code == LV_EVENT_RIGHT_CLICK)
    {
        //uint8_t event_code = UI_EVENT_XIAOZHI_DOWN;
       // xQueueSend(ui_send_event, &event_code, 0);
        //_ui_screen_change(&ui_muyu_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_muyu_screen_init);
        
        xz_ui_state_t st = app_get_xz_ui_state();
        if (st == XZ_UI_STATE_SPEAKING)
        {
               uint8_t msg = UI_EVENT_XIAOZHI_DOWN;
              xQueueSend(ui_send_event, &msg, 0);
             return;
        }
        app_force_to_wait();
        _ui_screen_change(&ui_muyu_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_muyu_screen_init);
            return;
    }
    if (event_code == LV_EVENT_UP_CLICK)
    {
        void *data = mmap_assets_get_mem(asset_lottie, MMAP_LOTTIE_ASSETS_THINK_JSON);
        size_t size = mmap_assets_get_size(asset_lottie, MMAP_LOTTIE_ASSETS_THINK_JSON);
        lv_lottie_set_src_data(ui_face_canvas, data, size);
        lv_lottie_set_segment(ui_face_canvas, 1, 30, true);
    }
    if (event_code == LV_EVENT_DOWN_CLICK)
    {
        void *data = mmap_assets_get_mem(asset_lottie, MMAP_LOTTIE_ASSETS_ASK_JSON);
        size_t size = mmap_assets_get_size(asset_lottie, MMAP_LOTTIE_ASSETS_ASK_JSON);
        lv_lottie_set_src_data(ui_face_canvas, data, size);
        lv_lottie_set_segment(ui_face_canvas, 1, 45, true);
    }


}
void ui_face_screen_init(void)
{
    ui_face_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_face_screen, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_face_page = lv_obj_create(ui_face_screen);
    lv_obj_set_width(ui_face_page, 240);
    lv_obj_set_height(ui_face_page, 240);
    lv_obj_set_align(ui_face_page, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_face_page, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_radius(ui_face_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_face_page, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_face_page, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_face_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_face_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_face_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_face_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_face_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_face_canvas = lv_lottie_create(ui_face_page);
    lv_obj_align(ui_face_canvas, LV_ALIGN_CENTER, 0, 0);

    static uint8_t *fb = NULL;
    ;
    if (fb == NULL)
    {
        fb = malloc(200 * 200 * 4);
        assert(fb);
        lv_lottie_set_buffer(ui_face_canvas, 200, 200, fb);
    }
    /*  左上角状态文本  */
    ui_face_status_label = lv_label_create(ui_face_screen);
    lv_obj_align(ui_face_status_label, LV_ALIGN_TOP_LEFT, 6, 4);
    lv_label_set_text(ui_face_status_label, "WAIT...");
    lv_obj_set_style_text_color(ui_face_status_label,lv_color_white(),0);




    lv_obj_add_event_cb(ui_face_screen, ui_event_face, LV_EVENT_ALL, NULL);
}

void ui_face_set_status_text(const char *text)
{
    if (ui_face_screen == NULL) {
        return;
    }

    /* 只在当前是表情界面时才更新 */
    if (lv_scr_act() != ui_face_screen) {
        return;
    }

    if (ui_face_status_label == NULL) {
        return;
    }

    lv_label_set_text(ui_face_status_label, text);
}
