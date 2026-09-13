#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "telemetry.h"
#include "types.h"

static double calculateEnergySlice(float voltage_instant, float current_instant);

static uint8_t telemetry;
static struct telemetry_packet telemetry_packet;
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
    uint8_t random = 60 + (uint8_t)(esp_random() % 21);
    uint8_t random1 = 60 + (uint8_t)(esp_random() % 21);
    uint8_t random2 = 60 + (uint8_t)(esp_random() % 21);

    telemetry_packet.current_instant = random;
    telemetry_packet.voltage_instant = random1;
    telemetry_packet.usedEnergy = random2;

    snprintf(
        bufferTeste,
        sizeof(bufferTeste),
        "%.2f,%.2f,%.2f",
        telemetry_packet.current_instant,
        telemetry_packet.voltage_instant,
        telemetry_packet.usedEnergy
    );
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
            xQueueSend(queues->data_telemetry_queue, &data_telemetry, portMAX_DELAY);

            printf("Used energy: %.20f\n", usedEnergy);
        }
    }
}

static double calculateEnergySlice(float voltage_instant, float current_instant) {
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