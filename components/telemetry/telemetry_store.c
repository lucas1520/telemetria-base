#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "telemetry.h"
#include "telemetry_store.h"

static const char *TAG = "STORE";

static SemaphoreHandle_t store_mutex;
static struct boat_state state = {
    .voltage_instant = NAN,
    .current_instant = NAN,
    .used_energy = 0.0,
    .time_remaining_min = NAN,
    .energy_remaining = BATTERY_CAPACITY_WH,
    .link_ok = false
};
static bool heartbeat_seen = false;
static int64_t last_heartbeat_us = 0;

bool telemetry_store_init(void) {
    if (store_mutex != NULL) {
        return true;
    }

    store_mutex = xSemaphoreCreateMutex();
    if (store_mutex == NULL) {
        ESP_LOGE(TAG, "failed to create telemetry store mutex");
        return false;
    }

    return true;
}

void telemetry_store_update_battery(float voltage, float current) {
    if (store_mutex == NULL) {
        return;
    }

    xSemaphoreTake(store_mutex, portMAX_DELAY);
    state.voltage_instant = voltage;
    state.current_instant = current;
    xSemaphoreGive(store_mutex);
}

void telemetry_store_update_heartbeat(void) {
    if (store_mutex == NULL) {
        return;
    }

    xSemaphoreTake(store_mutex, portMAX_DELAY);
    heartbeat_seen = true;
    last_heartbeat_us = esp_timer_get_time();
    xSemaphoreGive(store_mutex);
}

void telemetry_store_set_energy(double used_energy, float time_remaining_min, float energy_remaining) {
    if (store_mutex == NULL) {
        return;
    }

    xSemaphoreTake(store_mutex, portMAX_DELAY);
    state.used_energy = used_energy;
    state.time_remaining_min = time_remaining_min;
    state.energy_remaining = energy_remaining;
    xSemaphoreGive(store_mutex);
}

void telemetry_store_get(struct boat_state *out) {
    if (store_mutex == NULL) {
        *out = state;
        return;
    }

    xSemaphoreTake(store_mutex, portMAX_DELAY);
    *out = state;
    out->link_ok = heartbeat_seen
        && (esp_timer_get_time() - last_heartbeat_us) < (int64_t)LINK_TIMEOUT_MS * 1000;
    xSemaphoreGive(store_mutex);
}

int telemetry_store_format(char *buf, size_t len) {
    struct boat_state current;
    telemetry_store_get(&current);

    return snprintf(
        buf,
        len,
        "%.2f,%.2f,%.2f,%.0f,%.2f",
        current.current_instant,
        current.voltage_instant,
        current.used_energy,
        current.time_remaining_min,
        current.energy_remaining
    );
}
