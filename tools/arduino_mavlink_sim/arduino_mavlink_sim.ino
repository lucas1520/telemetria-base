/*
 * Simulador MAVLink para o telemetria-base, em Arduino (equivalente ao tools/mavlink_sim.py).
 *
 * Envia HEARTBEAT (1 Hz) e BATTERY_STATUS (5 Hz) em MAVLink v1, sem biblioteca.
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

#define CELLS 4

static uint8_t seq = 0;

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

// Pack de 4 celulas em torno de 14,8 V e 20 A, variando com o tempo.
static void send_battery_status(float t_s) {
    uint8_t p[36] = {0};

    float pack_mv = 14800.0f + 400.0f * sin(t_s / 5.0f);
    int16_t current_ca = (int16_t)((20.0f + 10.0f * sin(t_s / 3.0f)) * 100.0f);

    put_u32(p + 0, (uint32_t)-1);    // current_consumed (desconhecido)
    put_u32(p + 4, (uint32_t)-1);    // energy_consumed (desconhecido)
    put_u16(p + 8, 32767);           // temperature (desconhecida)
    for (uint8_t i = 0; i < 10; i++) {
        put_u16(p + 10 + 2 * i, i < CELLS ? (uint16_t)(pack_mv / CELLS) : 0xFFFF);
    }
    put_u16(p + 30, (uint16_t)current_ca);
    p[32] = 0;                       // id
    p[33] = 0;                       // battery_function: MAV_BATTERY_FUNCTION_UNKNOWN
    p[34] = 1;                       // type: MAV_BATTERY_TYPE_LIPO
    p[35] = (uint8_t)-1;             // battery_remaining (desconhecido)
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
