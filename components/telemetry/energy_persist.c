#include <math.h>
#include <stdint.h>
#include "esp_log.h"
#include "nvs.h"

#include "energy_persist.h"

static const char *TAG = "ENERGY_NVS";

#define NVS_NAMESPACE "telemetry"
#define NVS_KEY_USED_MWH "used_mwh"

bool energy_persist_load(double *used_wh) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return false;
    }

    uint32_t used_mwh = 0;
    ret = nvs_get_u32(handle, NVS_KEY_USED_MWH, &used_mwh);
    nvs_close(handle);
    if (ret != ESP_OK) {
        return false;
    }

    *used_wh = used_mwh / 1000.0;
    return true;
}

bool energy_persist_save(double used_wh) {
    if (!isfinite(used_wh) || used_wh < 0.0) {
        used_wh = 0.0;
    }

    double used_mwh = used_wh * 1000.0 + 0.5;
    if (used_mwh > (double)UINT32_MAX) {
        used_mwh = (double)UINT32_MAX;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = nvs_set_u32(handle, NVS_KEY_USED_MWH, (uint32_t)used_mwh);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to save used energy: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}
