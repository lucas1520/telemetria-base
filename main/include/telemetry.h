#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "esp_random.h"

#define TELEMETRY_TASK_PERIOD (1000 / portTICK_PERIOD_MS)

uint8_t get_telemetry(void);
void update_telemetry(void);
void telemetry_task_calc(void *pvParameters);

#endif
