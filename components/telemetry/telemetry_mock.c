#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <esp_timer.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "energy_persist.h"
#include "telemetry.h"
#include "telemetry_store.h"
#include "types.h"

static const char *TAG = "ENERGY";

static double calculateEnergySlice(float voltage_instant, float current_instant);
static void publish_telemetry(const struct data_boat *data_boat, double used_energy);

double usedEnergy = 0.0; // Wh consumidos desde a ultima carga completa (persistido na NVS)
uint64_t before = 0; // milliseconds
double previous_power = 0.0; // W


static void publish_telemetry(const struct data_boat *data_boat, double used_energy) {
    // Potencia media usada na autonomia: atualizada a cada AUTONOMY_AVG_SAMPLES amostras
    // e mantida entre as atualizacoes, para a autonomia nao oscilar a cada leitura.
    static double power_sum = 0.0;
    static unsigned power_count = 0;
    static double held_power = NAN;
    static bool power_ready = false;

    double energy_remaining = BATTERY_CAPACITY_WH - used_energy;
    if (energy_remaining < 0.0) {
        energy_remaining = 0.0;
    } else if (energy_remaining > BATTERY_CAPACITY_WH) {
        energy_remaining = BATTERY_CAPACITY_WH;
    }

    double power = (double)data_boat->voltage_instant * data_boat->current_instant;
    if (!isnan(power)) {
        power_sum += power;
        power_count++;
    }

    // A primeira amostra ja publica um valor (nao deixa a tela vazia ate completar a janela).
    if (power_count >= AUTONOMY_AVG_SAMPLES || (!power_ready && power_count > 0)) {
        held_power = power_sum / power_count;
        power_sum = 0.0;
        power_count = 0;
        power_ready = true;
    }

    float time_remaining_min = NAN;
    if (!isnan(held_power) && held_power > MIN_POWER_FOR_AUTONOMY_W) {
        time_remaining_min = (float)(energy_remaining / held_power * 60.0);
    }

    telemetry_store_set_energy(used_energy, time_remaining_min, (float)energy_remaining);
}

void telemetry_task_calc(void *pvParameters) {
    struct telemetry_calc_queues *queues = (struct telemetry_calc_queues *)pvParameters;
    struct data_boat data_boat;

    // Restaura o consumo salvo antes do reboot (bateria nao volta a "cheia" sozinha).
    double restored_wh = 0.0;
    bool have_persisted = energy_persist_load(&restored_wh)
        && isfinite(restored_wh) && restored_wh <= 10.0 * BATTERY_CAPACITY_WH;
    if (have_persisted) {
        usedEnergy = restored_wh;
        ESP_LOGI(TAG, "energia usada restaurada da NVS: %.2f Wh", usedEnergy);
    } else {
        ESP_LOGI(TAG, "sem energia salva na NVS, comecando em 0 Wh");
    }

    bool soc_checked = false;
    double last_saved_wh = usedEnergy;
    int64_t last_save_us = esp_timer_get_time();

    while (1) {
        if (xQueueReceive(queues->data_boat_queue, &data_boat, portMAX_DELAY)) {
            printf("Chegou na queue\n");
            printf("\t%f\n", data_boat.current_instant);
            printf("\t%f\n", data_boat.voltage_instant);

            bool save_now = false;

            // Pedido manual (pagina Wi-Fi): bateria recem-carregada.
            if (telemetry_store_take_energy_reset()) {
                usedEnergy = 0.0;
                soc_checked = true;   // a palavra do operador vence o SoC do barco
                save_now = true;
                ESP_LOGI(TAG, "energia usada zerada (bateria carregada)");
            }

            // Primeira amostra com SoC valido: sem valor salvo, adota o SoC; com valor salvo, so
            // adota se o SoC indicar recarga (bateria bem mais cheia que a salva).
            if (!soc_checked && data_boat.battery_remaining_pct >= 0 && data_boat.battery_remaining_pct <= 100) {
                soc_checked = true;
                double soc_used_wh = BATTERY_CAPACITY_WH * (1.0 - data_boat.battery_remaining_pct / 100.0);
                if (!have_persisted || soc_used_wh < usedEnergy - RECHARGE_DETECT_WH) {
                    ESP_LOGI(TAG, "energia usada ajustada pelo SoC do barco (%d%%): %.2f -> %.2f Wh",
                             data_boat.battery_remaining_pct, usedEnergy, soc_used_wh);
                    usedEnergy = soc_used_wh;
                    save_now = true;
                }
            }

            usedEnergy += calculateEnergySlice(data_boat.voltage_instant, data_boat.current_instant);
            struct data_telemetry data_telemetry = {
                .data_boat = data_boat,
                .usedEnergy = usedEnergy
            };
            publish_telemetry(&data_boat, usedEnergy);
            xQueueSend(queues->data_telemetry_queue, &data_telemetry, portMAX_DELAY);

            int64_t now_us = esp_timer_get_time();
            bool period_elapsed = now_us - last_save_us >= (int64_t)ENERGY_PERSIST_PERIOD_S * 1000000;
            if (save_now || (period_elapsed && fabs(usedEnergy - last_saved_wh) >= ENERGY_PERSIST_MIN_DELTA_WH)) {
                if (energy_persist_save(usedEnergy)) {
                    last_saved_wh = usedEnergy;
                }
                last_save_us = now_us;
            }

            printf("Used energy: %.20f\n", usedEnergy);
        }
    }
}

static double calculateEnergySlice(float voltage_instant, float current_instant) {
    if (isnan(voltage_instant) || isnan(current_instant)) {
        before = 0;   // proxima amostra valida reinicia a integracao
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

    // Link perdido: nao inventa energia interpolando por cima da lacuna.
    if (delta_time > MAX_INTEGRATION_GAP_MS) {
        previous_power = current_power;
        return 0.0;
    }

    double delta_hours = (double)delta_time / 3600000.0;

    printf("Delta: %llu ms\n", delta_time);
    printf("Delta time: %.10f hours\n", delta_hours);

    // Trapezoidal rule
    double energy = 0.5 * (previous_power + current_power) * delta_hours;
    previous_power = current_power;

    return energy;
}