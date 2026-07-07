#include "app_animation.h"
#include "./ui/ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include <time.h>
#include "esp_check.h"
#include "esp_camera.h"
#include "app_power.h"
#include "app_weather.h"
#include "esp_lv_decoder.h"
#include "wifi_config.h"
esp_lv_decoder_handle_t decoder_handle = NULL;
extern lv_obj_t *wifi_ip_bg;
static const char *TAG = "app_animation";

mmap_assets_handle_t asset_lottie;
mmap_assets_handle_t asset_weather;
mmap_assets_handle_t asset_audio;
static void app_mount_mmap_fs()
{
    const mmap_assets_config_t config_lottie = {
        .partition_label = "animation",
        .max_files = MMAP_LOTTIE_ASSETS_FILES,
        .checksum = MMAP_LOTTIE_ASSETS_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    mmap_assets_new(&config_lottie, &asset_lottie);
    ESP_LOGI(TAG, "[%s]stored_files:%d", config_lottie.partition_label, mmap_assets_get_stored_files(asset_lottie));

    const mmap_assets_config_t config_weather = {
        .partition_label = "weather",
        .max_files = MMAP_WEATHER_FILES,
        .checksum = MMAP_WEATHER_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    mmap_assets_new(&config_weather, &asset_weather);
    ESP_LOGI(TAG, "[%s]stored_files:%d", config_weather.partition_label, mmap_assets_get_stored_files(asset_weather));

    const mmap_assets_config_t config = {
        .partition_label = "storage",
        .max_files = MMAP_AUDIO_FILES,
        .checksum = MMAP_AUDIO_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    mmap_assets_new(&config, &asset_audio);
    ESP_LOGI(TAG, "stored_files:%d", mmap_assets_get_stored_files(asset_audio));
}
int find_icon_index(char *data)
{
    int i, j;
    char icon_name[10];
    snprintf(icon_name, sizeof(icon_name), "%s.png", data);
    for (i = 0; i < mmap_assets_get_stored_files(asset_weather); i++)
    {
        if (!strcmp(mmap_assets_get_name(asset_weather, i), icon_name))
        {
            break;
        }
    }
    if (i == mmap_assets_get_stored_files(asset_weather))
    {
        for (j = 0; i < mmap_assets_get_stored_files(asset_weather); j++)
        {
            if (!strcmp(mmap_assets_get_name(asset_weather, i), "999.png"))
            {
                break;
            }
        }
        i = j;
    }
    // ESP_LOGI(TAG, "find icon index:%d", i);
    return i;
}

void updata_weather_icon(void)
{
    static lv_img_dsc_t img_weather_dsc;

    img_weather_dsc.data_size = mmap_assets_get_size(asset_weather, find_icon_index(weather_icon));
    img_weather_dsc.data = mmap_assets_get_mem(asset_weather, find_icon_index(weather_icon));
    lv_img_set_src(ui_weather_weathershow, &img_weather_dsc);
}

struct timeval tv_now = {
    .tv_sec = 0,
    .tv_usec = 0};

void ui_clock_update(lv_timer_t *timer)
{
    lvgl_port_lock(0);
    struct tm timeinfo;
    static char hour_str[3], min_str[3], time_str[6];
    static char date_str[16], weekday_str[16];
    static char power_state_text[5];

    gettimeofday(&tv_now, NULL);
    localtime_r(&tv_now.tv_sec, &timeinfo);

    // format hour,minute,second
    snprintf(hour_str, sizeof(hour_str), "%02d", timeinfo.tm_hour);
    snprintf(min_str, sizeof(min_str), "%02d", timeinfo.tm_min);
    snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    snprintf(power_state_text, sizeof(power_state_text), "%d%%", get_power_value());

    // format month and day(for example:10/18)
    snprintf(date_str, sizeof(date_str), "%02d/%02d",
             timeinfo.tm_mon + 1, timeinfo.tm_mday);

    // get weekday
    const char *weekdays[] = {"周日", "周一", "周二", "周三",
                              "周四", "周五", "周六"};
    snprintf(weekday_str, sizeof(weekday_str), "%s", weekdays[timeinfo.tm_wday]);

    // update text of each label
    lv_label_set_text(ui_weather_hour, hour_str);
    lv_label_set_text(ui_weather_min, min_str);
    lv_label_set_text(ui_weather_date, date_str);
    lv_label_set_text(ui_weather_weekday, weekday_str);
    lv_label_set_text(ui_title_timestate, time_str);

    lv_label_set_text(ui_weather_weather, weather_text);
    lv_label_set_text(ui_weather_temp, weather_temp);
    lv_label_set_text(ui_title_batterytxt, power_state_text);
    lv_slider_set_value(ui_title_powerstate, get_power_value(), LV_ANIM_OFF);
   if (wifi_is_connected() == true)
   {
        //if(wifi_connected_already() == WIFI_STATUS_CONNECTED_OK){
      lv_img_set_src(ui_title_wifistate, &ui_img_wifi_png);
      lv_obj_clear_flag(wifi_ip_bg, LV_OBJ_FLAG_HIDDEN);
   }
   else if(wifi_is_connected() == false)
   {
      lv_img_set_src(ui_title_wifistate, &ui_img_wifi_disconnection_png);
      lv_obj_add_flag(wifi_ip_bg, LV_OBJ_FLAG_HIDDEN);
   }
    updata_weather_icon();

    lvgl_port_unlock();
}

void ui_init_timer()
{
    lv_timer_t *timer_clock = lv_timer_create(ui_clock_update, 1000, NULL);
    ui_clock_update(timer_clock);
}
esp_err_t app_animation_init(void)
{
    esp_err_t ret = ESP_OK;
    app_mount_mmap_fs();
    // ESP_ERROR_CHECK(esp_lv_decoder_init(&decoder_handle));
    lv_png_init();

    /* Initialize the camera */
    const camera_config_t camera_config = BSP_CAMERA_DEFAULT_CONFIG;
    ret = esp_camera_init(&camera_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return ret;
    }
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, BSP_CAMERA_VFLIP);
    s->set_hmirror(s, BSP_CAMERA_HMIRROR);
    ESP_LOGI(TAG, "Camera Init done");
    return ret;
    
}