#!/usr/bin/env python3
"""Simulador MAVLink para testar o telemetria-base sem o barco.

Envia HEARTBEAT (1 Hz) e BATTERY_STATUS (5 Hz) por uma porta serial ligada
na UART2 do ESP32 (RX=GPIO16, TX=GPIO17, GND comum, TX do adaptador no GPIO16).

Dependencias: pip install pymavlink pyserial
"""
import argparse
import math
import time

import serial
from pymavlink import mavutil

UINT16_MAX = 0xFFFF

HEARTBEAT_PERIOD_S = 1.0
BATTERY_PERIOD_S = 0.2
CELLS = 4


def build_battery_status(mav, t, invalid):
    """Pack de 4 celulas em torno de 14,8 V / 20 A, variando com o tempo."""
    voltages = [UINT16_MAX] * 10
    if invalid:
        # tensao e corrente desconhecidas: o firmware nao pode enfileirar a amostra
        current_ca = -1
    else:
        pack_mv = 14800 + 400 * math.sin(t / 5.0)
        for i in range(CELLS):
            voltages[i] = int(pack_mv / CELLS)
        current_ca = int((20.0 + 10.0 * math.sin(t / 3.0)) * 100)  # cA

    return mav.battery_status_encode(
        0,                                                    # id
        mavutil.mavlink.MAV_BATTERY_FUNCTION_ALL,
        mavutil.mavlink.MAV_BATTERY_TYPE_LIPO,
        32767,                                                # temperatura desconhecida
        voltages,
        current_ca,
        -1,                                                   # current_consumed
        -1,                                                   # energy_consumed
        -1,                                                   # battery_remaining
    )


def corrupt_crc(frame):
    """Inverte o ultimo byte (CRC) do frame."""
    data = bytearray(frame)
    data[-1] ^= 0xFF
    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", required=True, help="porta serial, ex.: /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=57600)
    parser.add_argument("--sysid", type=int, default=1)
    parser.add_argument("--bad-crc", action="store_true",
                        help="corrompe o CRC de 1 a cada 3 frames de BATTERY_STATUS")
    parser.add_argument("--no-heartbeat", action="store_true", help="nao envia HEARTBEAT")
    parser.add_argument("--invalid", action="store_true",
                        help="BATTERY_STATUS com corrente = -1 (desconhecida)")
    args = parser.parse_args()

    link = serial.Serial(args.port, args.baud)
    mav = mavutil.mavlink.MAVLink(link, srcSystem=args.sysid, srcComponent=1)

    start = time.monotonic()
    next_heartbeat = start
    next_battery = start
    battery_count = 0

    print(f"enviando em {args.port} @ {args.baud} (Ctrl+C para sair)")
    try:
        while True:
            now = time.monotonic()

            if not args.no_heartbeat and now >= next_heartbeat:
                mav.heartbeat_send(
                    mavutil.mavlink.MAV_TYPE_SURFACE_BOAT,
                    mavutil.mavlink.MAV_AUTOPILOT_ARDUPILOTMEGA,
                    0, 0, mavutil.mavlink.MAV_STATE_ACTIVE,
                )
                next_heartbeat += HEARTBEAT_PERIOD_S

            if now >= next_battery:
                msg = build_battery_status(mav, now - start, args.invalid)
                frame = msg.pack(mav)
                battery_count += 1
                if args.bad_crc and battery_count % 3 == 0:
                    frame = corrupt_crc(frame)
                link.write(frame)
                next_battery += BATTERY_PERIOD_S

            time.sleep(0.005)
    except KeyboardInterrupt:
        pass
    finally:
        link.close()


if __name__ == "__main__":
    main()
