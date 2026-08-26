#include <stdio.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"

#include "serial.h"
#include "common.h"
#include "gatt_svc.h"
#include "telemetry.h"
#include "gap.h"

#include "sd_card.h"
#include "ble.h"

void app_main(void) {
    BaseType_t rc = 0;

    init_ble();
    init_sd_card();

    rc = xTaskCreate(telemetry_task, "Telemetry ", 4 * 1024, NULL, 5, NULL);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "failed to create telemetry task");
        return;
    }

    xTaskCreate(read_serial_task, "uart_task", 4096, NULL, 5, NULL);
}
