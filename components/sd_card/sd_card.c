#include "esp_err.h"
#include"sd_card.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "types.h"

static void dump_buffer();

void init_sd_card(void) {
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

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    sdmmc_card_t *card;
    ret = esp_vfs_fat_sdspi_mount(
        MOUNT_POINT,
        &host,
        &slot_config,
        &mount_config,
        &card
    );

    if (ret != ESP_OK) {
        ESP_LOGE("SD", "Failed to mount filesystem: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI("SD", "SD card mounted!");

    FILE *f = fopen(MOUNT_POINT "/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE("SD", "Failed to open file for writing");
    } else {
        fprintf(f, "Hello from ESRP-IDF!\n");
        fclose(f);
        ESP_LOGI("SD", "File written successfully");
    }

    // esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
}
#define BUFFER_MAX_SIZE 1024

char buffer[BUFFER_MAX_SIZE];
size_t used = 0;

void write_telemetry_data(void *pvParameters) {
    QueueHandle_t data_telemetry_queue = (QueueHandle_t)pvParameters;
    struct data_telemetry data_telemetry;
    char line[128];
    uint8_t count = 0;

    while (1) {
        if (xQueueReceive(data_telemetry_queue, &data_telemetry, portMAX_DELAY)) {
            if (BUFFER_MAX_SIZE - used < sizeof(line)) {
                dump_buffer();
            }

            snprintf(line, sizeof(line), "%hhu, %f,%f,%f",
                count++,
                data_telemetry.data_boat.current_instant,
                data_telemetry.data_boat.voltage_instant,
                data_telemetry.usedEnergy);

            int written = snprintf(buffer + used, sizeof(line), "%s\n", line);
            used += written;
        }
    }
}

static void dump_buffer() {
    FILE *f = fopen(MOUNT_POINT "/data.txt", "a");

    if (f == NULL) {
        ESP_LOGE("SD", "Failed to open file for writing");
        return;
    }

    fprintf(f, "%s", buffer);
    fflush(f);
    fclose(f);

    used = 0;
    ESP_LOGI("SD", "File written successfully");
}
