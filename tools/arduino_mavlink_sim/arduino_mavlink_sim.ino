/*
 * Simulador MAVLink para o telemetria-base, em Arduino (equivalente ao tools/mavlink_sim.py).
 *
 * Envia HEARTBEAT (1 Hz) e BATTERY_STATUS (5 Hz) em MAVLink v1, sem biblioteca.
 * Simula o pack de 48 V / 38 Ah do barco alimentando o motor CC de 6 kW: tensao com sag,
 * corrente seguindo o perfil de carga e Ah/Wh consumidos integrados ao longo do tempo.
 *
 * Ligacao (so 2 fios):
 *   Arduino TX (pino 1)  ->  GPIO16 do ESP32 (RX da UART2)
 *   Arduino GND          ->  GND do ESP32
 *
 * ATENCAO: o GPIO16 do ESP32 e 3,3 V e NAO tolera 5 V.
 *   - Placas 3,3 V (ESP32, Due, Pro Mini 3,3 V, ...): ligar direto.
 *   - Placas 5 V (Uno, Nano, Mega, Pro Mini 5 V): colocar um divisor de tensao no TX:
 *       TX --[1k]--+--[2k]-- GND, e a juncao dos resistores vai no GPIO16.
 *
 * Nao conectar nada no pino 0 (RX) do Uno/Nano durante o upload.
 * Em placas com Serial1 (Mega, Leonardo), troque MAV_SERIAL para Serial1.
 */

#define MAV_SERIAL Serial
#define MAV_BAUD 57600   // deve bater com UART_BAUD_RATE do firmware

#define MAV_SYSID 1
#define MAV_COMPID 1

#define MAV_STX_V1 0xFE
#define MAVLINK_MSG_ID_HEARTBEAT 0
#define MAVLINK_MSG_ID_BATTERY_STATUS 147
#define HEARTBEAT_CRC_EXTRA 50
#define BATTERY_STATUS_CRC_EXTRA 154

#define HEARTBEAT_PERIOD_MS 1000UL
#define BATTERY_PERIOD_MS 200UL

// Pack do Fenix: 48 V nominal (13S Li-ion, 54,6 V cheio), 38 Ah, motor CC escovado de 6 kW.
#define PACK_CELLS_SERIES 13
#define PACK_CAPACITY_AH 38.0f
#define PACK_INTERNAL_OHM 0.040f   // resistencia interna do pack (sag ~5 V a 125 A)
#define MOTOR_POWER_W 6000.0f
#define AUX_POWER_W 70.0f          // cargas auxiliares (~1,5 A a 48 V)
#define LOAD_CYCLE_S 120.0f        // duracao de um ciclo do perfil de carga
#define ENERGY_TIME_SCALE 1.0f     // >1 descarrega mais rapido (so o simulador; o firmware integra em tempo real)

#define CELL_SLOTS 10              // BATTERY_STATUS v1 so tem 10 celulas; o pack e dividido entre elas

static uint8_t seq = 0;

// Tensao de celula em circuito aberto x estado de carga (Li-ion NMC).
static const float SOC_PTS[] = {0.00f, 0.10f, 0.30f, 0.50f, 0.70f, 0.90f, 1.00f};
static const float CELL_OCV_V[] = {3.00f, 3.45f, 3.60f, 3.70f, 3.82f, 4.00f, 4.20f};

// Potencia do motor (fracao de 6 kW) ao longo do ciclo: parado, aceleracao, potencia
// maxima, cruzeiro, arrancada intermediaria e desaceleracao.
static const float LOAD_T_S[] = {0, 8, 14, 20, 26, 90, 94, 98, 104, 116, 120};
static const float LOAD_FRAC[] = {0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.85f, 0.85f, 0.5f, 0.05f, 0.0f};

static float consumed_ah = 0.0f;
static float consumed_wh = 0.0f;

static float interpolate(const float *xs, const float *ys, uint8_t n, float x) {
    if (x <= xs[0]) {
        return ys[0];
    }
    for (uint8_t i = 1; i < n; i++) {
        if (x <= xs[i]) {
            return ys[i - 1] + (ys[i] - ys[i - 1]) * (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
        }
    }
    return ys[n - 1];
}

static uint16_t crc_accumulate(uint8_t data, uint16_t crc) {
    uint8_t tmp = data ^ (uint8_t)(crc & 0xFF);
    tmp ^= (tmp << 4);
    return (crc >> 8) ^ ((uint16_t)tmp << 8) ^ ((uint16_t)tmp << 3) ^ (tmp >> 4);
}

static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = v >> 8;
}

static void put_u32(uint8_t *p, uint32_t v) {
    put_u16(p, v & 0xFFFF);
    put_u16(p + 2, v >> 16);
}

static void send_frame(uint8_t msgid, const uint8_t *payload, uint8_t len, uint8_t crc_extra) {
    uint8_t header[6] = {MAV_STX_V1, len, seq++, MAV_SYSID, MAV_COMPID, msgid};

    uint16_t crc = 0xFFFF;
    for (uint8_t i = 1; i < 6; i++) {
        crc = crc_accumulate(header[i], crc);
    }
    for (uint8_t i = 0; i < len; i++) {
        crc = crc_accumulate(payload[i], crc);
    }
    crc = crc_accumulate(crc_extra, crc);

    MAV_SERIAL.write(header, sizeof(header));
    MAV_SERIAL.write(payload, len);
    MAV_SERIAL.write((uint8_t)(crc & 0xFF));
    MAV_SERIAL.write((uint8_t)(crc >> 8));
}

static void send_heartbeat(void) {
    uint8_t p[9] = {0};
    put_u32(p, 0);   // custom_mode
    p[4] = 11;       // type: MAV_TYPE_SURFACE_BOAT
    p[5] = 3;        // autopilot: MAV_AUTOPILOT_ARDUPILOTMEGA
    p[6] = 0;        // base_mode
    p[7] = 4;        // system_status: MAV_STATE_ACTIVE
    p[8] = 3;        // mavlink_version
    send_frame(MAVLINK_MSG_ID_HEARTBEAT, p, sizeof(p), HEARTBEAT_CRC_EXTRA);
}

// Pack de 48 V / 38 Ah alimentando um motor de 6 kW com potencia constante: a corrente sai de
// P = V * I, com V = Voc(SoC) - I * R. Integra Ah e Wh consumidos para current_consumed,
// energy_consumed e battery_remaining.
static void send_battery_status(float t_s) {
    uint8_t p[36] = {0};

    float soc = 1.0f - consumed_ah / PACK_CAPACITY_AH;
    if (soc < 0.0f) {
        soc = 0.0f;
    }
    float voc = PACK_CELLS_SERIES * interpolate(SOC_PTS, CELL_OCV_V, sizeof(SOC_PTS) / sizeof(SOC_PTS[0]), soc);

    float cycle_t = fmodf(t_s, LOAD_CYCLE_S);
    float load = interpolate(LOAD_T_S, LOAD_FRAC, sizeof(LOAD_T_S) / sizeof(LOAD_T_S[0]), cycle_t);
    if (load > 0.0f) {
        load *= 1.0f + 0.04f * sinf(t_s * 2.3f);   // ripple de carga
        if (load > 1.0f) {
            load = 1.0f;                           // o motor nao passa de 6 kW
        }
    }
    float power_w = MOTOR_POWER_W * load + AUX_POWER_W;

    float disc = voc * voc - 4.0f * PACK_INTERNAL_OHM * power_w;
    float current_a = disc > 0.0f
        ? (voc - sqrtf(disc)) / (2.0f * PACK_INTERNAL_OHM)
        : voc / (2.0f * PACK_INTERNAL_OHM);
    float pack_v = voc - current_a * PACK_INTERNAL_OHM;

    float dt_h = (BATTERY_PERIOD_MS / 1000.0f) * ENERGY_TIME_SCALE / 3600.0f;
    consumed_ah += current_a * dt_h;
    consumed_wh += pack_v * current_a * dt_h;

    uint32_t pack_mv = (uint32_t)(pack_v * 1000.0f);
    put_u32(p + 0, (uint32_t)(int32_t)(consumed_ah * 1000.0f));   // current_consumed (mAh)
    put_u32(p + 4, (uint32_t)(int32_t)(consumed_wh * 36.0f));     // energy_consumed (hJ: 1 Wh = 36 hJ)
    put_u16(p + 8, 32767);                                        // temperature (desconhecida)
    for (uint8_t i = 0; i < 10; i++) {
        // Pack dividido igualmente entre as celulas; o firmware soma as 10 e obtem a tensao do pack.
        uint16_t slot_mv = (uint16_t)(pack_mv / CELL_SLOTS) + (i < pack_mv % CELL_SLOTS ? 1 : 0);
        put_u16(p + 10 + 2 * i, slot_mv);
    }
    put_u16(p + 30, (uint16_t)(int16_t)(current_a * 100.0f));    // current_battery (cA)
    p[32] = 0;                       // id
    p[33] = 2;                       // battery_function: MAV_BATTERY_FUNCTION_PROPULSION
    p[34] = 3;                       // type: MAV_BATTERY_TYPE_LION
    p[35] = (uint8_t)(soc * 100.0f + 0.5f);   // battery_remaining (%)
    send_frame(MAVLINK_MSG_ID_BATTERY_STATUS, p, sizeof(p), BATTERY_STATUS_CRC_EXTRA);
}

void setup() {
    MAV_SERIAL.begin(MAV_BAUD);
}

void loop() {
    static unsigned long next_heartbeat = 0;
    static unsigned long next_battery = 0;
    unsigned long now = millis();

    if ((long)(now - next_heartbeat) >= 0) {
        send_heartbeat();
        next_heartbeat += HEARTBEAT_PERIOD_MS;
    }

    if ((long)(now - next_battery) >= 0) {
        send_battery_status(now / 1000.0f);
        next_battery += BATTERY_PERIOD_MS;
    }
}
