/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "app_weather.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_client.h"

#include "zlib.h"
#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"
#include "./wifi/wifi_config.h"
char weather_temp[5];
char weather_text[16];
char weather_icon[5];
static char west_south[20];

static const char *TAG = "app_weather";
#define MAX_HTTP_RESPONSE_SIZE 2048 // 根据实际需求调整大小

char *response_data;
static int response_data_len = 0;
int location_get = 0;
static int network_gzip_decompress(void *in_buf, size_t in_size, void *out_buf, size_t *out_size, size_t out_buf_size)
{
    int err = 0;
    z_stream d_stream = {0}; /* decompression stream */
    d_stream.zalloc = NULL;
    d_stream.zfree = NULL;
    d_stream.opaque = NULL;
    d_stream.next_in = in_buf;
    d_stream.avail_in = 0;
    d_stream.next_out = out_buf;

    if ((err = inflateInit2(&d_stream, 47)) != Z_OK)
    {
        return err;
    }
    while (d_stream.total_out < out_buf_size - 1 && d_stream.total_in < in_size)
    {
        d_stream.avail_in = d_stream.avail_out = 1;
        if ((err = inflate(&d_stream, Z_NO_FLUSH)) == Z_STREAM_END)
        {
            break;
        }
        if (err != Z_OK)
        {
            return err;
        }
    }

    if ((err = inflateEnd(&d_stream)) != Z_OK)
    {
        return err;
    }

    *out_size = d_stream.total_out;
    ((char *)out_buf)[*out_size] = '\0';

    return Z_OK;
}
static esp_err_t app_weather_parse_now(char *buffer)
{
    cJSON *json = cJSON_Parse(buffer);
    cJSON *json_now = NULL;

    if (NULL != json)
    {
        json_now = cJSON_GetObjectItem(json, "now");
        if (NULL != json_now)
        {
            cJSON *json_item_temp = cJSON_GetObjectItem(json_now, "temp");
            cJSON *json_item_icon = cJSON_GetObjectItem(json_now, "icon");
            cJSON *json_item_text = cJSON_GetObjectItem(json_now, "text");

            snprintf(weather_temp, sizeof(weather_temp), "%s°", json_item_temp->valuestring);
            snprintf(weather_icon, sizeof(weather_icon), "%s", json_item_icon->valuestring);
            snprintf(weather_text, sizeof(weather_text), "%s", json_item_text->valuestring);

            ESP_LOGI(TAG, "Temp : [%s]", json_item_temp->valuestring);
            ESP_LOGI(TAG, "Icon : [%s]", json_item_icon->valuestring);
            ESP_LOGI(TAG, "Text : [%s]", json_item_text->valuestring);
        }
        else
        {
            ESP_LOGE(TAG, "Error parsing object - [%s] - [%d]", __FILE__, __LINE__);
            return ESP_FAIL;
        }
        cJSON_Delete(json);
    }
    else
    {
        ESP_LOGE(TAG, "Error parsing object - [%s] - [%d]", __FILE__, __LINE__);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t app_location_parse_rectangle(char *buffer)
{
    cJSON *json = cJSON_Parse(buffer);
    if (NULL != json)
    {
        cJSON *rectangle = cJSON_GetObjectItem(json, "rectangle");
        if (rectangle != NULL)
        {
            char *rectangle_str = rectangle->valuestring;
            char *semicolon_pos = strchr(rectangle_str, ';');
            if (semicolon_pos != NULL)
            {
                *semicolon_pos = '\0';
            }
            double latitude, longtitude;
            sscanf(rectangle_str, "%lf,%lf", &longtitude, &latitude);
            snprintf(west_south, sizeof(west_south), "%.2f,%.2f", longtitude, latitude);

            from_nvs_set_value("west_south", west_south);

            cJSON_Delete(json);
            return ESP_OK;
        }
        else
        {
            cJSON_Delete(json);
            return ESP_FAIL;
        }
    }
    return ESP_FAIL;
}
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // 打印响应数据
            if (response_data_len + evt->data_len < MAX_HTTP_RESPONSE_SIZE)
            {
                memcpy(response_data + response_data_len, evt->data, evt->data_len);
                response_data_len += evt->data_len;
            }
            else
            {
                ESP_LOGE(TAG, "Response buffer overflow!");
            }
        }
        else
        {
            memcpy(response_data + response_data_len, evt->data, evt->data_len);
            response_data_len += evt->data_len;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        // 确保响应数据以 null 结尾
        response_data[response_data_len] = '\0';
        ESP_LOGI(TAG, "Response data: %s", response_data);
        ESP_LOGI(TAG, "Response data length: %d", response_data_len);
        if (location_get == 0)
        {
            if (app_location_parse_rectangle(response_data) == ESP_OK)
                location_get = 1;
            else
                location_get = 0;
        }
        else if (location_get == 2)
        {
            location_get = 3;
            size_t out_size = 0;
            // only show one city weather
            size_t decode_maxlen = response_data_len * 2;
            char *decode_out = malloc(decode_maxlen);
            if (NULL == decode_out)
            {
                printf("Failed allocate mem\n");
                break;
            }
            network_gzip_decompress(response_data, response_data_len, decode_out, &out_size, decode_maxlen);
            ESP_LOGI(TAG, "gzip data: %s", decode_out);

            // printf("HTTP_EVENT_ON_FINISH, location:%d, out_size:%d, %s\n", location, out_size, decode_out);
            app_weather_parse_now(decode_out);
        }
        response_data_len = 0;
        // process_response_data(response_data);
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    default:
        break;
    }
    return ESP_OK;
}
#define AMAP_KEY "64e91d15ed0d3496532fd5babf2d7d61"  // 需要在高德地图申请自己的API KEY
#define AMAP_URL "https://restapi.amap.com/v3/ip?key=%s"
void amap_get_location()
{
    char *url_request;
    url_request = malloc(1024);
    sprintf(url_request, AMAP_URL, AMAP_KEY);
    // 定义HTTP客户端配置
    esp_http_client_config_t config = {
        .url = url_request,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .timeout_ms = 30000,
        .buffer_size = 1460,
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    // 执行请求
    ESP_LOGI(TAG, "Sending GET request...");
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    // 清理
    esp_http_client_cleanup(client);
    free(url_request);
}
#define WEATHER_API_KEY "496eead4b0914672843447bf0cbb3c6d"  //需要在和风天气申请自己的API KYE
//#define WEATHER_URL "https://api.qweather.com/v7/weather/now?location=%s"
#define WEATHER_URL "https://mq5u9w92tf.re.qweatherapi.com/v7/weather/now?location=%s"
esp_err_t app_weather_request(char *data)
{
    ESP_LOGI(TAG, "location(west longtitude, south latitude) = %s", data);
    char *url_request = malloc(1024);
    sprintf(url_request, WEATHER_URL, data);
    // ESP_LOGI(TAG, "url_request:%s\r\n", url_request);

    esp_http_client_config_t config = {
        .url = url_request,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .timeout_ms = 30000,
        .buffer_size = 1460,
        .skip_cert_common_name_check = true,
    };
    // Set the headers
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "X-QW-Api-Key", WEATHER_API_KEY);

    // Send the request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "HTTP POST request failed: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    if (url_request)
    {
        free(url_request);
    }

    return ESP_OK;
}

void weather_task(void *arg)
{
    uint8_t cont = 0;
    location_get = 0;
    amap_get_location();

    while (1)
    {
        if (location_get)
        {
            app_weather_request(west_south);
            break;
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1000)); // 延时 1 秒
            cont++;
            if (cont == 31)
            {
                ESP_LOGE(TAG, "get location timeout");
                break;
            }
            if (cont % 10 == 0)
            {
                amap_get_location();
            }
        }
    }
    vTaskDelete(NULL);
}
void app_weather_get()
{
    response_data = malloc(MAX_HTTP_RESPONSE_SIZE);
    // location_get = false;
    // xTaskCreate(weather_task, "weather_task", 4096, NULL, 10, NULL);
    uint8_t cont = 0;
    location_get = 0;
    if (wifi_ip_flag == false)
        amap_get_location();
    else
    {
        size_t str1_size = sizeof(west_south);
        from_nvs_get_value("west_south", west_south, &str1_size);

        if (strlen(west_south) == 0)
            amap_get_location();
        else
            location_get = 1;
    }
    while (1)
    {
        if (location_get == 1)
        {
            location_get = 2;

            app_weather_request(west_south);
            // break;
        }
        if (location_get == 2)
        {
            vTaskDelay(pdMS_TO_TICKS(1000)); // 延时 1 秒
        }
        if (location_get == 3)
        {
            break;
        }
        if (location_get == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1000)); // 延时 1 秒
            cont++;
            if (cont == 11)
            {
                ESP_LOGE(TAG, "get location timeout");
                break;
            }
            if (cont % 5 == 0)
            {
                amap_get_location();
            }
        }
    }
    free(response_data);
}