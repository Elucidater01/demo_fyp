/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once
#include <esp_err.h>
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_wifi_init(void);
uint8_t app_get_wifi_connected_state();

#ifdef __cplusplus
}
#endif
