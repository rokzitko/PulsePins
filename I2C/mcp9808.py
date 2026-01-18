#!/usr/bin/env python3
"""
Read temperature from MCP9808 on /dev/i2c-1, address 0x18, once or repeatedly.

Ambient temperature register: 0x05 (16-bit, big-endian).
Resolution: 0.0625 °C.

Notes on sign handling:
If bit 12 (0x1000) is set, temperature is negative; common implementation is temp -= 256.
"""

import argparse
import sys
import time
from datetime import datetime, timezone

from smbus2 import SMBus

REG_AMBIENT_TEMP = 0x05

def parse_i2c_address(s: str) -> int:
    s = s.strip().lower()
    base = 16 if s.startswith("0x") else 10
    return int(s, base)

def read_temp_c(bus: SMBus, addr: int) -> float:
    data = bus.read_i2c_block_data(addr, REG_AMBIENT_TEMP, 2)
    msb, lsb = data[0], data[1]
    raw = (msb << 8) | lsb

    sign = raw & 0x1000
    temp_raw = raw & 0x0FFF  # lower 13 bits (includes sign bit; handled separately)

    temp_c = (temp_raw & 0x0FFF) * 0.0625
    if sign:
        temp_c -= 256.0

    return temp_c

def now_iso_utc() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")

def main() -> int:
    ap = argparse.ArgumentParser(description="Read MCP9808 temperature over I2C (once or repeatedly).")
    ap.add_argument("--bus", type=int, default=1, help="I2C bus number (default: 1 -> /dev/i2c-1)")
    ap.add_argument("--addr", type=str, default="0x18",
                    help="I2C address (default: 0x18). Accepts decimal (e.g. 24) or hex (e.g. 0x18).")

    ap.add_argument("--delay", type=float, default=1.0,
                    help="Delay between reads in seconds when looping (default: 1.0).")
    ap.add_argument("--count", type=int, default=0,
                    help="Number of samples to take. 0 means run forever (default: 0).")

    ap.add_argument("--fahrenheit", action="store_true", help="Print in Fahrenheit (in addition to Celsius).")
    ap.add_argument("--timestamp", action="store_true", help="Prefix each line with ISO-8601 UTC timestamp.")
    ap.add_argument("--csv", action="store_true",
                    help="CSV output (implies --timestamp). Columns: timestamp,temp_c[,temp_f]")
    ap.add_argument("--reopen", action="store_true",
                    help="Re-open the I2C bus for each sample (slower, but can recover from some glitches).")
    ap.add_argument("--quiet-errors", action="store_true",
                    help="On I2C read errors, print a placeholder line and continue (instead of exiting).")
    args = ap.parse_args()

    try:
        addr = parse_i2c_address(args.addr)
        if not (0x03 <= addr <= 0x77):
            raise ValueError(f"I2C address out of 7-bit range: 0x{addr:02x}")
        if args.delay < 0:
            raise ValueError("--delay must be non-negative")

        if args.csv:
            args.timestamp = True
            # Print header for CSV
            if args.fahrenheit:
                print("timestamp,temp_c,temp_f")
            else:
                print("timestamp,temp_c")

        def format_line(t_c: float) -> str:
            if args.fahrenheit:
                t_f = t_c * 9.0 / 5.0 + 32.0
            if args.csv:
                if args.fahrenheit:
                    return f"{now_iso_utc()},{t_c:.4f},{t_f:.4f}"
                return f"{now_iso_utc()},{t_c:.4f}"

            prefix = f"{now_iso_utc()}  " if args.timestamp else ""
            if args.fahrenheit:
                return f"{prefix}{t_c:.4f} °C  ({t_f:.4f} °F)"
            return f"{prefix}{t_c:.4f} °C"

        n = 0

        if not args.reopen:
            with SMBus(args.bus) as bus:
                while True:
                    try:
                        t_c = read_temp_c(bus, addr)
                        print(format_line(t_c), flush=True)
                    except OSError as e:
                        if args.quiet_errors:
                            # Emit placeholder output, keep cadence
                            if args.csv:
                                print(f"{now_iso_utc()},NaN" + (",NaN" if args.fahrenheit else ""), flush=True)
                            else:
                                prefix = f"{now_iso_utc()}  " if args.timestamp else ""
                                print(f"{prefix}ERROR: {e}", file=sys.stderr, flush=True)
                        else:
                            raise

                    n += 1
                    if args.count and n >= args.count:
                        break
                    if args.delay:
                        time.sleep(args.delay)

        else:
            # Re-open for each sample
            while True:
                try:
                    with SMBus(args.bus) as bus:
                        t_c = read_temp_c(bus, addr)
                    print(format_line(t_c), flush=True)
                except OSError as e:
                    if args.quiet_errors:
                        if args.csv:
                            print(f"{now_iso_utc()},NaN" + (",NaN" if args.fahrenheit else ""), flush=True)
                        else:
                            prefix = f"{now_iso_utc()}  " if args.timestamp else ""
                            print(f"{prefix}ERROR: {e}", file=sys.stderr, flush=True)
                    else:
                        raise

                n += 1
                if args.count and n >= args.count:
                    break
                if args.delay:
                    time.sleep(args.delay)

        return 0

    except KeyboardInterrupt:
        return 130
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
