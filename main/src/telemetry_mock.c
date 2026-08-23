/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "common.h"
#include "telemetry.h"

/* Private variables */
static uint8_t telemetry;

/* Public functions */
uint8_t get_telemetry(void) { return telemetry; }

void update_telemetry(void) { telemetry = 60 + (uint8_t)(esp_random() % 21); }

