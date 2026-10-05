#include <math.h>
#include <stdio.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "telemetry.h"
#include "types.h"

static double calculateEnergySlice(float voltage_instant, float current_instant);
static void publish_telemetry(const struct data_boat *data_boat, double used_energy);

static uint8_t telemetry;
static struct telemetry_packet telemetry_packet = {
    .voltage_instant = NAN,
    .current_instant = NAN,
    .usedEnergy = 0.0,
    .time_remaining_min = NAN,
    .energy_remaining = BATTERY_CAPACITY_WH
};
char bufferTeste[64];

double usedEnergy = 0.0; // Wh
uint64_t before = 0; // milliseconds
double previous_power = 0.0; // W

uint8_t get_telemetry(void) {
    return telemetry;
}

char* get_telemetry_packet(void) {
    return bufferTeste;
}

void update_telemetry(void) {
    struct telemetry_packet packet;

    packet = telemetry_packet;

    snprintf(
        bufferTeste,
        sizeof(bufferTeste),
        "%.2f,%.2f,%.2f,%.0f,%.2f",
        packet.current_instant,
        packet.voltage_instant,
        packet.usedEnergy,
        packet.time_remaining_min,
        packet.energy_remaining
    );
}

static void publish_telemetry(const struct data_boat *data_boat, double used_energy) {
    double energy_remaining = BATTERY_CAPACITY_WH - used_energy;
    if (energy_remaining < 0.0) {
        energy_remaining = 0.0;
    }

    float time_remaining_min = NAN;
    double power = (double)data_boat->voltage_instant * data_boat->current_instant;
    if (!isnan(power) && power > 0.0) {
        time_remaining_min = (float)(energy_remaining / power * 60.0);
    }

    telemetry_packet.voltage_instant = data_boat->voltage_instant;
    telemetry_packet.current_instant = data_boat->current_instant;
    telemetry_packet.usedEnergy = used_energy;
    telemetry_packet.time_remaining_min = time_remaining_min;
    telemetry_packet.energy_remaining = (float)energy_remaining;
}

void telemetry_task_calc(void *pvParameters) {
    struct telemetry_calc_queues *queues = (struct telemetry_calc_queues *)pvParameters;
    struct data_boat data_boat;
    while (1) {
        if (xQueueReceive(queues->data_boat_queue, &data_boat, portMAX_DELAY)) {
            printf("Chegou na queue\n");
            printf("\t%f\n", data_boat.current_instant);
            printf("\t%f\n", data_boat.voltage_instant);

            usedEnergy += calculateEnergySlice(data_boat.voltage_instant, data_boat.current_instant);
            struct data_telemetry data_telemetry = {
                .data_boat = data_boat,
                .usedEnergy = usedEnergy
            };
            publish_telemetry(&data_boat, usedEnergy);
            xQueueSend(queues->data_telemetry_queue, &data_telemetry, portMAX_DELAY);

            printf("Used energy: %.20f\n", usedEnergy);
        }
    }
}

static double calculateEnergySlice(float voltage_instant, float current_instant) {
    if (isnan(voltage_instant) || isnan(current_instant)) {
        return 0.0;
    }

    uint64_t now = esp_timer_get_time() / 1000; // milliseconds
    double current_power = (double)voltage_instant * current_instant;

    if (before == 0) {
        before = now;
        previous_power = current_power;
        return 0.0;
    }

    uint64_t delta_time = now - before;

    before = now;

    double delta_hours = (double)delta_time / 3600000.0;

    printf("Delta: %llu ms\n", delta_time);
    printf("Delta time: %.10f hours\n", delta_hours);

    // Trapezoidal rule
    double energy = 0.5 * (previous_power + current_power) * delta_hours;
    previous_power = current_power;

    return energy;
}