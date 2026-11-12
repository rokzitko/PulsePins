#!/usr/bin/env python3
"""Validate AD9959 SPI writes and finite back-to-back frequency-update bursts."""

import argparse
import csv
import hashlib
import json
import math
import random
import re
import shlex
import subprocess
import tempfile
import time
from pathlib import Path

from check_readback import waveform_crc32
from sequence_compact import compact_runs
from sweep1 import PulsePins, run_command
from sweep2 import FixedSpanSiglent


CLOCK_HZ = 100000000
DDS_CLOCK_HZ = 500000000
IDLE = 2


def frequencies(start, stop, points):
    if points < 2 or not (math.isfinite(start) and math.isfinite(stop) and 0 < start < stop <= 200e6):
        raise ValueError("Require at least two points and 0 < start < stop <= 200 MHz")
    return [start + (stop - start) * i / (points - 1) for i in range(points)]


def tuning_word(frequency):
    word = math.floor(frequency * 2**32 / DDS_CLOCK_HZ + 0.5)
    if not 0 < word < 2**32:
        raise ValueError("Frequency rounds to DC or exceeds tuning-word range")
    return word


class Ad9959Waveform:
    """AD9959 serial writes on J3/J4; times in 10 ns ticks."""

    def __init__(self, spi_clock, clock_source="qout", serial_bits=1,
                 pre_guard_ticks=2, post_guard_ticks=6, update_ticks=2):
        if serial_bits not in (1, 4):
            raise ValueError("Serial width must be 1 or 4 bits")
        if any(not isinstance(value, int) or value < 0 for value in (pre_guard_ticks, post_guard_ticks)):
            raise ValueError("Guard ticks must be nonnegative integers")
        if not isinstance(update_ticks, int) or update_ticks < 1:
            raise ValueError("UPDATE high/low must each be at least one 10 ns tick")
        if clock_source != "streamer" and (serial_bits, pre_guard_ticks, post_guard_ticks, update_ticks) != (1, 2, 6, 2):
            raise ValueError("Four-bit mode and adjustable guards/UPDATE require forwarded streamer SCLK")
        self.serial_bits = serial_bits
        self.pre_guard_ticks = pre_guard_ticks
        self.post_guard_ticks = post_guard_ticks
        self.update_pulse_ticks = update_ticks
        self.clock_source = clock_source
        if clock_source == "streamer":
            if spi_clock != CLOCK_HZ:
                raise ValueError("Forwarded streamer SCLK requires exactly 100 MHz")
            self.half = None
            self.clock = CLOCK_HZ
            self.update_ticks = 40 // serial_bits + pre_guard_ticks + post_guard_ticks + 2 * update_ticks
        elif clock_source == "qout":
            if not math.isfinite(spi_clock) or not 0 < spi_clock <= CLOCK_HZ / 2:
                raise ValueError("QOUT SPI clock must be finite and in (0, 50 MHz]")
            self.half = math.floor(CLOCK_HZ / (2 * spi_clock) + 0.5)
            self.clock = CLOCK_HZ / (2 * self.half)
            self.update_ticks = 80 * self.half + 12
        else:
            raise ValueError("Unknown SPI clock source")
        self.runs = []
        self.pins = IDLE
        self.emit(1, IDLE)

    def emit(self, ticks, pins):
        if not 0 < ticks <= 0xFFFFFFFF:
            raise ValueError("Invalid waveform duration")
        if self.runs and self.runs[-1][1] == pins:
            previous, _ = self.runs[-1]
            self.runs[-1] = (previous + ticks, pins)
        else:
            self.runs.append((ticks, pins))
        self.pins = pins

    def write(self, address, value, length):
        forwarded = self.clock_source == "streamer"
        # A free-running SCLK would consume extra bits during a selected setup
        # interval. Keep CS high here; launch CS low with the first data bit.
        if not forwarded or self.pre_guard_ticks:
            self.emit(self.pre_guard_ticks if forwarded else 2, self.pins | 2 if forwarded else self.pins & ~2)
        for byte in bytes([address]) + value.to_bytes(length, "big"):
            for bit in range(8 - self.serial_bits, -1, -self.serial_bits):
                pins = ((byte >> bit) & ((1 << self.serial_bits) - 1)) << 2
                self.emit(1 if forwarded else self.half, pins)
                if not forwarded:
                    self.emit(self.half, pins | 1)
        if not forwarded:
            self.emit(2, self.pins & ~1)  # CS hold after the final falling edge
        # With forwarded SCLK, the last bit is sampled on the edge launching
        # CS high. This assumes clock arrival before the new QOUT state at DDS.
        if not forwarded or self.post_guard_ticks:
            self.emit(self.post_guard_ticks if forwarded else 4, self.pins | 2)
        else:
            # UPDATE will launch CS high on the same tick; no extra selected clock.
            self.pins |= 2

    def update(self, ticks=None):
        if ticks is None:
            ticks = self.update_pulse_ticks
        self.emit(ticks, self.pins | 64)
        self.emit(ticks, self.pins & ~64)

    def initialize(self):
        # Reset starts in one-bit mode. Keep known-good setup timing independent
        # of the burst settings; SDIO_3 stays low until four-bit mode is active.
        setup = Ad9959Waveform(self.clock, self.clock_source)
        setup._initialize_single_bit()
        if self.serial_bits == 4:
            setup.write(0, 0x26, 1)  # CH1, four-bit, MSB-first; written in one-bit mode
            setup.update()
            setup.emit(100, IDLE)  # allow CSR mode change before the first quad write
        for ticks, pins in setup.runs[1:]:  # this waveform already has its initial idle tick
            self.emit(ticks, pins)

    def _initialize_single_bit(self):
        self.emit(100, IDLE | 128)
        self.emit(100, IDLE)
        self.write(1, 0xD00000, 3)
        self.update(100)  # 1 us while the PLL is not yet established
        self.emit(200000, IDLE)
        self.write(0, 0xF0, 1)
        self.write(3, 0x000301, 3)
        self.write(5, 0, 2)
        self.write(6, 0x0013FF, 3)
        for channel, frequency in enumerate((50e6, 100e6, 100e6, 125e6)):
            self.write(0, 0x10 << channel, 1)
            self.write(4, tuning_word(frequency), 4)
        self.update()
        self.write(0, 0x20, 1)  # CH1 selected once for subsequent FTW-only writes

    def sweep(self, values):
        for frequency in values:
            self.write(4, tuning_word(frequency), 4)
            self.update()

    def save(self, path):
        self.emit(1, IDLE)
        # Reuse common SPI half-cycles in the FPGA rather than sending every
        # edge over DMA. Stored data is not emitted until a replay references it.
        if self.clock_source == "qout":
            slots = [(self.half, pins) for pins in (0, 1, 4, 5, 4, 5, 0, 1)]
            records = compact_runs(self.runs, slots)
        else:
            records = [f"d {ticks} 0x{pins:x}" for ticks, pins in self.runs]
        path.write_text("\n".join(records) + "\nfinal 0x2\nf\n", encoding="ascii")
        return {
            "file": path.name,
            "ticks": sum(ticks for ticks, _ in self.runs),
            "records": len(self.runs),
            "encoded_records": len(records),
            "crc32": waveform_crc32(self.runs),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }


class Analyzer(FixedSpanSiglent):
    RBW_HZ = 1000
    VBW_HZ = 3000

    def __init__(self, host, start, stop, tolerance):
        super().__init__(host)
        margin = (stop - start) / 2
        self.START_HZ = max(0, start - margin)
        self.STOP_HZ = stop + margin
        self.SPAN_HZ = self.STOP_HZ - self.START_HZ
        self.PEAK_TOLERANCE_HZ = tolerance


class Board(PulsePins):
    def upload(self, files):
        self.directory = self.remote("mktemp -d /home/root/spi-rate.XXXXXX").strip().splitlines()[-1]
        if not re.fullmatch(r"/home/root/spi-rate\.[A-Za-z0-9]+", self.directory):
            raise RuntimeError("Unexpected remote directory: " + self.directory)
        run_command(["scp", *self.options, *map(str, files), f"{self.board}:{self.directory}/"], timeout=90)

    def build(self, output):
        source = shlex.quote(f"{self.directory}/sequence_dma.cc")
        binary = shlex.quote(f"{self.directory}/sequence_dma")
        command = (f"g++ -std=c++17 -O2 -pthread -DTIDBITNR=35 -Dsoc_cv_av "
                   f"-I/home/root/c++ -I/home/root/c++/include "
                   f"-I/home/root/c++/third_party/hwlib/include "
                   f"-I/home/root/c++/third_party/hwlib/include/soc_cv_av "
                   f"{source} -o {binary}")
        result = subprocess.run(["ssh", *self.options, self.board, command],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=180)
        output.write_text(result.stdout)
        if result.returncode:
            raise RuntimeError(f"Board helper build failed; see {output}")

    def play_checked(self, sequence, log):
        command = (
            "env -u PP_RANDOM_FINAL -u PP_IGNORE_QOUT_FINAL -u PP_IGNORE_RB_ERROR_IF_CRC_OK "
            f"timeout 20s {shlex.quote(self.directory + '/sequence_dma')} "
            f"-file {shlex.quote(self.directory + '/' + sequence['file'])} "
            f"-crc 0x{sequence['crc32']:08x} -i 0x2 -sobe"
        )
        result = subprocess.run(["ssh", *self.options, self.board, command],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=25)
        log.write_text(result.stdout)
        match = re.search(r"DMA_RESULT valid=1 expected_crc=0x([0-9a-f]+) output_crc=0x([0-9a-f]+) "
                          r"readback_crc=0x([0-9a-f]+) final=0x2 ticks=(\d+) capture_overflow=([01])", result.stdout)
        if (result.returncode or not match or any(int(match[i], 16) != sequence["crc32"] for i in (1, 2, 3))
                or int(match[4]) != sequence["ticks"]):
            raise RuntimeError(f"Digital waveform verification failed; see {log}")
        return bool(int(match[5]))


def ensure_idle():
    processes = run_command(["ps", "-eo", "args="])
    if re.search(r"^\S*python\S*\s+(?:-\S+\s+)*\S*/?sweep[12]\.py\b", processes, re.MULTILINE):
        raise RuntimeError("A previous frequency sweep is still running; finish or stop it before this test")


def save_report(directory, report):
    (directory / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    rows = report.get("measurements", [])
    measured = [row for row in rows if row["rf_pass"] is not None]
    passed = sum(row["rf_pass"] for row in measured)
    text = (f"# AD9959 SPI-rate validation\n\n"
            f"- UTC: {report['utc']}\n- Mode: {report['mode']}\n- Status: {report['status']}\n"
            f"- SPI requested/actual: {report['spi_requested_Hz']:g} / {report['spi_actual_Hz']:g} Hz\n"
            f"- SCLK source: {report.get('spi_clock_source', 'qout')}\n"
            f"- Serial width: {report.get('serial_bits', 1)} bits\n"
            f"- Pre/post guard ticks: {report.get('pre_guard_ticks', 2)} / {report.get('post_guard_ticks', 6)}\n"
            f"- UPDATE high/low ticks: {report.get('update_pulse_ticks', 2)} each\n"
            f"- Update interval: {report['update_interval_s'] * 1e6:.6f} us\n"
            f"- Within-burst rate: {report['update_rate_Hz']:.6f} updates/s\n"
            f"- Full burst: {report.get('burst_updates', report['points'])} updates, {report['burst_duration_s'] * 1e6:.6f} us\n"
            f"- Completed waveforms: {len(rows)}; measured RF endpoints passing: {passed}/{len(measured)}\n"
            f"- Waveforms with raw-capture overflow: {sum(row['capture_overflow'] for row in rows)}\n"
            f"- Notes: {report['note'] or '(none supplied)'}\n\n"
            "Hardware CRC checks cover the programmed QOUT waveform. Forwarded SCLK is outside\n"
            "that waveform and its CRC checks. Raw FIFO capture is not used\n"
            "for high-rate bursts; its overflow is recorded separately. RF endpoint measurements\n"
            "do not establish correctness of every intermediate update. Time-resolved RF and\n"
            "DDS-connector signal-integrity measurements remain manual validation steps.\n")
    if "error" in report:
        text += f"\nError: {report['error']}\n"
    (directory / "report.md").write_text(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("slow", "burst", "endpoints"), default="slow")
    parser.add_argument("--clock-source", choices=("qout", "streamer"), default="qout",
                        help="SCLK wiring: Q0 (default) or J8 pin 3 STREAMER_CLK")
    parser.add_argument("--spi-clock", type=float, help="SCLK Hz (default: 10e6 for qout, fixed 100e6 for streamer)")
    parser.add_argument("--serial-bits", type=int, choices=(1, 4), default=1, help="serial data width (default: 1)")
    parser.add_argument("--pre-guard-ticks", type=int, default=2, help="forwarded-clock deselected guard before write, 10 ns ticks")
    parser.add_argument("--post-guard-ticks", type=int, default=6, help="forwarded-clock deselected guard before UPDATE, 10 ns ticks")
    parser.add_argument("--update-ticks", type=int, default=2, help="UPDATE high and low durations, 10 ns ticks each")
    parser.add_argument("--burst-cycles", type=int, default=1, help="full passes in a burst; endpoints prepend cycles-1 passes")
    parser.add_argument("--shuffle-seed", type=int, help="deterministically shuffle the frequency list")
    parser.add_argument("--start", type=float, default=100e6)
    parser.add_argument("--stop", type=float, default=101e6)
    parser.add_argument("--points", type=int, default=100, help="including both endpoints")
    parser.add_argument("--trials", type=int, default=1, help="repeat the selected test (default: 1)")
    parser.add_argument("--interval", type=float, default=1, help="minimum seconds between playbacks")
    parser.add_argument("--tolerance", type=float, default=5000, help="RF endpoint tolerance Hz (default: 5000)")
    parser.add_argument("--board", default="root@192.168.3.47")
    parser.add_argument("--analyzer", default="192.168.3.55")
    parser.add_argument("--run-dir", type=Path, help="new directory for sequences, logs and results")
    parser.add_argument("--note", default="", help="wiring/probe notes stored with results")
    parser.add_argument("--generate-only", action="store_true", help="generate artifacts without contacting hardware")
    args = parser.parse_args()
    if args.spi_clock is None:
        args.spi_clock = CLOCK_HZ if args.clock_source == "streamer" else 10e6
    try:
        values = frequencies(args.start, args.stop, args.points)
        waveform_args = (args.spi_clock, args.clock_source, args.serial_bits,
                         args.pre_guard_ticks, args.post_guard_ticks, args.update_ticks)
        timing = Ad9959Waveform(*waveform_args)
        if args.burst_cycles < 1 or (args.mode == "slow" and args.burst_cycles != 1):
            raise ValueError("Burst cycles must be positive and must be 1 in slow mode")
        if args.points * args.burst_cycles * timing.update_ticks > 500000000 - 2:
            raise ValueError("A waveform exceeds the helper's 5-second limit")
        if args.shuffle_seed is not None:
            random.Random(args.shuffle_seed).shuffle(values)
        if args.trials < 1 or not math.isfinite(args.interval) or args.interval < 0:
            raise ValueError("Require positive trials and finite, nonnegative interval")
        if not math.isfinite(args.tolerance) or not 0 < args.tolerance < (args.stop - args.start) / (args.points - 1) / 2:
            raise ValueError("Tolerance must be positive and less than half the frequency step")
    except ValueError as error:
        parser.error(str(error))
    if not args.generate_only:
        ensure_idle()
    if args.run_dir:
        directory = args.run_dir.resolve()
        directory.mkdir(parents=True, exist_ok=False)
    else:
        root = Path(__file__).resolve().parent / "results"
        root.mkdir(exist_ok=True)
        directory = Path(tempfile.mkdtemp(prefix=time.strftime("spi-rate-%Y%m%dT%H%M%S-", time.gmtime()), dir=root))
    interval = timing.update_ticks / CLOCK_HZ
    report = {
        "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "mode": args.mode,
        "status": "generated", "spi_requested_Hz": args.spi_clock, "spi_actual_Hz": timing.clock,
        "spi_clock_source": args.clock_source,
        "serial_bits": args.serial_bits, "pre_guard_ticks": args.pre_guard_ticks,
        "post_guard_ticks": args.post_guard_ticks, "update_pulse_ticks": args.update_ticks,
        "burst_cycles": args.burst_cycles, "burst_updates": args.points * args.burst_cycles,
        "shuffle_seed": args.shuffle_seed, "frequency_order_Hz": values,
        "update_interval_s": interval, "update_rate_Hz": 1 / interval,
        "burst_duration_s": args.points * args.burst_cycles * interval, "points": args.points,
        "start_Hz": args.start, "stop_Hz": args.stop, "trials": args.trials,
        "interval_s": args.interval, "tolerance_Hz": args.tolerance,
        "board": args.board, "analyzer": args.analyzer, "note": args.note,
        "amplitude_code": 1023, "streamer_Hz": CLOCK_HZ, "dds_clock_Hz": DDS_CLOCK_HZ,
        "measurements": [],
    }
    setup = Ad9959Waveform(*waveform_args)
    setup.initialize()
    setup_info = setup.save(directory / "setup.seq")
    sequences = []
    for index in range(args.points if args.mode != "burst" else 1):
        selected = (values * args.burst_cycles if args.mode == "burst" else
                    values * (args.burst_cycles - 1) + values[:index + 1] if args.mode == "endpoints" else [values[index]])
        waveform = Ad9959Waveform(*waveform_args)
        waveform.sweep(selected)
        info = waveform.save(directory / f"point-{index:04d}.seq")
        if info["ticks"] > 500000000:
            parser.error("A waveform exceeds the helper's 5-second limit")
        info.update(updates=len(selected), endpoint_Hz=selected[-1])
        sequences.append(info)
    report["setup"] = setup_info
    report["sequences"] = sequences
    source = Path(__file__).resolve().with_name("sequence_dma.cc")
    (directory / source.name).write_bytes(source.read_bytes())
    (directory / Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    compact_source = Path(__file__).with_name("sequence_compact.py")
    (directory / compact_source.name).write_bytes(compact_source.read_bytes())
    save_report(directory, report)
    print(f"Artifacts: {directory}\nSPI: {args.serial_bits}-bit, source {args.clock_source}, requested {args.spi_clock:g} Hz, actual {timing.clock:g} Hz\n"
          f"Update interval {interval * 1e6:.6f} us; rate {1 / interval:.3f}/s; "
          f"{report['burst_updates']}-update burst {report['burst_duration_s'] * 1e6:.3f} us", flush=True)
    if args.generate_only:
        return
    report["status"] = "running"
    try:
        with tempfile.TemporaryDirectory(prefix="spi-rate-ssh-") as temporary, \
                Board(args.board, Path(temporary)) as board:
            board.upload([directory / source.name, directory / setup_info["file"],
                          *(directory / item["file"] for item in sequences)])
            report["remote_directory"] = board.directory
            board.build(directory / "build.log")
            report["setup_capture_overflow"] = board.play_checked(setup_info, directory / "setup.log")
            analyzer = Analyzer(args.analyzer, args.start, args.stop, args.tolerance)
            # Burst capture uses a scope; do not reconfigure an analyzer set up manually for it.
            if args.mode != "burst":
                analyzer.__enter__()
            try:
                if args.mode != "burst":
                    analyzer.configure()
                    report["analyzer_identity"] = analyzer.identity
                    report["analyzer_settings"] = dict(start_Hz=analyzer.START_HZ, stop_Hz=analyzer.STOP_HZ,
                                                       RBW_Hz=analyzer.RBW_HZ, VBW_Hz=analyzer.VBW_HZ)
                save_report(directory, report)
                with (directory / "measurements.csv").open("w", newline="") as output:
                    fields = ["trial", "index", "updates", "set_Hz", "measured_Hz", "level_dBm", "error_Hz",
                              "rf_pass", "capture_overflow", "log"]
                    writer = csv.DictWriter(output, fieldnames=fields)
                    writer.writeheader()
                    for trial in range(args.trials):
                        for index, info in enumerate(sequences):
                            started = time.monotonic()
                            log = directory / f"trial-{trial:03d}-point-{index:04d}.log"
                            overflow = board.play_checked(info, log)
                            measured, level = analyzer.peak(info["endpoint_Hz"]) if args.mode != "burst" else (math.nan, math.nan)
                            valid = math.isfinite(measured)
                            row = dict(trial=trial + 1, index=index + 1, updates=info["updates"],
                                       set_Hz=info["endpoint_Hz"], measured_Hz=measured if valid else None,
                                       level_dBm=level if math.isfinite(level) else None,
                                       error_Hz=measured - info["endpoint_Hz"] if valid else None,
                                       rf_pass=valid if args.mode != "burst" else None,
                                       capture_overflow=overflow, log=log.name)
                            writer.writerow(row)
                            output.flush()
                            report["measurements"].append(row)
                            save_report(directory, report)
                            print(f"Trial {trial + 1}, point {index + 1}/{len(sequences)}: "
                                  f"{info['updates']} updates; digital CRC PASS; "
                                  f"RF {'PASS' if valid else 'FAIL' if args.mode != 'burst' else 'not measured'}; "
                                  f"peak={measured / 1e6:.6f} MHz; capture_overflow={overflow}", flush=True)
                            time.sleep(max(0, args.interval - (time.monotonic() - started)))
            finally:
                if args.mode != "burst":
                    analyzer.__exit__(None, None, None)
        report["status"] = "rf_endpoint_fail" if any(row["rf_pass"] is False for row in report["measurements"]) else "completed"
    except BaseException as error:
        report["status"] = "interrupted" if isinstance(error, KeyboardInterrupt) else "failed"
        report["error"] = str(error) or type(error).__name__
        raise
    finally:
        save_report(directory, report)
    print(f"{report['status']}: {directory / 'report.md'}\nCH1 holds the last tested frequency at full amplitude.")
    if report["status"] != "completed":
        raise SystemExit(1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        raise SystemExit("Interrupted; completed measurements and logs are preserved.")
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        raise SystemExit(f"spi_update_rate: {error}")
