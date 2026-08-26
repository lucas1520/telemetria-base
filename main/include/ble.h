#ifndef BLE_H
#define BLE_H

#include "freertos/FreeRTOS.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"

#include "esp_log.h"

#include "gatt_svc.h"
#include "telemetry.h"
#include "gap.h"

void init_ble(void);
void telemetry_task(void *params);

#endif