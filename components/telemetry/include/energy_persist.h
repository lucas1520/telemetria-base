#ifndef ENERGY_PERSIST_H
#define ENERGY_PERSIST_H

#include <stdbool.h>

// Energia usada (Wh desde a ultima carga completa) guardada na NVS, para sobreviver a reboot.
// Requer nvs_flash_init() ja executado (feito em init_ble()).

// Retorna false se nao ha valor salvo (primeiro boot, NVS apagada) ou a NVS nao esta disponivel.
bool energy_persist_load(double *used_wh);

// Retorna false em falha de escrita. Valores negativos/NaN sao gravados como 0.
bool energy_persist_save(double used_wh);

#endif
