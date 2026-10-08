"""Run one RTL bench with isolated artifacts, reproducible seeds and bounded logs."""

import argparse
from collections import deque
import glob
import json
import os
from pathlib import Path
import re
import secrets
import shlex
import signal
import subprocess
import sys
import tempfile
import threading
import time


ERROR = re.compile(r"\*\* (?:Error|Fatal):|\b(?:ERROR|FATAL):")


def flag(name):
    value = os.environ.get(name, "0")
    if value not in ("0", "1"):
        raise ValueError(f"{name} must be 0 or 1")
    return value == "1"


def run_command(command, directory, log, verbose, timeout):
    log.write("$ " + shlex.join(command) + "\n")
    log.flush()
    recent = deque(maxlen=100)
    errors = deque(maxlen=20)
    expired = threading.Event()
    process = subprocess.Popen(command, cwd=directory, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, errors="replace",
                               start_new_session=True)

    def terminate():
        expired.set()
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass

    timer = threading.Timer(timeout, terminate)
    timer.start()
    try:
        for line in process.stdout:
            recent.append(line)
            if ERROR.search(line):
                errors.append(line)
            if verbose:
                log.write(line)
                print(line, end="", flush=True)
        code = process.wait()
    except BaseException:
        terminate()
        process.wait()
        raise
    finally:
        timer.cancel()
        process.stdout.close()
    if not verbose:
        # Keep the end of each stage plus any earlier errors, rather than MB of
        # successful cycle traces. The full trace is available with VERBOSE=1.
        log.writelines(errors)
        log.writelines(recent)
    log.flush()
    if expired.is_set() or code or errors:
        if not verbose:
            sys.stderr.writelines(errors)
            sys.stderr.writelines(recent)
        if expired.is_set():
            raise RuntimeError(f"wall-clock timeout after {timeout:g}s: {command[0]}")
        raise RuntimeError(f"{command[0]} failed (exit {code})")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runner", type=Path, help="calling run*/compile* script")
    parser.add_argument("top", help="testbench top module")
    parser.add_argument("sources", nargs="+", help="paths/globs relative to the runner")
    parser.add_argument("--define", action="append", default=[])
    parser.add_argument("--library", action="append", default=[])
    parser.add_argument("--no-marker", action="store_true", help="bench uses exit status rather than SUCCESS")
    args = parser.parse_intermixed_args()
    script = args.runner.resolve()
    bench = script.parent
    sim = os.environ.get("SIM", "icarus" if script.name.startswith("compile") else "questa")
    if sim not in ("icarus", "questa"):
        raise ValueError("SIM must be icarus or questa")
    requested_seed = os.environ.get("SEED", "1")
    seed = secrets.randbelow(2147483647) + 1 if requested_seed == "random" else int(requested_seed)
    if not 1 <= seed <= 2147483647:
        raise ValueError("SEED must be 1..2147483647 or random")
    verbose, waves = flag("VERBOSE"), flag("WAVES")
    timeout = float(os.environ.get("TIMEOUT_SEC", "180"))
    if not 0 < timeout < float("inf"):
        raise ValueError("TIMEOUT_SEC must be positive and finite")
    sources = []
    for pattern in args.sources:
        matches = sorted(glob.glob(str(bench / pattern)))
        if not matches:
            raise ValueError(f"no source matches {pattern!r} in {bench}")
        sources.extend(str(Path(path).resolve()) for path in matches)

    root = bench / ".sim" / sim / script.name
    root.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix=f"seed-{seed}-", dir=root))
    replay_args = [sys.executable, str(Path(__file__).resolve()), str(script), args.top, *args.sources]
    for define in args.define:
        replay_args.extend(["--define", define])
    for library in args.library:
        replay_args.extend(["--library", library])
    if args.no_marker:
        replay_args.append("--no-marker")
    replay = f"SIM={sim} SEED={seed} VERBOSE=1 WAVES={int(waves)} TIMEOUT_SEC={timeout:g} " + shlex.join(replay_args)
    metadata = dict(simulator=sim, seed=seed, top=args.top, sources=sources,
                    defines=args.define, libraries=args.library, verbose=verbose,
                    waves=waves, timeout_seconds=timeout, replay=replay, status="running")
    metadata_path = directory / "run.json"
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n")
    label = f"{bench.parent.name}/{bench.name}/{script.name}"
    print(f"[RUN] {label} sim={sim} seed={seed} output={directory}", flush=True)
    print(f"[REPLAY] {replay}", flush=True)
    include_dirs = list(dict.fromkeys([str(bench), str(bench.parent),
                                      *(str(Path(source).parent) for source in sources)]))
    plusargs = [f"+seed={seed}"]
    if verbose:
        plusargs.append("+verbose")
    if waves:
        plusargs.append("+waves")
    started = time.monotonic()
    try:
        with (directory / "run.log").open("w") as log:
            def execute(command):
                run_command(command, directory, log, verbose, timeout)

            if sim == "icarus":
                runtime = str(Path(__file__).with_name("runtime.sv"))
                execute([os.environ.get("IVERILOG", "iverilog"), "-g2012", "-DIVERILOG",
                         "-s", args.top, "-s", "tb_runtime", "-o", "simulation",
                         *(f"-I{path}" for path in include_dirs),
                         *(f"-D{define}" for define in args.define), *sources, runtime])
                # Unbuffered output preserves the last diagnostic on timeout;
                # an unexpected $stop must fail rather than enter an interactive prompt.
                execute([os.environ.get("VVP", "vvp"), "-i", "-N", "simulation", *plusargs])
            else:
                execute([os.environ.get("VLIB", "vlib"), "work"])
                execute([os.environ.get("VLOG", "vlog"), "-sv", "+warn=all", "-lint", "-cover", "bcesft",
                         *(f"+incdir+{path}" for path in include_dirs),
                         *(f"+define+{define}" for define in args.define), *sources])
                commands = ["onerror {quit -code 1}", "onbreak {quit -code 1}",
                            "coverage save -onexit coverage.ucdb"]
                if waves:
                    commands.append("log -r /*")
                commands.append("run -all")
                execute([os.environ.get("VSIM", "vsim"), "-c", "-onfinish", "exit",
                         "-sv_seed", str(seed), "-coverage", "-wlf", "waves.wlf",
                         "-l", "transcript" if verbose else os.devnull,
                         *(item for library in args.library for item in ("-L", library)),
                         "-do", "; ".join(commands), args.top, *plusargs])
            if not args.no_marker and not (directory / "SUCCESS").is_file():
                raise RuntimeError("simulation exited without a SUCCESS marker")
        metadata["status"] = "passed"
        print(f"[PASS] {label} sim={sim} seed={seed}", flush=True)
        return 0
    except (OSError, RuntimeError) as error:
        metadata["status"] = "failed"
        metadata["error"] = str(error)
        print(f"[FAIL] {label}: {error}\nLog: {directory / 'run.log'}\nReplay: {replay}", file=sys.stderr)
        return 1
    finally:
        metadata["elapsed_seconds"] = round(time.monotonic() - started, 3)
        metadata_path.write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        sys.exit(str(error))
