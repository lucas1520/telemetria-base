#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "telemetry.h"
#include "data_boat_queue.h"
#include "types.h"

static double calculateEnergySlice(float voltage_instant, float current_instant);

static uint8_t telemetry;

double usedEnergy = 0.0; // Wh
uint64_t before = 0; // milliseconds

uint8_t get_telemetry(void) {
    return telemetry;
}

void update_telemetry(void) {
    telemetry = 60 + (uint8_t)(esp_random() % 21);
}

void telemetry_task_calc(void *pvParameters) {
    struct data_boat data_boat;
    while (1) {
        if (xQueueReceive(data_boat_queue, &data_boat, portMAX_DELAY)) {
            printf("Chegou na queue\n");
            printf("\t%f\n", data_boat.current_instant);
            printf("\t%f\n", data_boat.voltage_instant);

            usedEnergy += calculateEnergySlice(data_boat.voltage_instant, data_boat.current_instant);
            struct data_telemetry data_telemetry = {
                .data_boat = data_boat,
                .usedEnergy = usedEnergy
            };
            xQueueSend(data_telemetry_queue, &data_telemetry, portMAX_DELAY);

            printf("Used energy: %.20f\n", usedEnergy);
        }
    }
}

static double calculateEnergySlice(float voltage_instant, float current_instant) {
    uint64_t now = esp_timer_get_time() / 1000; // milliseconds

    if (before == 0) {
        before = now;
        return 0.0;
    }

    uint64_t delta_time = now - before;

    before = now;

    double delta_hours = (double)delta_time / 3600000.0;

    printf("Delta: %llu ms\n", delta_time);
    printf("Delta time: %.10f hours\n", delta_hours);

    return voltage_instant * current_instant * delta_hours;
}