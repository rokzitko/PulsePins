#!/usr/bin/env python3
"""
Read temperature from MCP9808 on /dev/i2c-1, address 0x18.

MCP9808 ambient temperature register: 0x05 (16-bit, big-endian).
Temperature format:
  - Bits 15..13: flags (including sign)
  - Bits 12..0 : temperature magnitude in 0.0625 °C steps
  - If bit 12 (0x1000) is set => negative temperature (two's complement style per datasheet)
"""

import argparse
import sys
from smbus2 import SMBus

REG_AMBIENT_TEMP = 0x05

def parse_i2c_address(s: str) -> int:
    """Accept '18' (decimal) or '0x18' (hex). If user passes plain '18', treat as hex only if asked."""
    s = s.strip().lower()
    base = 16 if s.startswith("0x") else 10
    return int(s, base)

def read_temp_c(bus: SMBus, addr: int) -> float:
    # MCP9808 returns 2 bytes, MSB then LSB, for register 0x05
    data = bus.read_i2c_block_data(addr, REG_AMBIENT_TEMP, 2)
    msb, lsb = data[0], data[1]
    raw = (msb << 8) | lsb

    # Clear alert flags (bits 15..13), keep sign bit (bit 12) + magnitude (bits 11..0)
    sign = raw & 0x1000
    temp_raw = raw & 0x0FFF  # bits 11..0 plus sign bit if present (we handle sign separately)

    # Convert magnitude
    temp_c = (temp_raw & 0x0FFF) * 0.0625

    # Negative?
    if sign:
        # Datasheet convention: if sign bit set, temperature is negative and magnitude is (temp_raw & 0x0FFF)
        # Many implementations use: temp_c -= 256.0
        temp_c -= 256.0

    return temp_c

def main() -> int:
    ap = argparse.ArgumentParser(description="Read MCP9808 temperature over I2C.")
    ap.add_argument("--bus", type=int, default=1, help="I2C bus number (default: 1 -> /dev/i2c-1)")
    ap.add_argument("--addr", type=str, default="0x18",
                    help="I2C address (default: 0x18). Accepts decimal (e.g. 24) or hex (e.g. 0x18).")
    ap.add_argument("--fahrenheit", action="store_true", help="Also print temperature in Fahrenheit.")
    args = ap.parse_args()

    try:
        addr = parse_i2c_address(args.addr)
        if not (0x03 <= addr <= 0x77):
            raise ValueError(f"I2C address out of 7-bit range: 0x{addr:02x}")

        with SMBus(args.bus) as bus:
            t_c = read_temp_c(bus, addr)

        if args.fahrenheit:
            t_f = t_c * 9.0 / 5.0 + 32.0
            print(f"{t_c:.4f} °C  ({t_f:.4f} °F)")
        else:
            print(f"{t_c:.4f} °C")

        return 0

    except FileNotFoundError:
        print(f"Error: /dev/i2c-{args.bus} not found. Is the i2c-dev module loaded and bus enabled?",
              file=sys.stderr)
        return 2
    except PermissionError:
        print("Error: permission denied opening I2C device. Try running with sudo or adjust udev permissions.",
              file=sys.stderr)
        return 3
    except OSError as e:
        print(f"I2C OS error: {e}", file=sys.stderr)
        return 4
    except ValueError as e:
        print(f"Argument error: {e}", file=sys.stderr)
        return 5

if __name__ == "__main__":
    raise SystemExit(main())
