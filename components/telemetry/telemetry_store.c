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
static bool energy_reset_requested = false;

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

void telemetry_store_request_energy_reset(void) {
    if (store_mutex == NULL) {
        return;
    }

    xSemaphoreTake(store_mutex, portMAX_DELAY);
    energy_reset_requested = true;
    xSemaphoreGive(store_mutex);
}

bool telemetry_store_take_energy_reset(void) {
    if (store_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(store_mutex, portMAX_DELAY);
    bool requested = energy_reset_requested;
    energy_reset_requested = false;
    xSemaphoreGive(store_mutex);

    return requested;
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

// Escreve em buf a partir de pos; devolve quantos caracteres foram (ou seriam) escritos, como snprintf.
static int put(char *buf, size_t len, size_t pos, const char *text) {
    size_t room = pos < len ? len - pos : 0;
    return snprintf(room ? buf + pos : NULL, room, "%s", text);
}

static int put_number(char *buf, size_t len, size_t pos, const char *fmt, double value) {
    size_t room = pos < len ? len - pos : 0;
    return snprintf(room ? buf + pos : NULL, room, fmt, value);
}

int telemetry_store_format(char *buf, size_t len) {
    struct boat_state current;
    telemetry_store_get(&current);

    // Ordem: corrente, tensao, energia_usada, autonomia_min, energia_restante.
    // Valor desconhecido (NaN/inf) vai como campo vazio, ex.: "12.34,,0.00,,1824.00".
    const double values[5] = {
        current.current_instant,
        current.voltage_instant,
        current.used_energy,
        current.time_remaining_min,
        current.energy_remaining
    };
    const char *formats[5] = {"%.2f", "%.2f", "%.2f", "%.0f", "%.2f"};

    size_t pos = 0;
    for (int k = 0; k < 5; k++) {
        if (k > 0) {
            pos += put(buf, len, pos, ",");
        }
        if (isfinite(values[k])) {
            pos += put_number(buf, len, pos, formats[k], values[k]);
        }
    }

    return (int)pos;
}
