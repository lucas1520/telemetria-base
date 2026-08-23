#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "serial.h"

void app_main(void) {
    xTaskCreate(read_serial_task, "uart_task", 4096, NULL, 5, NULL);

    char *currentTaskName = pcTaskGetName(NULL);
    while (1) {
        ESP_LOGI(currentTaskName, "Hello");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}
