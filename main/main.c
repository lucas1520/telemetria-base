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

static void nimble_host_config_init(void);

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

static void telemetry_task(void *param) {
    ESP_LOGI(TAG, "telemetry task has been started!");

    while (1) {
        update_telemetry();
        ESP_LOGI(TAG, "telemetry task updated to %d", get_telemetry());

        send_telemetry_indication();

            vTaskDelay(TELEMETRY_TASK_PERIOD);
    }
}

void app_main(void) {
    BaseType_t rc = 0;
    esp_err_t ret;

    /*
     * NVS flash initialization
     * Dependency of BLE stack to store configurations
     */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
            ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    /* NimBLE stack initialization */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ", ret);
        return;
    }

    /*GATT Server initialization */
    rc = gatt_svc_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d ", rc);
        nimble_port_deinit();
        return;
    }

    /* NimBLE host configuration initialization */
    nimble_host_config_init();

    /* Start NimBLE host task thread and return */
    nimble_port_freertos_init(nimble_host_task);

    rc = xTaskCreate(telemetry_task, "Telemetry ", 4 * 1024, NULL, 5, NULL);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "failed to create telemetry task");
        return;
    }

    xTaskCreate(read_serial_task, "uart_task", 4096, NULL, 5, NULL);

    char *currentTaskName = pcTaskGetName(NULL);
    while (1) {
        ESP_LOGI(currentTaskName, "Hello");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

static void on_stack_sync(void) {
    adv_init();
}

// GATT server register callback function
static void nimble_host_config_init(void) {
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.sync_cb = on_stack_sync;
}
