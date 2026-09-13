#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define TELEMETRY_TASK_PERIOD (1000 / portTICK_PERIOD_MS)

struct telemetry_calc_queues {
    QueueHandle_t data_boat_queue;
    QueueHandle_t data_telemetry_queue;
};

uint8_t get_telemetry(void);
void update_telemetry(void);
void telemetry_task_calc(void *pvParameters);
char* get_telemetry_packet(void);


#endif
