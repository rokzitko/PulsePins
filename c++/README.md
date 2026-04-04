PulsePins host-side software lives in `c++/`.

This directory contains the ARM-side C++ code that configures the FPGA fabric, streams pulse programs, runs self-tests, exposes measurement blocks, and provides the command-line tools shipped as `pptool` and its symlink-based modes.

## What lives here

- `pptool.cc` - main executable entry point and symlink-based command dispatcher
- `pptool_commands.hh` - catalog of supported `pp...` command handlers
- `pptool_streaming.cc` - commands that primarily drive the streamer datapath
- `pptool_measurement.cc` - commands for readback, counters, timestamps, temperature, and frequency measurement
- `host_runtime.hh` - shared bootstrap/runtime object used by the main host-side executables
- `fpga.hh` - top-level ARM-side ownership of memory maps, PLL helpers, trigger monitors, and output-enable GPIO
- `startup.hh` - common process bootstrap and FPGA startup policy
- `options.hh` - typed option-resolution helpers shared by startup, trigger, streamer, and measurement code
- `ppworkflow.hh` - shared send/trigger/readback/check workflow used by several commands
- `elements.hh` and `sequence.hh` - host-side representation of pulse programs and trigger elements, including text/VCD import and deterministic waveform export to VCD
- `streamer_control.hh`, `streamer_fifo.hh`, `streamer_dma.hh`, `basic_multi_dma.hh` - host-side streamer lifecycle control and transport/topology helpers
- `streamer*.hh`, `readback.hh`, `counter.hh`, `timestamp.hh`, `freq_meter.hh` - typed wrappers around major FPGA subsystems

## Host-side architecture

At a high level the control flow is:

1. `main()` in `pptool.cc` builds a `HostRuntime`, which parses options, applies shared process/bootstrap policy, constructs the single `FPGA` object, and performs the startup frequency-meter report.
2. `startup.hh` applies clock-selection and PLL policy before command execution begins.
3. The executable name (`pptool`, `ppfg`, `ppcounter`, and so on) selects a command handler from the dispatch table in `pptool.cc`.
4. Command handlers construct typed subsystem wrappers such as `streamer`, `readback`, `counter`, `timestamp`, or `freq_meter`.
5. Streaming-oriented commands typically build a `Sequence`, transmit it through a transport (`streamer_fifo` or DMA-based transport), then use `send_and_trig(...)` from `ppworkflow.hh` to coordinate triggering, completion checks, CRC verification, and optional readback validation.

This split is intentional:

- `host_runtime.hh` owns the shared executable bootstrap and startup-report policy.
- `pptool*.cc` files define user-facing behavior and option handling.
- wrapper headers expose hardware blocks as typed C++ interfaces.
- sequence classes model the programmable pulse stream in a way that can be reused by CLI tools, tests, and future bindings.

## Main extension points

If you want to customize or extend the host software, the usual starting points are:

- add a new CLI mode: implement a new `pp...` function and register it in the dispatch table in `pptool.cc`
- add a new sequence construct: extend `elements.hh` and `sequence.hh`, then update the relevant command or parser path
- change common streamer execution behavior: update `send_and_trig(...)` in `ppworkflow.hh`
- change how sequences reach hardware: update `streamer_fifo.hh`, `streamer_dma.hh`, and `basic_multi_dma.hh`
- change default startup behavior: update `apply_fpga_startup_policy(...)` in `startup.hh`
- change clock-selection or PLL option semantics: update `options.hh` and `pll_rules.hh`
- expose a new hardware block: add a typed wrapper header, then call it from a command handler or higher-level API

The project aims to keep high-level interfaces stable, so the preferred pattern is to extend behind existing command names and wrapper types rather than renaming public entry points.

## Reading order for maintainers

For a first pass through the codebase, read in this order:

1. `pptool.cc`
2. `pptool_commands.hh`
3. `host_runtime.hh`
4. `pptool_streaming.cc` and `pptool_measurement.cc`
5. `fpga.hh` and `startup.hh`
6. `options.hh` and `pll_rules.hh`
7. `ppworkflow.hh`
8. `streamer_control.hh`, `streamer_fifo.hh`, `streamer_dma.hh`, and `basic_multi_dma.hh`
9. `elements.hh` and `sequence.hh`
10. subsystem wrappers such as `streamer.hh`, `readback.hh`, `counter.hh`, `timestamp.hh`, and `freq_meter.hh`

That order mirrors the path a user command takes from CLI invocation down to FPGA-facing transactions.

## Verification and related docs

- build host tools: `make -C c++ build`
- run unit tests: `make -C c++ unit_tests`
- check whitespace policy: `make -C c++ lint-whitespace`
- broader contributor workflow: `HACKING.md`
- C++ API overview: `docs/docs/cpp.md`
- CLI command overview: `docs/docs/pptool.md`
- hardware subsystem reference: `docs/docs/counter.md`, `docs/docs/freq_meter.md`, `docs/docs/timestamp.md`, `docs/docs/details.md`

## Whitespace policy

- C++ files use spaces only, never tabs.
- Indentation width is 2 spaces.
- Leading indentation must be divisible by 2.
- Lines use LF endings, no trailing whitespace, and end with a final newline.
- This policy intentionally does not enforce a single brace style or broad auto-formatting.
- `.editorconfig` is the editor-facing source of truth, and `make -C c++ lint-whitespace` checks compliance.
