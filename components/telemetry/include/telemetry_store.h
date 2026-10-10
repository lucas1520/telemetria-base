#ifndef TELEMETRY_STORE_H
#define TELEMETRY_STORE_H

#include <stdbool.h>
#include <stddef.h>

#include "types.h"

#define LINK_TIMEOUT_MS 3000

// Cria o mutex. Chamar uma vez, antes de criar as tasks. Retorna false em falha.
bool telemetry_store_init(void);

void telemetry_store_update_battery(float voltage, float current);
void telemetry_store_update_heartbeat(void);

// Pedido para zerar a energia usada (bateria recem-carregada), feito pela pagina Wi-Fi.
// A task de calculo consome o pedido na proxima amostra.
void telemetry_store_request_energy_reset(void);
bool telemetry_store_take_energy_reset(void);
void telemetry_store_set_energy(double used_energy, float time_remaining_min, float energy_remaining);

// Copia atomica do estado atual; link_ok e calculado no momento da leitura.
void telemetry_store_get(struct boat_state *out);

// Monta o payload BLE "corrente,tensao,energia_usada,autonomia_min,energia_restante"
// a partir do estado atual. Retorna o mesmo que snprintf.
int telemetry_store_format(char *buf, size_t len);

#endif
