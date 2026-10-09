PulsePins frequency meter subsystem.

This directory contains the multi-channel clock-frequency measurement block used to observe external, internal, streamer, and core clock rates from software.

## Main files

- `freq_meter.sv` - Avalon-MM controlled multi-channel frequency meter with Gray-coded CDC handling

## Architecture overview

Each observed signal is itself a running clock, so the subsystem cannot measure it by ordinary sampling in the Avalon domain.

Instead, `freq_meter.sv`:

1. counts edges in each observed input clock domain
2. converts each counter to Gray code
3. synchronizes the Gray value into the gate/reference clock domain
4. computes a delta over one gate interval
5. transfers coherent latest results back into the Avalon domain through an acknowledged mailbox

This makes the block robust across unrelated clock domains while keeping the software-visible model simple.

## Programming model

The Avalon-MM register file exposes:

- control register - enable and clear-pulse control
- gate-length register - measurement window in `cnt_clk` cycles
- number-of-channels register (4)
- one result register per channel

The hardware continuously produces per-gate edge counts while enabled. Software converts those counts into Hz using the known gate length and nominal counter-clock frequency.

`clear` restarts the count-domain measurement baseline and publishes zeroed Avalon-visible result registers through the normal result update path. Direct register readers still need to allow CDC propagation before expecting the zeroed snapshot to be visible.

Configuration crosses as one latest-value bundle; back-to-back writes cannot
cancel each other. Clear requests carry a sequence number through the same
mailbox. Result updates may coalesce when the Avalon clock is slower than the
measurement cadence, but the latest completed measurement still propagates.
All channel results are committed together in the Avalon clock domain. The two
domain resets must represent the same logical reset event, as required by the
shared CDC helper. Gate length zero is interpreted as one count-clock cycle.

## Reading order for maintainers

1. `freq_meter.sv`
2. `c++/freq_meter.hh`
3. `docs/docs/freq_meter.md`
4. `docs/docs/ppfreq.md`

## Verification and related docs

- subsystem overview: `docs/docs/freq_meter.md`
- direct tool entry point: `docs/docs/ppfreq.md`
- HPS-side wrapper: `c++/freq_meter.hh`
- boundary and CDC regressions: `tb/README.md`
