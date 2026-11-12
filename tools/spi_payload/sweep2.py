#!/usr/bin/env python3
"""Sweep AD9959 CH1 at full amplitude with the SVA1015X fixed at 0-250 MHz."""

import argparse
import math
import subprocess
import tempfile
import time
from pathlib import Path

from sweep1 import PulsePins, Siglent, acquire, run_command


def sweep_points(count):
    if count < 2:
        raise ValueError("At least two points are required")
    return [(200e6 * index / (count - 1), 1023) for index in range(count)]


class FixedSpanSiglent(Siglent):
    START_HZ = 0
    STOP_HZ = 250000000
    SPAN_HZ = STOP_HZ - START_HZ
    RBW_HZ = 100000
    VBW_HZ = 300000
    PEAK_TOLERANCE_HZ = 500000

    def verify_range(self):
        for query, expected in ((":FREQ:STAR?", self.START_HZ), (":FREQ:STOP?", self.STOP_HZ)):
            actual = float(self.query(query))
            if not math.isfinite(actual) or abs(actual - expected) > 1:
                raise RuntimeError("Siglent fixed frequency range changed: " + query)

    def configure(self):
        self.identity = self.query("*IDN?")
        if "SVA1015X" not in self.identity:
            raise RuntimeError("Expected an SVA1015X, received " + self.identity)
        for command in (
            "*CLS", ":INST SA", ":INIT:CONT OFF", ":ABOR", ":OUTP OFF",
            f":FREQ:STAR {self.START_HZ}", f":FREQ:STOP {self.STOP_HZ}",
            ":BWID:AUTO OFF", f":BWID {self.RBW_HZ}",
            ":BWID:VID:AUTO OFF", f":BWID:VID {self.VBW_HZ}",
            ":UNIT:POW DBM", ":DISP:WIND:TRAC:Y:SPAC LOG",
            ":DISP:WIND:TRAC:Y:RLEV 0", ":DISP:WIND:TRAC:Y:PDIV 10",
            ":POW:ATT:AUTO OFF", ":POW:ATT 20", ":POW:GAIN OFF",
            ":TRIG:SOUR IMM", ":SWE:MODE AUTO", ":SWE:TIME:AUTO ON", ":SWE:COUN 1",
            ":TRAC1:MODE WRIT", ":DET:TRAC1 POS",
            ":TRAC2:MODE BLAN", ":TRAC3:MODE BLAN", ":TRAC4:MODE BLAN",
            ":CALC:MARK:TRCK OFF", ":CALC:MARK1:MODE POS", ":CALC:MARK1:TRAC 1",
            ":CALC:MARK1:FUNC OFF", ":CALC:MARK1:X:READ FREQ", ":CALC:MARK1:CPE OFF",
            ":CALC:MARK:PEAK:SEAR:MODE MAX",
        ):
            self.write(command)
        self.complete()
        self.verify_range()
        for query, expected in (
            (":BWID?", self.RBW_HZ), (":BWID:VID?", self.VBW_HZ),
            (":INIT:CONT?", 0), (":SWE:COUN?", 1), (":POW:ATT?", 20), (":POW:GAIN?", 0),
        ):
            actual = float(self.query(query))
            if not math.isfinite(actual) or abs(actual - expected) > 0.01:
                raise RuntimeError("Siglent setting verification failed: " + query)
        self.check_errors()
        return float(self.query(":SWE:TIME?"))

    def peak(self, frequency):
        # Only the DDS moves. Every fresh trace covers the same frequency range.
        self.verify_range()
        self.write(":ABOR")
        scan_time = float(self.query(":SWE:TIME?"))
        if not math.isfinite(scan_time) or scan_time <= 0:
            raise RuntimeError("Invalid Siglent sweep time")
        for attempt in range(3):
            self.write(":INIT:IMM")
            self.complete()
            # Allow trace publication on the bench firmware before reading the marker.
            time.sleep(scan_time + 0.02)
            self.write(":CALC:MARK1:MAX")
            self.complete()
            measured = float(self.query(":CALC:MARK1:X?"))
            level = float(self.query(":CALC:MARK1:Y?"))
            self.verify_range()
            self.check_errors()
            if frequency == 0:
                return math.nan, level
            if (math.isfinite(measured) and math.isfinite(level) and measured > 0
                    and level >= -80 and abs(measured - frequency) <= self.PEAK_TOLERANCE_HZ):
                return measured, level
        return math.nan, level


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board", default="root@192.168.3.47", help="PulsePins SSH destination")
    parser.add_argument("--analyzer", default="192.168.3.55", help="Siglent LAN address (TCP 5025)")
    parser.add_argument("--points", type=int, default=1000, help="points including both endpoints (default: 1000)")
    parser.add_argument("--duration", type=float, default=3600, help="nominal sweep seconds (default: 3600)")
    parser.add_argument("--output", type=Path, default=Path("sweep2.dat"), help="ASCII result file (default: sweep2.dat)")
    args = parser.parse_args()
    if args.points < 2 or not math.isfinite(args.duration) or args.duration <= 0:
        parser.error("--points must be at least 2 and --duration must be finite and positive")
    generator = Path(__file__).resolve().with_name("spi_ad9959")
    if not generator.is_file():
        parser.error(f"Build the generator first: make -C {generator.parent} spi_ad9959")
    points = sweep_points(args.points)
    with tempfile.TemporaryDirectory(prefix="sweep2-") as temporary:
        directory = Path(temporary)
        files = [directory / "setup.seq"]
        run_command([str(generator), str(files[0])])
        for index, (frequency, amplitude) in enumerate(points):
            path = directory / f"point-{index:04d}.seq"
            run_command([str(generator), "--set-ch1", str(path), f"{frequency:.9f}", str(amplitude)])
            files.append(path)
        with PulsePins(args.board, directory) as board, FixedSpanSiglent(args.analyzer) as analyzer:
            board.upload(files)
            scan_time = analyzer.configure()
            board.play("setup.seq")
            print(f"{analyzer.identity}\nSingle scan: {scan_time:.3f} s; "
                  f"nominal point interval: {args.duration / args.points:.3f} s.\n"
                  f"Sequences and playback logs: {args.board}:{board.directory}\n"
                  f"Starting {args.points} points, 0-200 MHz, full amplitude (1023/1023); "
                  "analyzer fixed at 0-250 MHz.", flush=True)
            with args.output.open("w", encoding="ascii") as output:
                output.write("# set_frequency_Hz measured_frequency_Hz\n"
                             f"# UTC {time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}\n"
                             f"# {analyzer.identity}\n"
                             f"# CH1 points={args.points} nominal_duration_s={args.duration:g}\n"
                             "# Full DAC current and constant amplitude code 1023; 500 MHz DDS clock, 100 MHz streamer\n"
                             f"# fixed_start_Hz={analyzer.START_HZ} fixed_stop_Hz={analyzer.STOP_HZ} "
                             f"RBW_Hz={analyzer.RBW_HZ} VBW_Hz={analyzer.VBW_HZ}\n"
                             f"# peak_tolerance_Hz={analyzer.PEAK_TOLERANCE_HZ}; wide-span trace peak, not a frequency counter\n"
                             f"# Playback logs: {args.board}:{board.directory}\n"
                             "# nan: DC, peak below -80 dBm, or no valid peak within tolerance of the setpoint\n")
                elapsed, valid = acquire(board, analyzer, points, args.duration, output)
            print(f"Saved {args.points} pairs to {args.output.resolve()} in {elapsed:.2f} s "
                  f"({valid} valid RF peaks; DC is nan).\n"
                  "CH1 remains at 200 MHz, full amplitude; analyzer remains at 0-250 MHz.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        raise SystemExit("Sweep interrupted; any completed rows are saved. CH1 holds its last setting.")
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        raise SystemExit(f"sweep2: {error}")
