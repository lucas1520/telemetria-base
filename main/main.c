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

#include "esp_vfs_fat.h"
#include "driver/spi_master.h"
#include "sdmmc_cmd.h"

#define MOUNT_POINT "/telemetryData"

#define PIN_NUM_MOSI 23
#define PIN_NUM_MISO 19
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5

static void nimble_host_config_init(void);
static void init_ble(void);
static void init_sd_card(void);

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

    init_ble();
    init_sd_card();

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

static void init_ble(void) {
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
}

static void init_sd_card(void) {
    esp_err_t ret;

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE("SD", "Failed to initialize SPI bus");
        return;
    }

    // Configure the SD card device
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    // Mount the SD card
    sdmmc_card_t *card;
    ret = esp_vfs_fat_sdspi_mount(
        MOUNT_POINT,
        &host,
        &slot_config,
        &mount_config,
        &card
    );

    if (ret != ESP_OK) {
        ESP_LOGE("SD", "Failed to mount filesystem: %s",
                 esp_err_to_name(ret));
        return;
    }

    ESP_LOGI("SD", "SD card mounted!");

    // Write a file
    FILE *f = fopen(MOUNT_POINT "/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE("SD", "Failed to open file for writing");
    } else {
        fprintf(f, "Hello from ESP-IDF!\n");
        fclose(f);
        ESP_LOGI("SD", "File written successfully");
    }

    // Unmount when you're completely finished with the card
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
}

static void on_stack_sync(void) {
    adv_init();
}

// GATT server register callback function
static void nimble_host_config_init(void) {
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.sync_cb = on_stack_sync;
}
