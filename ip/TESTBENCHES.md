# IP testbenches

This file is the entry point for understanding the simulation testbench tree under `ip/`.

## What these testbenches are for

The testbenches in `ip/` mix several styles of verification:

* direct functional checks of datapath behavior
* Avalon-MM / Avalon-ST wrapper and register-interface checks
* integration tests across multiple internal blocks
* randomized sanity loops for repeated register-programmed scenarios
* timing/latency-sensitive checks where cycle alignment matters

The goal is not only to catch regressions, but also to explain what each subsystem promises at its external interface.

## Common conventions

Most testbenches follow these conventions:

* generate a simple local simulation clock and short reset pulse
* print `SUCCESS` and create a `SUCCESS` marker file on pass
* write a `.ucdb` coverage file with a subsystem-specific name
* use `$strobe` on selected DUT internals so failures can be diagnosed without immediately opening a waveform viewer

The comments added to each testbench focus on purpose and scope rather than on every signal assignment.

## How to read the tree

### Streamer family

The streamer tree is the core of PulsePins and is split by verification layer:

* `streamer/tb_streamer/` - top-level streamer datapath and feature integration
* `streamer/tb_st_interface/` - Avalon-ST / Avalon-MM wrapper behavior
* `streamer/tb_input_fifo/` - ingress FIFO behavior and backpressure
* `streamer/tb_preprocessor/` - store/replay preprocessor logic in isolation
* `streamer/tb_input_fifo+preprocessor/` - combined ingress + preprocessor path
* `streamer/tb_chain_trigger/` - multi-stage trigger-program engine
* `streamer/tb_and_trigger/` - simpler trigger primitive

### Readback

* `rl_encoder_if/tb_rl_encoder/` - core run-length encoder behavior
* `rl_encoder_if/tb_rl_encoder_if/` - software-visible wrapper/interface behavior

### Counter subsystem

Each `counter/tb_*` directory targets one measurement primitive or the top-level counter wrapper.

### Other IPs

* `combiner*` - qout and trigger combiner datapaths and wrappers
* `st_mux/` - Avalon-ST multiplexer behavior
* `ts_core/` - timestamp capture core
* `freq_meter/` - frequency meter
* `misc/` - small reusable support blocks

## Suggested reading order

For the main PulsePins data path, start with:

1. `streamer/tb_streamer/`
2. `streamer/tb_st_interface/`
3. `streamer/tb_input_fifo+preprocessor/`
4. `streamer/tb_chain_trigger/`
5. `rl_encoder_if/tb_rl_encoder_if/`
6. `counter/tb_counter_if/`

That sequence follows the same path a host-generated sequence takes through the hardware and back through the observability path.

