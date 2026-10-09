# RTL test runners

The `run*` and `compile*` scripts compile **and run** their bench through
`run.py`. Python 3.8+ and the selected simulator must be on `PATH`.
Make targets keep their existing names (`test`, `test1`, etc.).

```sh
make -C ip/streamer/tb_streamer -j4
make -C ip test-icarus -j4
SEED=42 make -C ip/streamer/tb_st_interface test3
SEED=random make -C ip/combiner/tb_combiner test2
VERBOSE=1 WAVES=1 make -C ip/misc/tb_cdc test
```

## Artifacts and parallel runs

Every invocation has a fresh directory beneath
`<bench>/.sim/<simulator>/<runner>/seed-<seed>-<unique suffix>/`.
Executables, Questa `work/`, coverage, waveforms, logs, and `SUCCESS` markers
stay inside that directory. Separate invocations of the same bench are also
isolated. A stale marker in the source directory cannot make a run pass.
Questa still needs the existing vendor library mappings for vendor-IP benches.

The launcher checks compiler/simulator exit status and error diagnostics, and
requires a fresh success marker for benches that create one. Legacy Icarus
benches without markers explicitly use `--no-marker`; their fatal assertions
and watchdogs determine the result. All benches must also report required
scenario coverage. `TIMEOUT_SEC` (default 180) bounds each
tool invocation in wall-clock seconds, including benches with stopped clocks.

## Seeds, logs, and waveforms

* `SEED` defaults to **1**. An integer from 1 to 2147483647 replays the same
  simulator's stimulus. `SEED=random` chooses and prints a concrete seed.
  Questa uses `-sv_seed`; Icarus seeds its shared `$urandom` generator before
  clocked stimulus. Cross-simulator random streams need not be identical.
* Every run prints its artifact directory and a ready-to-run replay command,
  including sources, compile defines, libraries, and the timeout setting.
  `run.json` records the seed, simulator, source paths, result, and elapsed time.
* Normal output contains run/pass/fail summaries. `run.log` keeps the last 100
  lines of each tool stage and any earlier error diagnostics. On failure that
  context is also printed. `VERBOSE=1` retains and prints the complete trace.
* Waveforms are opt-in: `WAVES=1` enables Icarus VCD or Questa WLF capture.
  They are written inside the run directory, never beside the source files.
* `SIM=icarus` or `SIM=questa` overrides a script's default simulator. This
  does not make vendor-dependent RTL or unsupported syntax portable.
* `IVERILOG`, `VVP`, `VLIB`, `VLOG`, and `VSIM` can select tool executables.

Use each test directory's `make clean` to remove its artifacts. Do not run
`clean` concurrently with tests in that directory.
Coverage merge/report targets aggregate the retained Questa runs. Use
`make clean` before collecting coverage for a different RTL revision.

## Required scenario coverage

Each bench reports the scenarios it actually exercised before declaring success.
These lightweight counters complement functional assertions and scoreboards:
correct outputs cannot compensate for stimulus that never reached an intended
stall, reset-in-flight, overflow, mode, or boundary case.

Include `scenario_coverage.svh` inside the bench module and call its task for
every required bin, before writing `SUCCESS` or calling `$finish`:

```systemverilog
tb_require_scenario("write_backpressure", blocked_cycles);
tb_require_scenario("checked_words", received_count, 65);
```

The default minimum is 1. Counts must come from sampled DUT activity or
completed functional checks; setting a configuration bit alone does not prove
that its datapath was exercised. Device-specific observers live beside their
benches. Requirements are chosen per bench, and retain coverage across reset
epochs when the bench verifies recovery.

The task emits `SCENARIO <unique_name> <observed_count> <positive_minimum>` and
fatals on an unmet or unknown count. Names contain no whitespace and must be
unique within a run. The runner independently rejects missing reports, duplicate
names, or unmet/nonpositive thresholds, including in `--no-marker` mode. A passing
run prints `[COVERAGE] ...: N/N required scenarios`; `run.json` retains every
reported bin under `scenario_coverage`, with `observed`, `required`, and `met`
fields, even if its line has aged out of the bounded log. A failure may contain
only the bins reached before the first fatal; that is not a complete coverage
report. This is scenario coverage, separate from Questa's code-coverage database.

## Portable CI coverage

`make -C ip test-icarus` runs 48 benches: counter, timestamp and miscellaneous
tests; all nine combiner benches; the frequency meter and streaming mux;
CDC helpers; exhaustive single-stage triggering; standalone decoder tests 13/14;
and the preprocessor reference-model test 5. GitHub Actions runs these on Ubuntu
24.04 with four parallel jobs, checks required-scenario failures and the trigger
oracle against six RTL mutants, and uploads diagnostics on failure. The full
mixed-simulator target runs 99 benches.

CRC was assessed but remains on Questa: Icarus does not support the DUT's
unpacked array of CRC-mask parameters. FIFO-based streamer/readback tests need
vendor models; the older preprocessor benches use event-filter syntax unsupported
by Icarus. The Icarus target selects only supported benches rather than
substituting behavioral DUT models.

Run launcher regression checks with:

```sh
python3 -m unittest discover -s ip/testbench -p 'test_*.py'
```

These use small real RTL fixtures to check parallel isolation, seed replay,
stale/missing markers, fatal and nonfatal errors, wall-clock timeouts,
scenario-report enforcement, and quiet/verbose/waveform modes. The omission
regressions also remove stimulus from real benches while keeping the DUT healthy:
constant correlation input and idle-only decoder resets run on Icarus; missing
FIFO backpressure, CRC gaps, and effective combiner forcing run on Questa.
Each must fail its named scenario after earlier functional checks complete.
Icarus is required; Questa checks run when it is available.
