/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

/* Includes */
/* ESP APIs */
#include "esp_random.h"

/* Defines */
#define TELEMETRY_TASK_PERIOD (1000 / portTICK_PERIOD_MS)

/* Public function declarations */
uint8_t get_telemetry(void);
void update_telemetry(void);

#endif

