#!/usr/bin/env python3
"""
TMP117 I2C reader for Linux.

- Reads temperature from register 0x00 (16-bit, signed).
- Converts to °C using TMP117 scale: 0.0078125 °C/LSB.

Requires: pip3 install smbus2

Fix version number in gpg-1.13.1_unknown-py3.8.egg-info

Generated using chatgpt, RZ Dec 2025
"""

import time
import argparse
from smbus2 import SMBus

# TMP117 register map (key ones)
REG_TEMP   = 0x00
REG_CONF   = 0x01
REG_T_HIGH = 0x02
REG_T_LOW  = 0x03
REG_EEPROM = 0x04
REG_OFFSET = 0x07
REG_ID     = 0x0F  # Device ID

TEMP_LSB_C = 0.0078125  # °C per LSB


def _read_u16(bus: SMBus, addr: int, reg: int) -> int:
    """Read unsigned 16-bit big-endian register."""
    data = bus.read_i2c_block_data(addr, reg, 2)
    return (data[0] << 8) | data[1]


def _read_s16(bus: SMBus, addr: int, reg: int) -> int:
    """Read signed 16-bit big-endian register."""
    val = _read_u16(bus, addr, reg)
    if val & 0x8000:
        val -= 0x10000
    return val


def read_temperature_c(bus: SMBus, addr: int) -> float:
    raw = _read_s16(bus, addr, REG_TEMP)
    return raw * TEMP_LSB_C


def dump_registers(bus: SMBus, addr: int) -> None:
    conf = _read_u16(bus, addr, REG_CONF)
    dev_id = _read_u16(bus, addr, REG_ID)
    thigh = _read_s16(bus, addr, REG_T_HIGH) * TEMP_LSB_C
    tlow  = _read_s16(bus, addr, REG_T_LOW) * TEMP_LSB_C
    offset = _read_s16(bus, addr, REG_OFFSET) * TEMP_LSB_C

    print(f"I2C addr: 0x{addr:02X}")
    print(f"Device ID (0x0F): 0x{dev_id:04X}")
    print(f"Config    (0x01): 0x{conf:04X}")
    print(f"T_HIGH    (0x02): {thigh:.4f} °C")
    print(f"T_LOW     (0x03): {tlow:.4f} °C")
    print(f"Offset    (0x07): {offset:.4f} °C")


def main():
    ap = argparse.ArgumentParser(description="Read TMP117 over I2C on Linux")
    ap.add_argument("--bus", type=int, default=1, help="I2C bus number (default: 1 -> /dev/i2c-1)")
    ap.add_argument("--addr", type=lambda x: int(x, 0), default=0x48, help="I2C address (default: 0x48)")
    ap.add_argument("--interval", type=float, default=1.0, help="Polling interval seconds (default: 1.0)")
    ap.add_argument("--once", action="store_true", help="Read once and exit")
    ap.add_argument("--dump", action="store_true", help="Dump key registers then exit")
    args = ap.parse_args()

    with SMBus(args.bus) as bus:
        if args.dump:
            dump_registers(bus, args.addr)
            return

        if args.once:
            t = read_temperature_c(bus, args.addr)
            print(f"{t:.2f} °C")
            return

        while True:
            t = read_temperature_c(bus, args.addr)
            print(f"{t:.2f} °C")
            time.sleep(args.interval)


if __name__ == "__main__":
    main()
