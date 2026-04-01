PulsePins streamer subsystem.

This directory contains the RTL for the main pulse-sequence engine: it accepts encoded sequence elements on the control side, expands them into output updates, applies trigger and gating policy, and emits the final `qout` stream in the selected streaming clock domain.

## Main files

- `st_interface.sv` - Avalon-ST data ingress plus Avalon-MM control/status wrapper around the streamer core
- `streamer.sv` - top-level datapath glue joining FIFOs, decoder, trigger chain, and output flow control
- `config.vh` - shared widths, control-bit definitions, register enums, and FIFO sizing parameters
- `input_fifo.sv` - control-side FIFO that buffers incoming sequence elements
- `preprocessor.sv` - optional second-level replay/store stage for short subsequences
- `rl_decoder.sv` - run-length decoder that expands encoded elements into output updates
- `output_fifo.sv` - dual-clock FIFO bridging control-side decode logic into the `streamer_clk` output domain
- `and_trigger.sv` - simple single-stage mask/pattern trigger block
- `chain_trigger.sv` - multi-stage trigger program loader and runtime trigger state machine
- `prng.sv` - pseudorandom data source used by PRNG elements

## Architecture overview

The streaming path is intentionally split into two domains:

1. control-side logic in `clk`
2. output-side logic in `streamer_clk`

At a high level the flow is:

1. the host or DMA engine sends encoded `{control, counter, data}` elements into `st_interface.sv`
2. `st_interface.sv` byte-swaps them into the internal layout and forwards them to `streamer.sv`
3. `input_fifo.sv` absorbs bursty host traffic and provides backpressure through `asi_ready`
4. regular elements go to `rl_decoder.sv`, while trigger elements are consumed by `chain_trigger.sv`
5. decoder output is buffered by `output_fifo.sv`, which crosses into `streamer_clk`
6. `chain_trigger.sv` decides when output is allowed to advance
7. gating and stop logic further qualify output reads from the output FIFO
8. `qout`, `qout_valid`, and `qout_strobe` become the external streamer outputs

## Programming model

The external software-visible wrapper is `st_interface.sv`.

It exposes:

- an Avalon-ST ingress port carrying encoded sequence elements
- an Avalon-MM control/status port for trigger control, output override, gating, CRC readout, and FIFO statistics
- direct trigger and gate inputs used at runtime in the `streamer_clk` domain

Important behavioral points:

- regular sequence elements describe output updates and durations
- trigger elements are intercepted before decode and loaded into the trigger chain
- `initial_value` defines the observable output before the trigger fires
- `qout_select` can override the normal streamer output for debugging or manual output driving
- `gate_enable` controls whether the output-side FIFO is allowed to advance once triggering has started
- `done` reflects successful completion of the buffered output stream
- `buffer_error` indicates output-side underrun

## Key customization points

If you want to customize the streamer, the usual starting points are:

- register map or external control behavior - `st_interface.sv`
- encoded element semantics - `config.vh`, `rl_decoder.sv`, `preprocessor.sv`
- trigger behavior - `chain_trigger.sv`, `and_trigger.sv`
- output pacing, strobe timing, underrun behavior - `output_fifo.sv`
- buffering and throughput tradeoffs - `config.vh`, `input_fifo.sv`, `output_fifo.sv`

## Reading order for maintainers

1. `config.vh`
2. `st_interface.sv`
3. `streamer.sv`
4. `rl_decoder.sv`
5. `preprocessor.sv`
6. `output_fifo.sv`
7. `chain_trigger.sv`

That order follows the boundary from software-visible control surfaces toward the deeper implementation details.

## Verification and related docs

- run RTL tests: `make -C ip test`
- subsystem overview: `docs/docs/streamer.md`
- low-level hardware details: `docs/docs/details.md`
- CDC and clock ownership: `docs/docs/clock_domain.md`
- readback verification path: `docs/docs/readback.md`
