#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
    // #define WIFI_SSID "Xiaomi_zhangguangwei"
    // #define WIFI_PASSWORD "12345678"
    // #define SET_NVS_WIFI_AUTO_CONNECT "auto_wifi"
    extern bool wifi_ip_flag;
    extern bool wifi_sta_connect_flag;
    esp_err_t from_nvs_set_value(char *key, char *value);
    esp_err_t from_nvs_get_value(char *key, char *value, size_t *size);
    uint8_t nvs_get_u8_data(const char *key_name);
    void nvs_set_u8_data(const char *key_name, uint8_t value);
    esp_err_t api_nvs_set_value(char *key, char *value);
    esp_err_t api_nvs_get_value(char *key, char *value, size_t *size);
    uint16_t api_nvs_get_u16_data(const char *key_name);
    void api_nvs_set_u16_data(const char *key_name, uint16_t value);
    void wifi_init_softap(void);
    void wifi_station_task(void *pvParameters);
    esp_err_t wifi_init_sta();
    esp_err_t wifi_init(uint8_t mode);
    void wifi_stop(void);
    bool wifi_is_connected(void);
#ifdef __cplusplus
}
#endif