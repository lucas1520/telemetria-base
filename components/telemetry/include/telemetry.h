#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define TELEMETRY_TASK_PERIOD (1000 / portTICK_PERIOD_MS)

#define BATTERY_CAPACITY_WH 1824.0

// Autonomia usa a potencia media de N amostras e so e recalculada a cada N amostras.
#define AUTONOMY_AVG_SAMPLES 8

// Intervalo maximo entre amostras para integrar energia. Acima disso o trecho e
// descartado (link perdido) em vez de interpolado.
#define MAX_INTEGRATION_GAP_MS 10000

// Abaixo dessa potencia media a autonomia e considerada indeterminada.
#define MIN_POWER_FOR_AUTONOMY_W 1.0

// Energia usada e gravada na NVS a cada ENERGY_PERSIST_PERIOD_S segundos, se mudou pelo menos
// ENERGY_PERSIST_MIN_DELTA_WH. Perde no maximo esse periodo de consumo em queda de energia.
#define ENERGY_PERSIST_PERIOD_S 30
#define ENERGY_PERSIST_MIN_DELTA_WH 0.1

// Se o SoC do barco indicar uma bateria mais cheia que a salva por mais que isso, considera
// que ela foi recarregada e adota o SoC.
#define RECHARGE_DETECT_WH (0.10 * BATTERY_CAPACITY_WH)

struct telemetry_calc_queues {
    QueueHandle_t data_boat_queue;
    QueueHandle_t data_telemetry_queue;
};

void telemetry_task_calc(void *pvParameters);


#endif
