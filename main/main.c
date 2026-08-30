#include <stdio.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "serial.h"

#include "common.h"
#include "sd_card.h"
#include "ble.h"
#include "types.h"
#include "data_boat_queue.h"

void app_main(void) {
    BaseType_t rc = 0;
    data_boat_queue = xQueueCreate(10, sizeof(struct data_boat));

    init_ble();
    init_sd_card();

    rc = xTaskCreate(telemetry_task, "Telemetry ", 4 * 1024, NULL, 5, NULL);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "failed to create telemetry task");
        return;
    }

    rc = xTaskCreate(read_serial_task, "uart_task", 4096, NULL, 5, NULL);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "failed to create read serial task");
        return;
    }

    rc = xTaskCreate(telemetry_task_calc, "Telemetry calculator", 4096, NULL, 5, NULL);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "failed to create telemetry task");
        return;
    }
}
