#include <sys/param.h>

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "web_server.h"
#include "wifi_config.h"
#include "../ui/ui.h"
extern lv_obj_t *wifi_ip_label;
bool wifi_ip_flag = false;
static bool g_wifi_sta_inited = false;
static bool g_wifi_ap_inited = false;
bool wifi_sta_connect_flag = 0;
bool g_wifi_sta_ap_state = false;
static char wifi_current_src[100] = "";
// SemaphoreHandle_t ap_sem;
esp_event_handler_instance_t instance_any_id;
esp_event_handler_instance_t instance_got_ip;
static bool wifi_init_flag = false;
static const char *TAG = "wifi_config";
#define PRIFIX_NAME "FZ" // 热点名称+mac

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
esp_netif_t *esp_netif_sta;
esp_netif_t *esp_netif_ap;
// httpd_handle_t server = NULL;

#define EXAMPLE_ESP_WIFI_PASS ""
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_MAX_STA_CONN CONFIG_LWIP_MAX_SOCKETS

#define EXAMPLE_ESP_MAXIMUM_RETRY 5
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif
static void wifi_deinit(); // 关闭wifi热点

static void wifi_ap_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

//---------------------nvs-----------------------//
void nvs_get_str_log(esp_err_t err, char *key, char *value)
{
    switch (err)
    {
    case ESP_OK:
        // ESP_LOGI(TAG, "%s = %s", key, value);
        ESP_LOGI(TAG, "nvs get %s value success!", key);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "%s : Can't find in NVS!", key);
        break;
    default:
        ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
    }
}
esp_err_t api_nvs_set_value(char *key, char *value)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("api", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    }
    else
    {
        err = nvs_set_str(my_handle, key, value);
        ESP_LOGI(TAG, "set %s is %s!,the err is %d\n", key, (err == ESP_OK) ? "succeed" : "failed", err);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "NVS close Done\n");
    }
    return ESP_OK;
}
esp_err_t api_nvs_get_value(char *key, char *value, size_t *size)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("api", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    }
    else
    {
        err = nvs_get_str(my_handle, key, value, size);
        nvs_get_str_log(err, key, value);
        nvs_close(my_handle);
    }
    return err;
}
uint16_t api_nvs_get_u16_data(const char *key_name)
{
    nvs_handle_t my_handle;
    uint16_t value = 0;
    esp_err_t ret;
    ret = nvs_open("main", NVS_READWRITE, &my_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "opening NVS Error (%s)!\n", esp_err_to_name(ret));
    }
    else
    {
        ret = nvs_get_u16(my_handle, key_name, &value);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "%s get Error", key_name);
    }
    nvs_close(my_handle);
    return value;
}
void api_nvs_set_u16_data(const char *key_name, uint16_t value)
{
    nvs_handle_t my_handle;
    esp_err_t ret;
    ret = nvs_open("main", NVS_READWRITE, &my_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "opening NVS Error (%s)!\n", esp_err_to_name(ret));
    }
    else
    {
        ret = nvs_set_u16(my_handle, key_name, value);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "%s set Error", key_name);
    }
    nvs_close(my_handle);
}
esp_err_t from_nvs_set_value(char *key, char *value)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    }
    else
    {
        err = nvs_set_str(my_handle, key, value);
        ESP_LOGI(TAG, "set %s is %s!,the err is %d\n", key, (err == ESP_OK) ? "succeed" : "failed", err);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "NVS close Done\n");
    }
    return ESP_OK;
}
uint8_t nvs_get_u8_data(const char *key_name)
{
    nvs_handle_t my_handle;
    uint8_t value = 0;
    esp_err_t ret;
    ret = nvs_open("main", NVS_READWRITE, &my_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "opening NVS Error (%s)!\n", esp_err_to_name(ret));
    }
    else
    {
        ret = nvs_get_u8(my_handle, key_name, &value);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "%s get Error", key_name);
    }
    nvs_close(my_handle);
    return value;
}
void nvs_set_u8_data(const char *key_name, uint8_t value)
{
    nvs_handle_t my_handle;
    esp_err_t ret;
    ret = nvs_open("main", NVS_READWRITE, &my_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "opening NVS Error (%s)!\n", esp_err_to_name(ret));
    }
    else
    {
        ret = nvs_set_u8(my_handle, key_name, value);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "%s set Error", key_name);
    }
    nvs_close(my_handle);
}
esp_err_t from_nvs_get_value(char *key, char *value, size_t *size)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    }
    else
    {
        err = nvs_get_str(my_handle, key, value, size);
        nvs_get_str_log(err, key, value);
        nvs_close(my_handle);
    }
    return err;
}
/**************读取Nvs中WIFI账号密码***************/
esp_err_t http_get_nvs_wifi_config(wifi_config_t *wifi_config)
{
    char str1[64] = "";
    char str2[64] = "";
    char str3[64] = "";
    size_t str1_size = sizeof(str1);
    size_t str2_size = sizeof(str2);
    size_t str3_size = sizeof(str3);

    esp_err_t err = from_nvs_get_value("ssid", str1, &str1_size);
    if (err == ESP_OK)
    {
        strncpy(&wifi_config->sta.ssid, str1, str1_size);
        // sprintf(start_data.wifi_name, str1);
    }
    err = from_nvs_get_value("password", str2, &str2_size);
    if (err == ESP_OK)
    {
        strncpy(&wifi_config->sta.password, str2, sizeof(wifi_config->sta.password));
    }
    // err = from_nvs_get_value("decive_code", str3, &str3_size);
    // if (err == ESP_OK)
    // {
    //     // strncpy(start_data.qcp, str3, sizeof(start_data.qcp));
    // }

    ESP_LOGI(TAG, "%s\r\n", wifi_config->sta.ssid);
    ESP_LOGI(TAG, "%s\r\n", wifi_config->sta.password);
    // ESP_LOGI(TAG, "%s\r\n", start_data.qcp);
    return ESP_OK;
}

//---------------------wifi_sta-----------------------//
static int s_retry_num = 0;
static char wifi_current_ip[20] = "";
/*UI-WIFI状态判断变量*/
static volatile bool g_wifi_connected = false;
bool wifi_is_connected(void)
{
    return g_wifi_connected;
}
static void sta_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_sta_connect_flag = 0;
        
        g_wifi_connected = false;

        wifi_event_sta_disconnected_t *disconnected = (wifi_event_sta_disconnected_t *)event_data;
        // ESP_LOGE(TAG, "Disconnect reason : %d", disconnected->reason);
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            char src[6] = "......";
            char src_num[6] = "";
            strncpy(src_num, src, s_retry_num);
            sprintf(wifi_current_src, "重连中 %s", src_num);
            set_wifi_ip_text(wifi_current_src);

            ESP_LOGI(TAG, "retry to connect to the AP ,s_retry_num:%d", s_retry_num);
        }
        else
        {
            // xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            // wifi_deinit();
            sprintf(wifi_current_src, "连接WIFI失败");
            set_wifi_ip_text(wifi_current_src);

            ESP_LOGI(TAG, "connect to the AP fail");
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_BEACON_TIMEOUT)
    {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_BEACON_TIMEOUT");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_sta_connect_flag = 1;
        g_wifi_connected = true;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        size_t str1_size = sizeof(wifi_current_ip);

        sprintf(wifi_current_src, "%d.%d.%d.%d", IP2STR(&event->ip_info.ip));

        from_nvs_get_value("ip", wifi_current_ip, &str1_size);
        if (strcmp(wifi_current_ip, wifi_current_src) == 0)
        {
            wifi_ip_flag = true;
        }
        else
        {
            wifi_ip_flag = false;
            from_nvs_set_value("ip", wifi_current_src);
        }
        set_wifi_ip_text(wifi_current_src);
        sprintf(wifi_current_ip, "%d.%d.%d.%d", IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        g_wifi_sta_ap_state = 0;

        // xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
esp_err_t wifi_init_sta()
{
    esp_err_t err = ESP_OK;
    // s_wifi_event_group = xEventGroupCreate();

    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            // .password = "12345678",
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    // wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    // printf("%s\r\n",wifi_config.sta.ssid);
    // printf("%s\r\n",wifi_config.sta.password);
    http_get_nvs_wifi_config(&wifi_config);
    if (strlen((char *)wifi_config.sta.ssid) == 0)
    {
        ESP_LOGI(TAG, "No wifi config found in NVS");
        sprintf(wifi_current_src, " 长按头部配网");
        set_wifi_ip_text(wifi_current_src);

        return ESP_FAIL;
    }

    // 如果wifi已连接
    if (wifi_sta_connect_flag == 0)
    {
        sprintf(wifi_current_src, "WIFI :%s", wifi_config.sta.ssid);
        set_wifi_ip_text(wifi_current_src);
    }
    else
    {
        sprintf(wifi_current_src, wifi_current_ip);
        set_wifi_ip_text(wifi_current_src);

        return err;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_retry_num = 0;

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    err = esp_wifi_connect();
    return err;
}
static void wifi_deinit() // 关闭wifi热点
{
    ESP_LOGI(TAG, "wifi deinit start...");
    if (g_wifi_ap_inited == 0 && g_wifi_sta_inited == 0)
    {
        return;
    }
    if (g_wifi_sta_inited)
    {
        /* The event will not be processed after unregister */
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));

        g_wifi_sta_inited = 0;
        s_retry_num = 0;
    }
    if (g_wifi_ap_inited)
    {
        esp_event_handler_unregister(WIFI_EVENT,
                                     ESP_EVENT_ANY_ID,
                                     &wifi_ap_event_handler);
        g_wifi_ap_inited = 0;
    }
    esp_wifi_stop();

    esp_netif_destroy_default_wifi(esp_netif_sta);
    esp_netif_destroy_default_wifi(esp_netif_ap);

    esp_event_loop_delete_default();
    esp_wifi_deinit();
    esp_netif_deinit();
    // httpd_stop(server);
    wifi_init_flag = false;
    wifi_sta_connect_flag = 0;
    ESP_LOGI(TAG, "wifi deinit end...");
}
void wifi_init_softap(void)
{
    char wifi_ap_name[32];
    uint8_t mac[6];
    char chip_mac[6];

    esp_base_mac_addr_get(mac);
    for (uint8_t i = 0; i < 3; i++)
    {
        sprintf(chip_mac + i * 2, "%02X", mac[i + 3]);
    }
    sprintf(wifi_ap_name, "%s-%s", PRIFIX_NAME, chip_mac);

    wifi_config_t wifi_config = {
        .ap = {
            // .ssid = wifi_ap_name,
            .ssid_len = strlen(wifi_ap_name),
            .password = "",
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    memcpy(wifi_config.ap.ssid, wifi_ap_name, sizeof(wifi_config.ap.ssid));
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); // AP+STA
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    sprintf(wifi_current_src, "%s", ip_addr);
    set_wifi_ip_text(wifi_current_src);

    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             wifi_ap_name, EXAMPLE_ESP_WIFI_PASS);
    g_wifi_sta_ap_state = 1;
}

esp_err_t wifi_init(uint8_t mode)
{
    esp_err_t err = ESP_OK;
    if (wifi_init_flag == false)
    {
        wifi_init_flag = true;
        ESP_ERROR_CHECK(esp_netif_init());

        esp_netif_sta = esp_netif_create_default_wifi_sta();
        esp_netif_ap = esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &sta_event_handler,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &sta_event_handler,
                                                            NULL,
                                                            &instance_got_ip));

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_ap_event_handler, NULL));
        start_webserver();
    }
    if (mode == 1) // STA
    {
        err = wifi_init_sta();
        g_wifi_sta_inited = 1;
    }
    else if (mode == 2) // AP
    {
        g_wifi_ap_inited = 1;
        wifi_init_softap();
    }
    else
    {
        ESP_LOGE(TAG, "wifi mode error");
    }
    return err;
}
void wifi_stop(void)
{
    wifi_deinit();
    // httpd_stop(server);
}