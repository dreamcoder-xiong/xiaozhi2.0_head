/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef _APP_IMU_H_
#define _APP_IMU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "i2c_bus.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/i2s.h"
#else
#include "driver/i2s_std.h"
#endif

typedef struct
{
    float acc_x;
    float acc_y;
    float acc_z;
    float gyr_x;
    float gyr_y;
    float gyr_z;
} bmi270_value_t;
void bsp_i2c_init(void);
void app_imu_init(void);
i2c_master_bus_handle_t i2c_bus_get_master_bus_handle();
void app_imu_dice_send();
#ifdef __cplusplus
}
#endif

#endif
