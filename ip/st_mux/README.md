PulsePins Avalon-ST multiplexer subsystem.

This directory contains a small integration block used to switch one downstream Avalon-ST consumer between two upstream producers while keeping simple per-input traffic counters.

## Main files

- `st_mux_if.sv` - Avalon-ST data multiplexer plus Avalon-MM control/status interface

## Architecture overview

`st_mux_if.sv` is intentionally simple:

1. software selects input channel 1 or 2 through a one-bit control register
2. the selected input drives the shared Avalon-ST output
3. only the selected input sees `ready` asserted
4. each input has an independent 64-bit transfer counter that increments on successful handshakes

The block does not buffer or reorder traffic. It is strictly a routing and statistics helper.

## Programming model

The Avalon-MM interface exposes:

- address `0` on write: select channel (`0` = input 1, `1` = input 2)
- addresses `0` and `1` on read: low/high words of input-1 transfer counter
- addresses `2` and `3` on read: low/high words of input-2 transfer counter

This model is mirrored by the host-side wrapper in `c++/st_mux.hh`.

## Reading order for maintainers

1. `st_mux_if.sv`
2. `c++/st_mux.hh`
3. `docs/docs/st_mux.md`

## Verification and related docs

- subsystem overview: `docs/docs/st_mux.md`
- host-side wrapper: `c++/st_mux.hh`
