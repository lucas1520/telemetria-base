#include "esp_err.h"

#include "common.h"

#include "freertos/FreeRTOS.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"

#include "esp_log.h"

#include "gatt_svc.h"
#include "telemetry.h"
#include "gap.h"

static void nimble_host_config_init(void);
static void on_stack_sync(void);
static void nimble_host_task(void *param);

void init_ble(void) {
    BaseType_t rc = 0;
    esp_err_t ret;

    /*
     * NVS flash initialization
     * Dependency of BLE stack to store configurations
     */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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

    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP, error code: %d", rc);
        nimble_port_deinit();
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
}

void telemetry_task(void *param) {
    ESP_LOGI(TAG, "telemetry task has been started!");

    while (1) {
        update_telemetry();
        ESP_LOGI(TAG, "telemetry task updated to %d", get_telemetry());

        send_telemetry_indication();

        vTaskDelay(TELEMETRY_TASK_PERIOD);
    }
}

// GATT server register callback function
static void nimble_host_config_init(void) {
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.sync_cb = on_stack_sync;
}

static void on_stack_sync(void) {
    adv_init();
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}