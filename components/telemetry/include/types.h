#ifndef TELEMETRIA_BASE_TYPES_H
#define TELEMETRIA_BASE_TYPES_H

struct data_boat {
    float voltage_instant;
    float current_instant;
};

struct data_telemetry {
    struct data_boat data_boat;
    double usedEnergy;
};

struct telemetry_packet {
    float voltage_instant;
    float current_instant;
    double usedEnergy;
};

#endif //TELEMETRIA_BASE_TYPES_H
