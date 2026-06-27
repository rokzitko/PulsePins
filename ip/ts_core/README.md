PulsePins timestamp capture subsystem.

This directory contains the low-rate event timestamping core used to record the clock-cycle position of selected PPS and auxiliary timing signals.

## Main files

- `ts_core.sv` - dual-path timestamp capture core with one Avalon-ST output per capture path

## Architecture overview

`ts_core.sv` is intentionally simple and synchronous:

1. a free-running counter advances in `clk`
2. each asynchronous input is synchronized through a short flip-flop chain
3. a rising edge on `sig` or `sigA` captures the current counter value
4. the captured value is presented on the corresponding Avalon-ST output
5. Avalon-ST `valid` remains asserted until the downstream FIFO accepts the timestamp
6. if another event arrives while the path already has a pending timestamp, the later event is dropped and the path's overflow latch/counter is updated

This design is therefore best suited to sparse timing events such as PPS pulses, trigger markers, and low-rate diagnostic signals.

## Programming model

The core exposes a small Avalon-MM status/control register file for pending and overflow reporting. Routing configuration happens outside the block through the routing PIO controlled by `c++/timestamp.hh`, which chooses:

- whether PPS comes from the external input or the crystal-derived source
- which source is routed to the auxiliary `sigA` capture path

The captured timestamps are emitted as raw counter values on two separate Avalon-ST streams.

Register map, word offsets:

- `0`: status, read-only; bit 0 = PPS pending, bit 1 = sigA pending, bit 8 = PPS overflow, bit 9 = sigA overflow
- `1`: control, write-only; bit 0 clears PPS overflow latch/count, bit 1 clears sigA overflow latch/count
- `2`: PPS overflow count, read-only saturating 32-bit counter
- `3`: sigA overflow count, read-only saturating 32-bit counter

## Reading order for maintainers

1. `ts_core.sv`
2. `c++/timestamp.hh`
3. `docs/docs/timestamp.md`
4. `docs/docs/ppts.md`
5. `docs/docs/ppgpsdo.md`

## Verification and related docs

- subsystem overview: `docs/docs/timestamp.md`
- direct tool entry point: `docs/docs/ppts.md`
- GPSDO integration: `docs/docs/ppgpsdo.md`
- host-side wrapper: `c++/timestamp.hh`
