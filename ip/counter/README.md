PulsePins counter subsystem.

This directory contains the on-chip measurement blocks used to inspect streamed or external digital signals. The counter subsystem acts as a shared measurement backplane: software selects one or more observed channels, chooses an instrument, latches a consistent snapshot, and reads the resulting statistics through a single Avalon-MM programming surface.

## Main files

- `counter_if.sv` - top-level Avalon-MM wrapper and instrument multiplexer
- `basic_counter.sv` - total/high/low/edge statistics for one selected channel
- `runs_counter.sv` - run-length statistics for high and low runs
- `packet_stats.sv` - valid/idle and packet-length statistics
- `seq_counter.sv` - histogram of short bit sequences
- `autocorrelation.sv` - lag-based autocorrelation counters for one channel
- `crosscorrelation.sv` - lag-based correlation counters for two selected channels (enabled if COUNTER_CC is defined)
- `time_counter.sv` - elapsed-time capture between start and stop edges
- `mux32to1.sv`, `mux32to2.sv` - channel selectors used by the wrapper
- `cdc.v` - local CDC helpers

## Architecture overview

`counter_if.sv` is the software-visible entry point.

Its job is to:

1. receive a wide sampled input bus `d`
2. choose one or two channels of interest through selector registers
3. feed those selected channels into several instruments in parallel
4. latch or reset the instrument bank on request
5. multiplex the chosen instrument result onto a single Avalon-MM read port

The subsystem therefore favors a compact control interface over independent per-instrument bus wrappers.

## Programming model

Software controls the subsystem by programming a small set of selector registers in `counter_if.sv`:

- choose the instrument number
- choose low/high 32-bit word when reading 64-bit counters
- choose the instrument-local result address
- choose `sel0`, `sel1`, and `sel2` channel indices
- pulse `latch_all` or `reset_all`

This model is shared by the C++ wrapper in `c++/counter.hh` and by the `ppcounter` command documented in `docs/docs/ppcounter.md`.

## Channel selection model

The wrapper exposes three channel selectors:

- `sel0` - primary single-bit observation channel used by most instruments
- `sel1`, `sel2` - secondary channel pair used by two-input measurements and timing capture

Most instruments use `sel0`, while correlation and elapsed-time paths consume `sel1` and `sel2` as well.

## Clocking and latching

The subsystem spans two clock domains:

- `clk` - Avalon-MM control and readout domain
- `d_clk` - sampled data domain

Important maintenance facts:

- live counting happens in `d_clk`
- `reset_all` and `latch_all` are synchronized into `d_clk`
- readout happens from the control side after a latch operation
- the time-counter path is slightly different: it converts selected channel edges into asynchronous start/stop events and measures elapsed time back in `clk`

See also `README.clock_domains` and `docs/docs/clock_domain.md`.

## Reading order for maintainers

1. `counter_if.sv`
2. `basic_counter.sv`
3. `runs_counter.sv`
4. `packet_stats.sv`
5. `seq_counter.sv`
6. `autocorrelation.sv`
7. `crosscorrelation.sv`
8. `time_counter.sv`

That order starts from the programming surface and then moves through the individual measurement families.

## Verification and related docs

- run RTL tests: `make -C ip test`
- architecture page: `docs/docs/counter.md`
- tool entry point: `docs/docs/ppcounter.md`
- CDC notes: `docs/docs/clock_domain.md`
- host-side wrapper: `c++/counter.hh`
