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
#include "app_datafusion.h"
typedef struct
{
    float acc_x;
    float acc_y;
    float acc_z;
    float gyr_x;
    float gyr_y;
    float gyr_z;
} bmi270_value_t;

// extern bmi270_axis_t imu_sensor;

void app_imu_init(void);
void app_imu_read(void);

#ifdef __cplusplus
}
#endif

#endif
