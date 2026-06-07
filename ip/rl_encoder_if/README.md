PulsePins readback / run-length encoder subsystem.

This directory contains the RTL used to observe `qout` activity, compress it back into run-length encoded elements, and expose that captured stream to software for verification and debugging.

## Main files

- `rl_encoder_if.sv` - Avalon-ST/Avalon-MM wrapper around the readback encoder core
- `rl_encoder.sv` - core run-length encoder plus dual-clock FIFO buffering
- `rl_config.vh` - shared widths and configuration constants

## Architecture overview

The readback path is the mirror image of the streamer datapath:

1. sampled output symbols arrive on `qin`
2. normal builds sample valid input words on `qin_clk`; the strobe-clocked policy is dormant behind `WEIRD_CLOCK`
3. `rl_encoder.sv` groups consecutive equal values into `{count, value}` runs
4. a dual-clock FIFO decouples the sampled input domain from software-side reads
5. `rl_encoder_if.sv` exports the captured runs as Avalon-ST data and adds status/control registers

This subsystem is mainly used for:

- self-test of streamer output correctness
- CRC-based sanity checking
- debugging external signals when output enable is disabled

## Programming model

`rl_encoder_if.sv` exposes:

- Avalon-ST output carrying encoded `{control=0, counter, data}` elements
- Avalon-MM control/status registers for reset, active-mode status, pulse count, FIFO-empty state, overflow, and CRC32

The wrapper also maintains a pulse counter and CRC in the sampled-input domain so software can compare the observed stream against the transmitted reference.

## Reading order for maintainers

1. `rl_encoder_if.sv`
2. `rl_encoder.sv`
3. `rl_config.vh`
4. `c++/readback.hh`
5. `docs/docs/readback.md`

## Verification and related docs

- subsystem overview: `docs/docs/readback.md`
- related streamer docs: `docs/docs/streamer.md`
- host-side wrapper: `c++/readback.hh`
