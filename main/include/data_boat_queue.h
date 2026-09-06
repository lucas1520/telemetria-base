#ifndef TELEMETRIA_BASE_DATA_BOAT_QUEUE_H
#define TELEMETRIA_BASE_DATA_BOAT_QUEUE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t data_boat_queue;
extern QueueHandle_t data_telemetry_queue;

#endif //TELEMETRIA_BASE_DATA_BOAT_QUEUE_H
