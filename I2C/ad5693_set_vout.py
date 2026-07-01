#!/usr/bin/env python3
import argparse
import sys
from smbus2 import SMBus

# AD5693/AD5693R command bytes (upper nibble is command; low nibble is don't-care)
CMD_WRITE_DAC_AND_INPUT = 0x30  # 0b0011xxxx: Write DAC and input registers (updates output)
CMD_WRITE_CONTROL       = 0x40  # 0b0100xxxx: Write control register

def clamp(x, lo, hi):
    return lo if x < lo else hi if x > hi else x

def write_control(bus: SMBus, addr: int, gain_2x: bool, ref_disable: bool):
    """
    Control register bits (D15..D11): RESET, PD1, PD0, REF, GAIN.
    - gain_2x: True => output range 0..2*VREF; False => 0..VREF
    - ref_disable: For AD5693R variants, REF=0 means reference enabled (default), REF=1 disables it.
    """
    control = 0
    # PD bits default 0 (normal mode), RESET default 0
    if ref_disable:
        control |= (1 << 12)          # REF bit
    if gain_2x:
        control |= (1 << 11)          # GAIN bit

    hi = (control >> 8) & 0xFF
    lo = control & 0xFF
    bus.write_i2c_block_data(addr, CMD_WRITE_CONTROL, [hi, lo])

def write_dac_code(bus: SMBus, addr: int, code: int):
    code = int(code) & 0xFFFF
    hi = (code >> 8) & 0xFF
    lo = code & 0xFF
    bus.write_i2c_block_data(addr, CMD_WRITE_DAC_AND_INPUT, [hi, lo])

def main() -> int:
    ap = argparse.ArgumentParser(description="Set AD5693/AD5693R DAC output voltage over I2C.")
    ap.add_argument("--bus", type=int, default=1, help="I2C bus number (default: 1 => /dev/i2c-1)")
    ap.add_argument("--addr", type=lambda s: int(s, 0), default=0x4C,
                    help="7-bit I2C address (default: 0x4C). Accepts 0x.. or decimal.")
    ap.add_argument("--vout", type=float, default=2.0, help="Desired output voltage in volts (default: 2.0)")
    ap.add_argument("--vref", type=float, default=2.5,
                    help="Reference voltage in volts (default: 2.5; typical for AD5693R internal ref)")
    ap.add_argument("--gain", type=int, choices=(1, 2), default=1,
                    help="Output amplifier gain (1 => 0..Vref, 2 => 0..2*Vref). Default: 1")
    ap.add_argument("--bits", type=int, default=16, choices=(12, 14, 16),
                    help="Resolution bits for code calculation; 12/14-bit codes are left-aligned. Default: 16")
    ap.add_argument("--set-control", action="store_true",
                    help="Write control register (sets gain; keeps normal mode; keeps internal ref enabled).")
    ap.add_argument("--disable-ref", action="store_true",
                    help="Disable internal reference (AD5693R variants). Use only if you know you want this.")
    args = ap.parse_args()

    if not (0x03 <= args.addr <= 0x77):
        print(f"Error: address out of 7-bit range: 0x{args.addr:02X}", file=sys.stderr)
        return 2
    if args.vref <= 0:
        print("Error: vref must be > 0", file=sys.stderr)
        return 2
    if args.disable_ref and not args.set_control:
        print("Error: --disable-ref requires --set-control so the DAC control register is updated", file=sys.stderr)
        return 2
    if args.gain != 1 and not args.set_control:
        print("Error: non-default --gain requires --set-control so the DAC gain register is updated", file=sys.stderr)
        return 2

    full_scale = args.vref * args.gain
    max_code = (1 << args.bits) - 1

    # Compute code (clamp to range)
    code_f = (args.vout / full_scale) * max_code
    code = int(round(code_f))
    code = clamp(code, 0, max_code)
    dac_code = code << (16 - args.bits)

    try:
        with SMBus(args.bus) as bus:
            if args.set_control:
                write_control(bus, args.addr, gain_2x=(args.gain == 2), ref_disable=args.disable_ref)

            write_dac_code(bus, args.addr, dac_code)

        if args.bits == 16:
            print(f"addr=0x{args.addr:02X} bus={args.bus} vout={args.vout:.6f} V -> code=0x{code:04X} ({code})")
        else:
            code_hex_width = (args.bits + 3) // 4
            print(f"addr=0x{args.addr:02X} bus={args.bus} vout={args.vout:.6f} V -> code=0x{code:0{code_hex_width}X} ({code}) dac_word=0x{dac_code:04X}")
        return 0

    except PermissionError:
        print("Permission denied opening I2C device. Try sudo or adjust udev permissions.", file=sys.stderr)
        return 3
    except FileNotFoundError:
        print(f"/dev/i2c-{args.bus} not found. Is i2c-dev loaded and the bus enabled?", file=sys.stderr)
        return 4
    except OSError as e:
        print(f"I2C error: {e}", file=sys.stderr)
        return 5

if __name__ == "__main__":
    raise SystemExit(main())
