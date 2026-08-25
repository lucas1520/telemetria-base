#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_vfs_fat.h"
#include "driver/spi_master.h"
#include "sdmmc_cmd.h"

#define PIN_NUM_MOSI 23
#define PIN_NUM_MISO 19
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5

#define MOUNT_POINT "/telemetryData"

void init_sd_card(void);

#endif

