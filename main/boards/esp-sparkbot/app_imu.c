
/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include "app_imu.h"
#include "bmi270.h"
#include "./ui/ui.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#include "./tinyGL_cube/tinyGL_cube.h"

static const char *TAG = "imu";

#define GRAVITY_EARTH (9.80665f)
#define ACCEL UINT8_C(0x00)
#define GYRO UINT8_C(0x01)

#define FILTER_ALPHA 0.5f
#define TRIGGER_THRESHOLD 4.5f // deg
static bool gesright_sentalready = false;
static bool gesleft_sentalready = false;
static bool gesup_sentalready = false;
static bool gesdown_sentalready = false;
static bmi270_handle_t bmi_handle = NULL;
static bmi270_axis_t axis_last_val = {0.0f, 0.0f, 0.0f};
static esp_err_t bmi_init_ret;
static i2c_bus_handle_t i2c_bus_handle = NULL;

#include <math.h>
#define sampleFreq 25.0f       // sample frequency in Hz
#define twoKpDef (2.0f * 1.0f) // 2 * proportional gain
#define twoKiDef (2.0f * 0.0f) // 2 * integral gain
#define RAD_TO_DEG (180.0f / 3.14159265358979323846f)
#define DEG_TO_RAD (3.14159265358979323846f / 180.0f)

volatile float twoKp = twoKpDef; // 2 * proportional gain (Kp)
volatile float twoKi = twoKiDef; // 2 * integral gain (Ki)
volatile float
    q0 = 1.0,
    q1 = 0.0, q2 = 0.0,
    q3 = 0.0; // quaternion of sensor frame relative to auxiliary frame
volatile float integralFBx = 0.0f, integralFBy = 0.0f,
               integralFBz = 0.0f; // integral error terms scaled by Ki

static float invSqrt(float x)
{
    float halfx = 0.5f * x;
    float y = x;
    long i = *(long *)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float *)&i;
    y = y * (1.5f - (halfx * y * y));
    return y;
}
void MahonyAHRSupdateIMU(float gx, float gy, float gz, float ax, float ay,
                         float az, bmi270_axis_t *axis_val)
{
    float recipNorm;
    float halfvx, halfvy, halfvz;
    float halfex, halfey, halfez;
    float qa, qb, qc;

    gx *= DEG_TO_RAD;
    gy *= DEG_TO_RAD;
    gz *= DEG_TO_RAD;
    // Compute feedback only if accelerometer measurement valid (avoids NaN in
    // accelerometer normalisation)
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f)))
    {
        // Normalise accelerometer measurement
        recipNorm = invSqrt(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Estimated direction of gravity and vector perpendicular to magnetic
        // flux
        halfvx = q1 * q3 - q0 * q2;
        halfvy = q0 * q1 + q2 * q3;
        halfvz = q0 * q0 - 0.5f + q3 * q3;

        // Error is sum of cross product between estimated and measured
        // direction of gravity
        halfex = (ay * halfvz - az * halfvy);
        halfey = (az * halfvx - ax * halfvz);
        halfez = (ax * halfvy - ay * halfvx);

        // Compute and apply integral feedback if enabled
        if (twoKi > 0.0f)
        {
            integralFBx += twoKi * halfex *
                           (1.0f / sampleFreq); // integral error scaled by Ki
            integralFBy += twoKi * halfey * (1.0f / sampleFreq);
            integralFBz += twoKi * halfez * (1.0f / sampleFreq);
            gx += integralFBx; // apply integral feedback
            gy += integralFBy;
            gz += integralFBz;
        }
        else
        {
            integralFBx = 0.0f; // prevent integral windup
            integralFBy = 0.0f;
            integralFBz = 0.0f;
        }

        // Apply proportional feedback
        gx += twoKp * halfex;
        gy += twoKp * halfey;
        gz += twoKp * halfez;
    }

    // Integrate rate of change of quaternion
    gx *= (0.5f * (1.0f / sampleFreq)); // pre-multiply common factors
    gy *= (0.5f * (1.0f / sampleFreq));
    gz *= (0.5f * (1.0f / sampleFreq));
    qa = q0;
    qb = q1;
    qc = q2;
    q0 += (-qb * gx - qc * gy - q3 * gz);
    q1 += (qa * gx + qc * gz - q3 * gy);
    q2 += (qa * gy - qb * gz + q3 * gx);
    q3 += (qa * gz + qb * gy - qc * gx);

    // Normalise quaternion
    recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;

    axis_val->pitch = asin(-2 * q1 * q3 + 2 * q0 * q2); // pitch
    axis_val->roll = atan2(2 * q2 * q3 + 2 * q0 * q1,
                           -2 * q1 * q1 - 2 * q2 * q2 + 1); // roll
    axis_val->yaw = atan2(2 * (q1 * q2 + q0 * q3),
                          q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3); // yaw

    axis_val->pitch *= RAD_TO_DEG;
    axis_val->yaw *= RAD_TO_DEG;
    // Declination of SparkFun Electronics (40°05'26.6"N 105°11'05.9"W) is
    //  8° 30' E  ± 0° 21' (or 8.5°) on 2016-07-19
    // - http://www.ngdc.noaa.gov/geomag-web/#declination
    axis_val->yaw -= 8.5;
    axis_val->roll *= RAD_TO_DEG;
}

typedef struct
{
    float angle; // The calculated angle
    float bias;  // The gyro bias
    float rate;  // The rate from gyro

    float P[2][2];   // Error covariance matrix
    float Q_angle;   // Process noise variance for angle
    float Q_bias;    // Process noise variance for bias
    float R_measure; // Measurement noise variance
} KalmanFilter;

// Initialize Kalman Filter
static void KalmanFilter_Init(KalmanFilter *kf)
{
    kf->angle = 0.0f;
    kf->bias = 0.0f;
    kf->rate = 0.0f;

    kf->P[0][0] = 0.0f;
    kf->P[0][1] = 0.0f;
    kf->P[1][0] = 0.0f;
    kf->P[1][1] = 0.0f;

    kf->Q_angle = 0.005f;
    kf->Q_bias = 0.003f;
    kf->R_measure = 0.03f;
}

// Update Kalman Filter
static float KalmanFilter_Update(KalmanFilter *kf, float newAngle, float newRate, float dt)
{
    // Predict step
    kf->rate = newRate - kf->bias;
    kf->angle += dt * kf->rate;

    kf->P[0][0] += dt * (dt * kf->P[1][1] - kf->P[0][1] - kf->P[1][0] + kf->Q_angle);
    kf->P[0][1] -= dt * kf->P[1][1];
    kf->P[1][0] -= dt * kf->P[1][1];
    kf->P[1][1] += kf->Q_bias * dt;

    // Measurement update
    float S = kf->P[0][0] + kf->R_measure;
    float K[2]; // Kalman gain
    K[0] = kf->P[0][0] / S;
    K[1] = kf->P[1][0] / S;

    float y = newAngle - kf->angle; // Angle difference
    kf->angle += K[0] * y;
    kf->bias += K[1] * y;

    // Update error covariance matrix
    float P00_temp = kf->P[0][0];
    float P01_temp = kf->P[0][1];

    kf->P[0][0] -= K[0] * P00_temp;
    kf->P[0][1] -= K[0] * P01_temp;
    kf->P[1][0] -= K[1] * P00_temp;
    kf->P[1][1] -= K[1] * P01_temp;

    return kf->angle;
}

// Calculate Roll, Pitch, and Yaw
void calculateAttitude(float gyroX, float gyroY, float gyroZ, float accX, float accY, float accZ, float dt, bmi270_axis_t *axis_val)
{
    static KalmanFilter kfRoll, kfPitch;
    static int initialized = 0;

    if (!initialized)
    {
        KalmanFilter_Init(&kfRoll);
        KalmanFilter_Init(&kfPitch);
        initialized = 1;
    }

    // Calculate roll and pitch angles from accelerometer
    float accRoll = atan2f(accY, accZ) * 180.0f / M_PI;
    float accPitch = atan2f(-accX, sqrtf(accY * accY + accZ * accZ)) * 180.0f / M_PI;

    // Update Kalman filters for roll and pitch
    axis_val->roll = KalmanFilter_Update(&kfRoll, accRoll, gyroX, dt);
    axis_val->pitch = KalmanFilter_Update(&kfPitch, accPitch, gyroY, dt);

    // For yaw, integrate gyroZ directly (no accelerometer correction)
    static float yawAngle = 0.0f;
    yawAngle += gyroZ * dt;
    axis_val->yaw = yawAngle;
}
void bsp_i2c_init(void)
{
    /* I2C was initialized before */

    const i2c_config_t i2c_bus_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_bus_handle = i2c_bus_create((i2c_port_t)0, &i2c_bus_conf);

    return;
}
i2c_master_bus_handle_t i2c_bus_get_master_bus_handle()
{
    return i2c_bus_get_internal_bus_handle(i2c_bus_handle);
}
static void i2c_sensor_bmi270_init(void)
{
    bsp_i2c_init();
    // i2c_bus_handle_t i2c_bus_handle = bsp_i2c_get_handle();
    // if(!i2c_bus_handle){
    //     ESP_LOGE(TAG, "Failed to get i2c bus handle");
    //     return;
    // }
    //     i2c_master_bus_handle_t bus_handle;

    //bmi270_i2c_config_t i2c_bmi270_conf = {
     //   .i2c_handle = i2c_bus_handle,
     //   .i2c_addr = BMI270_I2C_ADDRESS,
    //};

    //bmi_init_ret = bmi270_sensor_create(&i2c_bmi270_conf, &bmi_handle);
}
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width)
{
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (GRAVITY_EARTH * val * g_range) / half_scale;
}

static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width)
{
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (dps / (half_scale)) * (val);
}

bmi270_axis_t applyDiceInertia(float dice_x_set, float dice_y_set, float dice_z_set)
{
    float left;

    static float stop_x = 0;
    static float stop_y = 0;
    static float stop_z = 0;

    static float dice_x_rotation = 0.0f;
    static float dice_y_rotation = 0.0f;
    static float dice_z_rotation = 0.0f;

    static float dice_dst_x = 0.0f;
    static float dice_dst_y = 0.0f;
    static float dice_dst_z = 0.0f;

    dice_x_rotation += dice_x_set;
    dice_y_rotation += dice_y_set;
    dice_z_rotation += dice_z_set;

    // Check if the velocities are close to zero, and stop the rotation
    if (fabs(dice_x_set - stop_x) < 2.5f && fabs(dice_y_set - stop_y) < 2.5f && fabs(dice_z_set - stop_z) < 2.5f)
    {
        if (fabs(dice_x_set - stop_x) < 2.5f)
        {
            if (dice_x_set)
            {
                left = fmod(dice_x_rotation, 90.0f);
                if (dice_x_rotation > 0)
                {
                    dice_dst_x = fabs(left) < 45 ? dice_x_rotation - left : dice_x_rotation + (90 - fabs(left));
                }
                else
                {
                    dice_dst_x = fabs(left) < 45 ? dice_x_rotation + fabs(left) : dice_x_rotation - (90 - fabs(left));
                }
            }
        }

        if (fabs(dice_y_set - stop_y) < 2.5f)
        {
            if (dice_y_set)
            {
                left = fmod(dice_y_rotation, 90.0f);
                if (dice_y_rotation > 0)
                {
                    dice_dst_y = fabs(left) < 45 ? dice_y_rotation - left : dice_y_rotation + (90 - fabs(left));
                }
                else
                {
                    dice_dst_y = fabs(left) < 45 ? dice_y_rotation + fabs(left) : dice_y_rotation - (90 - fabs(left));
                }
            }
        }

        if (fabs(dice_z_set - stop_z) < 2.5f)
        {
            if (dice_z_set)
            {
                left = fmod(dice_z_rotation, 90.0f);
                if (dice_z_rotation > 0)
                {
                    dice_dst_z = fabs(left) < 45 ? dice_z_rotation - left : dice_z_rotation + (90 - fabs(left));
                }
                else
                {
                    dice_dst_z = fabs(left) < 45 ? dice_z_rotation + fabs(left) : dice_z_rotation - (90 - fabs(left));
                }
            }
        }

        dice_x_set = 0.0f;
        dice_y_set = 0.0f;
        dice_z_set = 0.0f;
        stop_x = 0.0f;
        stop_y = 0.0f;
        stop_z = 0.0f;
    }
    else
    {
        stop_x = dice_x_set;
        stop_y = dice_y_set;
        stop_z = dice_z_set;
    }

    if (dice_dst_x)
    {
        if (fabs(dice_dst_x - dice_x_rotation) < 0.1f)
        {
            dice_x_rotation = dice_dst_x;
            dice_dst_x = 0;
        }
        else
        {
            dice_x_rotation += (dice_dst_x > dice_x_rotation ? 1 : -1) * fmin(8.05f, fabs(dice_dst_x - dice_x_rotation) * 0.2f);
        }
    }

    if (dice_dst_y)
    {
        if (fabs(dice_dst_y - dice_y_rotation) < 0.1f)
        {
            dice_y_rotation = dice_dst_y;
            dice_dst_y = 0;
        }
        else
        {
            dice_y_rotation += (dice_dst_y > dice_y_rotation ? 1 : -1) * fmin(8.05f, fabs(dice_dst_y - dice_y_rotation) * 0.2f);
        }
    }

    if (dice_dst_z)
    {
        if (fabs(dice_dst_z - dice_z_rotation) < 0.1f)
        {
            dice_z_rotation = dice_dst_z;
            dice_dst_z = 0;
        }
        else
        {
            dice_z_rotation += (dice_dst_z > dice_z_rotation ? 1 : -1) * fmin(8.05f, fabs(dice_dst_z - dice_z_rotation) * 0.2f);
        }
    }

    bmi270_axis_t dice_axis;
    dice_axis.pitch = dice_x_rotation;
    dice_axis.roll = dice_y_rotation;
    dice_axis.yaw = dice_z_rotation;

    return dice_axis;
}

static int8_t set_accel_gyro_config(struct bmi2_dev *bmi)
{

    int8_t rslt;
    struct bmi2_sens_config config[2];

    config[ACCEL].type = BMI2_ACCEL;
    config[GYRO].type = BMI2_GYRO;

    rslt = bmi2_get_sensor_config(config, 2, bmi);
    //bmi2_error_codes_print_result(rslt);

    rslt = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, bmi);
    //bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK)
    {

        config[ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;
        config[ACCEL].cfg.acc.range = BMI2_ACC_RANGE_2G;
        config[ACCEL].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
        config[ACCEL].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
        config[GYRO].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;
        config[GYRO].cfg.gyr.range = BMI2_GYR_RANGE_2000;
        config[GYRO].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;
        config[GYRO].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;
        config[GYRO].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

        rslt = bmi2_set_sensor_config(config, 2, bmi);
        //bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}

static void bmi270_enable_accel_gyro(struct bmi2_dev *bmi2_dev)
{
    int8_t rslt;

    /* Assign accel and gyro sensor to variable. */
    uint8_t sensor_list[2] = {BMI2_ACCEL, BMI2_GYRO};

    struct bmi2_sens_config config;
    /* Accel and gyro configuration settings. */
    rslt = set_accel_gyro_config(bmi2_dev);
    //bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK)
    {
        rslt = bmi2_sensor_enable(sensor_list, 2, bmi2_dev);
        //bmi2_error_codes_print_result(rslt);

        if (rslt == BMI2_OK)
        {
            config.type = BMI2_ACCEL;
            rslt = bmi2_get_sensor_config(&config, 1, bmi2_dev);
            //bmi2_error_codes_print_result(rslt);
        }
    }
}

static void bmi270_get_accel_gyro_value(struct bmi2_dev *bmi2_dev, bmi270_value_t *accel_gyro_val)
{
    int8_t rslt;
    struct bmi2_sens_data sensor_data;
    rslt = bmi2_get_sensor_data(&sensor_data, bmi2_dev);
    //bmi2_error_codes_print_result(rslt);

    if ((rslt == BMI2_OK) && (sensor_data.status & BMI2_DRDY_ACC) &&
        (sensor_data.status & BMI2_DRDY_GYR))
    {
        /* Converting lsb to meter per second squared for 16 bit accelerometer at 2G range. */
        accel_gyro_val->acc_x = lsb_to_mps2(sensor_data.acc.x, (float)2, bmi2_dev->resolution);
        accel_gyro_val->acc_y = lsb_to_mps2(sensor_data.acc.y, (float)2, bmi2_dev->resolution);
        accel_gyro_val->acc_z = lsb_to_mps2(sensor_data.acc.z, (float)2, bmi2_dev->resolution);

        /* Converting lsb to degree per second for 16 bit gyro at 2000dps range. */
        accel_gyro_val->gyr_x = lsb_to_dps(sensor_data.gyr.x, (float)2000, bmi2_dev->resolution);
        accel_gyro_val->gyr_y = lsb_to_dps(sensor_data.gyr.y, (float)2000, bmi2_dev->resolution);
        accel_gyro_val->gyr_z = lsb_to_dps(sensor_data.gyr.z, (float)2000, bmi2_dev->resolution);
    }
}

static bmi270_axis_t app_imu_read(void)
{
    bmi270_value_t accel_gyro_val;
    bmi270_axis_t axis_val = {0.0f, 0.0f, 0.0f};
    bmi270_axis_t axis_offset = {0.0f, 0.0f, 0.0f};

    bmi270_get_accel_gyro_value(bmi_handle, &accel_gyro_val);
    calculateAttitude(accel_gyro_val.gyr_x, accel_gyro_val.gyr_y, accel_gyro_val.gyr_z, accel_gyro_val.acc_x, accel_gyro_val.acc_y, accel_gyro_val.acc_z, 0.01f, &axis_val);
    // ESP_LOGI(TAG, "%5.2f, %5.2f, %5.2f", axis_val.pitch, axis_val.yaw, axis_val.roll);

    axis_offset.pitch = axis_val.pitch - axis_last_val.pitch;
    axis_offset.roll = axis_val.roll - axis_last_val.roll;
    axis_offset.yaw = axis_val.yaw - axis_last_val.yaw;

    axis_last_val.pitch = axis_val.pitch;
    axis_last_val.roll = axis_val.roll;
    axis_last_val.yaw = axis_val.yaw;
    return axis_offset;
}
static void app_dice_event(bmi270_axis_t axis_offset)
{
    bmi270_axis_t dice_axis = applyDiceInertia(axis_offset.pitch * 15, axis_offset.roll * 15, axis_offset.yaw * 15);
    // ESP_LOGI(TAG, "pitch,yaw: %5.2f, %5.2f, %5.2f", axis_offset.pitch, axis_offset.yaw, axis_offset.roll);
    lv_event_send(ui_dice_canvas, LV_EVENT_VALUE_CHANGED, &dice_axis);
}
void app_imu_dice_send()
{
    if (bmi_init_ret != ESP_OK)
    {
        return;
    }
    bmi270_axis_t axis_offset = app_imu_read();
    app_dice_event(axis_offset);
}
void app_imu_init(void)
{
    i2c_sensor_bmi270_init();
    //(bmi_handle);
}