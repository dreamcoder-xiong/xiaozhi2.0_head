/**
 * 虚拟 MAC（Device-Id）支持
 *
 * 移植自 v1.5.2 定制版的 esp_hi_rebind 组件，只保留 MAC 读取逻辑。
 * 与原版的行为差异：NVS 中不存在 rebind/mac 时不再自动生成随机 MAC，
 * 而是回退芯片真实 MAC，避免升级固件时意外改变设备身份。
 */

#include "esp_hi_rebind.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <esp_log.h>
#include <esp_mac.h>
#include <nvs.h>
#include <nvs_flash.h>

#define TAG "RebindMac"

#define REBIND_NVS_NAMESPACE "rebind"
#define REBIND_NVS_KEY       "mac"

static void rebind_format_mac(const uint8_t *mac, char *out, size_t out_len)
{
    snprintf(out, out_len, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void rebind_get_chip_mac(char *out, size_t out_len)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    rebind_format_mac(mac, out, out_len);
}

static bool rebind_mac_is_valid(const char *mac)
{
    if (mac == NULL || strlen(mac) != 17) {
        return false;
    }
    for (int i = 0; i < 17; i++) {
        if (i == 2 || i == 5 || i == 8 || i == 11 || i == 14) {
            if (mac[i] != ':') {
                return false;
            }
        } else if (isxdigit((unsigned char)mac[i]) == 0) {
            return false;
        }
    }
    return true;
}

const char *esp_hi_rebind_get_mac(void)
{
    static char cached_mac[24];

    if (cached_mac[0] != '\0') {
        return cached_mac;
    }

    char value[24] = {0};
    size_t length = sizeof(value);
    esp_err_t err = ESP_FAIL;
    nvs_handle_t handle;

    if (nvs_open(REBIND_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        err = nvs_get_str(handle, REBIND_NVS_KEY, value, &length);
        nvs_close(handle);
    }

    if (err == ESP_OK && strcmp(value, "chip") == 0) {
        rebind_get_chip_mac(cached_mac, sizeof(cached_mac));
    } else if (err == ESP_OK && rebind_mac_is_valid(value)) {
        snprintf(cached_mac, sizeof(cached_mac), "%s", value);
    } else {
        rebind_get_chip_mac(cached_mac, sizeof(cached_mac));
        ESP_LOGW(TAG, "rebind/mac not found or invalid, fallback to chip MAC");
    }

    char chip[24] = {0};
    rebind_get_chip_mac(chip, sizeof(chip));
    ESP_LOGI(TAG, "Device-Id MAC: %s (chip: %s)", cached_mac, chip);
    return cached_mac;
}
