#!/usr/bin/env python3
"""Sweep AD9959 CH1 and record fresh SVA1015X peak frequencies (Hz)."""

import argparse
import math
import re
import shlex
import socket
import subprocess
import tempfile
import time
from pathlib import Path


def run_command(arguments, timeout=15):
    result = subprocess.run(arguments, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {shlex.join(map(str, arguments))}\n"
                           + result.stdout)
    return result.stdout


def sweep_points(count):
    if count < 2:
        raise ValueError("At least two points are required")
    return [(200e6 * index / (count - 1), round(1023 * (1 - 0.9 * index / (count - 1))))
            for index in range(count)]


class PulsePins:
    """Short, checked per-point playbacks through the board's existing ABI 5 tools."""

    def __init__(self, board, directory):
        self.board = board
        self.options = ["-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
                        "-o", "ControlMaster=auto", "-o", "ControlPersist=120",
                        "-o", f"ControlPath={directory / 'ssh-control'}"]

    def remote(self, command):
        return run_command(["ssh", *self.options, self.board, command])

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        subprocess.run(["ssh", *self.options, "-O", "exit", self.board],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)

    def upload(self, files):
        self.directory = self.remote("mktemp -d /home/root/sweep1.XXXXXX").strip().splitlines()[-1]
        if not re.fullmatch(r"/home/root/sweep1\.[A-Za-z0-9]+", self.directory):
            raise RuntimeError("Unexpected remote temporary directory: " + self.directory)
        run_command(["scp", *self.options, *map(str, files), f"{self.board}:{self.directory}/"], timeout=60)

    def play(self, filename):
        sequence = shlex.quote(f"{self.directory}/{filename}")
        log = shlex.quote(f"{self.directory}/{filename}.log")
        command = (
            "env -u PP_RANDOM_FINAL -u PP_IGNORE_QOUT_FINAL -u PP_IGNORE_RB_ERROR_IF_CRC_OK "
            f"timeout 10s /home/root/ppplay -file {sequence} -format text -i 0x2 -force "
            f"-check -timeout 2 -hard-timeout 5s > {log} 2>&1\n"
            f"status=$?\ncat {log}\nexit \"$status\""
        )
        report = self.remote(command)
        if ("C Checker report: 0 errors" not in report
                or "Size difference=0 Length difference=0" not in report):
            raise RuntimeError("Missing successful PulsePins waveform check:\n" + report)
        if not re.search(r"freq_meter:\s+streamer_clk\s+100\.000 MHz", report):
            raise RuntimeError("Expected a 100 MHz PulsePins streamer:\n" + report)
        return report


class Siglent:
    SPAN_HZ = 200000
    RBW_HZ = 1000
    VBW_HZ = 3000

    def __init__(self, host):
        self.host = host

    def __enter__(self):
        self.connection = socket.create_connection((self.host, 5025), timeout=5)
        self.connection.settimeout(5)
        self.stream = self.connection.makefile("rb")
        return self

    def __exit__(self, *exc):
        try:
            self.write(":INIT:CONT ON")
            self.write(":INIT:IMM")
        except OSError:
            pass
        finally:
            self.stream.close()
            self.connection.close()

    def write(self, command):
        self.connection.sendall((command + "\n").encode("ascii"))

    def query(self, command):
        self.write(command)
        response = self.stream.readline(8192)
        if not response.endswith(b"\n"):
            raise RuntimeError(f"Incomplete Siglent response to {command}: {response!r}")
        return response.decode("ascii").strip()

    def complete(self):
        if self.query("*OPC?") != "1":
            raise RuntimeError("Siglent did not acknowledge completion")

    def check_errors(self):
        status = int(self.query("*ESR?"))
        if status & 0x3C:
            raise RuntimeError(f"Siglent SCPI error (event status {status})")

    def configure(self):
        self.identity = self.query("*IDN?")
        if "SVA1015X" not in self.identity:
            raise RuntimeError("Expected an SVA1015X, received " + self.identity)
        for command in (
            "*CLS", ":INST SA", ":INIT:CONT OFF", ":ABOR", ":OUTP OFF",
            f":FREQ:SPAN {self.SPAN_HZ}", ":FREQ:CENT 75000000",
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
        for query, expected in (
            (":FREQ:SPAN?", self.SPAN_HZ), (":BWID?", self.RBW_HZ),
            (":BWID:VID?", self.VBW_HZ), (":INIT:CONT?", 0), (":SWE:COUN?", 1),
            (":POW:ATT?", 20), (":POW:GAIN?", 0),
        ):
            if abs(float(self.query(query)) - expected) > 0.01:
                raise RuntimeError("Siglent setting verification failed: " + query)
        self.check_errors()
        return float(self.query(":SWE:TIME?"))

    def peak(self, frequency):
        # Retune while idle, then acquire a NEW trace with the DDS held fixed.
        center = max(frequency, self.SPAN_HZ / 2 + 1)
        self.write(":ABOR")
        self.write(f":FREQ:CENT {center:.9f}")
        self.complete()
        if abs(float(self.query(":FREQ:CENT?")) - center) > 1:
            raise RuntimeError("Siglent center frequency did not follow the setpoint")
        scan_time = float(self.query(":SWE:TIME?"))
        if not math.isfinite(scan_time) or scan_time <= 0:
            raise RuntimeError("Invalid Siglent sweep time")
        for attempt in range(3):
            self.write(":INIT:IMM")
            self.complete()
            # This firmware occasionally acknowledges before the trace/marker
            # display is ready (X=Y=0). Allow publication time and retry a fresh
            # scan at this SAME DDS setting if the marker is still invalid.
            time.sleep(scan_time + 0.02)
            self.write(":CALC:MARK1:MAX")
            self.complete()
            measured = float(self.query(":CALC:MARK1:X?"))
            level = float(self.query(":CALC:MARK1:Y?"))
            self.check_errors()
            if frequency == 0:  # DC is not an RF carrier.
                return math.nan, level
            if (math.isfinite(measured) and math.isfinite(level)
                    and level >= -80 and abs(measured - frequency) <= 0.45 * self.SPAN_HZ):
                return measured, level
        return math.nan, level


def acquire(board, analyzer, points, duration, output):
    dwell = duration / len(points)
    started = time.monotonic()
    valid = 0
    for index, (frequency, amplitude) in enumerate(points):
        step_started = time.monotonic()
        board.play(f"point-{index:04d}.seq")
        measured, level = analyzer.peak(frequency)
        output.write(f"{frequency:.9f} {measured:.9f}\n")
        output.flush()
        valid += math.isfinite(measured)
        print(f"{index + 1:3d}/{len(points)}  set={frequency / 1e6:10.6f} MHz  "
              f"peak={measured / 1e6:10.6f} MHz  amplitude={amplitude / 1023:.3f}  "
              f"level={level:.1f} dBm", flush=True)
        # No catch-up: a slow measurement extends this dwell and the overall run.
        time.sleep(max(0, dwell - (time.monotonic() - step_started)))
    elapsed = time.monotonic() - started
    output.write(f"# completed_points={len(points)} valid_rf_peaks={valid} elapsed_s={elapsed:.6f}\n")
    return elapsed, valid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board", default="root@192.168.3.47", help="PulsePins SSH destination")
    parser.add_argument("--analyzer", default="192.168.3.55", help="Siglent LAN address (TCP 5025)")
    parser.add_argument("--points", type=int, default=100, help="points including both endpoints (default: 100)")
    parser.add_argument("--duration", type=float, default=60, help="nominal sweep seconds (default: 60)")
    parser.add_argument("--output", type=Path, default=Path("sweep1.dat"), help="ASCII result file (default: sweep1.dat)")
    args = parser.parse_args()
    if args.points < 2 or not math.isfinite(args.duration) or args.duration <= 0:
        parser.error("--points must be at least 2 and --duration must be finite and positive")
    generator = Path(__file__).resolve().with_name("spi_ad9959")
    if not generator.is_file():
        parser.error(f"Build the generator first: make -C {generator.parent} spi_ad9959")
    points = sweep_points(args.points)
    with tempfile.TemporaryDirectory(prefix="sweep1-") as temporary:
        directory = Path(temporary)
        files = [directory / "setup.seq"]
        run_command([str(generator), str(files[0])])
        for index, (frequency, amplitude) in enumerate(points):
            path = directory / f"point-{index:04d}.seq"
            run_command([str(generator), "--set-ch1", str(path), f"{frequency:.9f}", str(amplitude)])
            files.append(path)
        with PulsePins(args.board, directory) as board, Siglent(args.analyzer) as analyzer:
            board.upload(files)
            board.play("setup.seq")
            scan_time = analyzer.configure()
            print(f"{analyzer.identity}\nSingle scan: {scan_time:.3f} s; "
                  f"nominal point interval: {args.duration / args.points:.3f} s.\n"
                  f"Sequences and playback logs: {args.board}:{board.directory}\n"
                  f"Starting {args.points} points, 0-200 MHz, amplitude 100%-10%.", flush=True)
            with args.output.open("w", encoding="ascii") as output:
                output.write("# set_frequency_Hz measured_frequency_Hz\n"
                             f"# UTC {time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}\n"
                             f"# {analyzer.identity}\n"
                             f"# CH1 points={args.points} nominal_duration_s={args.duration:g}\n"
                             "# Linear digital amplitude 1023 -> 102; 500 MHz DDS clock, 100 MHz streamer\n"
                             f"# span_Hz={analyzer.SPAN_HZ} RBW_Hz={analyzer.RBW_HZ} VBW_Hz={analyzer.VBW_HZ}\n"
                             "# nan: DC, peak below -80 dBm, or no valid peak inside the tracking window\n")
                elapsed, valid = acquire(board, analyzer, points, args.duration, output)
            print(f"Saved {args.points} pairs to {args.output.resolve()} in {elapsed:.2f} s "
                  f"({valid} valid RF peaks; DC is nan).\nCH1 remains at 200 MHz, amplitude code 102/1023.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        raise SystemExit("Sweep interrupted; any completed rows are saved. CH1 holds its last setting.")
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        raise SystemExit(f"sweep1: {error}")
