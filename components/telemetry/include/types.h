#ifndef TELEMETRIA_BASE_TYPES_H
#define TELEMETRIA_BASE_TYPES_H

#include <stdbool.h>

struct data_boat {
    float voltage_instant;
    float current_instant;
};

struct data_telemetry {
    struct data_boat data_boat;
    double usedEnergy;
};

// Ultimo estado conhecido do barco (snapshot devolvido por telemetry_store_get)
struct boat_state {
    float voltage_instant;      // V
    float current_instant;      // A
    double used_energy;         // Wh
    float time_remaining_min;
    float energy_remaining;     // Wh
    bool link_ok;               // HEARTBEAT recebido nos ultimos 3 s
};

#endif //TELEMETRIA_BASE_TYPES_H
