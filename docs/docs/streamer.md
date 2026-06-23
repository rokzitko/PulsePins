# Streamer subsystem

The streamer is the core PulsePins output engine.

It accepts a compact sequence of encoded elements on the control side, expands them into timed output updates, waits for the configured trigger program, and emits the final `qout` stream in the active `streamer_clk` domain.

The main integration wrapper is `ip/streamer/st_interface.sv`, and the main internal glue module is `ip/streamer/streamer.sv`.

## Why this block exists

PulsePins is built around the idea that long, deterministic digital output programs should be cheap to store, easy to generate in software, and reliable to replay in hardware.

The streamer achieves that by separating responsibilities:

* software builds or parses compact [run-length encoded sequences](index.md#run-length-encoding)
* the streamer decodes them into output updates on the FPGA
* trigger and gate logic decide when those updates are allowed to advance
* readback and counters provide verification and observability

## External interfaces

The streamer uses Intel Avalon interfaces: [Avalon-ST](https://www.intel.com/content/www/us/en/docs/programmable/683091/22-3/avalon-streaming-interfaces.html) for sequence ingress and [Avalon-MM](https://www.intel.com/content/www/us/en/docs/programmable/683091/22-3/avalon-memory-mapped-interfaces.html) for control/status registers.

### Avalon-ST sequence ingress

The data ingress port on `st_interface.sv` receives encoded `{control, counter, data}` elements.

Conceptually each regular element says:

* what output update to perform
* what value or mask to use
* how many `streamer_clk` periods that state should persist

Trigger elements reuse the same ingress path but are diverted into the trigger-program loader rather than the normal decode path.

### Avalon-MM control/status interface

`st_interface.sv` provides a small register map for:

* trigger control
* output override and initial output value
* gating control
* FIFO statistics and overflow state
* CRC readback
* live output visibility

The register constants live in `ip/streamer/config.vh`.

### Runtime signals

At runtime the streamer also consumes:

* `trigger_in` - external trigger bus sampled in the output domain
* `gate_in` - optional external gate input
* `streamer_clk` - the active streaming/output clock

And it produces:

* `qout` - active output word
* `qout_valid` - output-valid qualifier
* `qout_strobe` - output strobe pulse
* `done` - successful stream completion
* `buffer_error` - output underrun/error indication

## Datapath structure

The streamer is intentionally split into control-side and output-side stages.

### 1. Control-side buffering

`input_fifo.sv` buffers incoming encoded elements in the `clk` domain.

This decouples host-side burstiness from the decode path and allows the software interface to rely on normal backpressure through `asi_ready` rather than cycle-perfect pacing.

### 2. Element classification

`streamer.sv` examines the incoming control word:

* regular elements go to `rl_decoder.sv`
* trigger elements go to `chain_trigger.sv`

This split is one of the most important maintenance facts in the subsystem: the trigger program is carried in the same encoded transport format as data elements, but it is not part of the normal decode/output stream.

### 3. Decode and preprocessing

`rl_decoder.sv` expands regular elements into output updates.

The supported operations are defined by the control-bit layout and operation enum in `config.vh`. These include direct loads, bitwise updates, replay-related control, retrigger behavior, and PRNG-backed output generation.

`preprocessor.sv` exists to support second-level compression features such as short stored subsequences and replay.

### 4. Output-domain crossing

Decoded output updates are written into `output_fifo.sv` in the control clock domain and read out in `streamer_clk`.

This FIFO is the key CDC boundary in the subsystem. It is also where underrun behavior and completion tracking become visible through `buffer_error` and `done`.

### 5. Trigger and gating policy

`chain_trigger.sv` runs in the output domain and controls when streaming is allowed to start.

Once triggered, output advancement is additionally qualified by:

* `gate_enable`
* `stop`
* optional stop-on-buffer-error policy

In `streamer.sv`, the output FIFO read request is gated by:

* trigger active
* gate open

This makes gating an output-side pacing mechanism rather than an input-side buffering mechanism.

For the concise trigger, gate, and output-valid timing summary, see [RTL latency and timing](latency.md).

## Trigger model

The trigger subsystem supports more than a single mask/pattern comparison.

`chain_trigger.sv` loads a trigger program from the same ingress stream used for regular sequence elements. Each trigger stage contains a pattern and mask, and the chain advances through those stages until the final condition is satisfied.

Important behavioral facts:

* trigger configuration is loaded from sequence elements before streaming begins
* the trigger chain runs in `streamer_clk`
* forced trigger can bypass normal input-condition detection
* retrigger support lets a sequence pause and wait for a later trigger event

On the host side there are two related trigger-control paths:

* `c++/trigger.hh` configures the trigger combiner that selects and conditions the upstream trigger sources
* `c++/trigger_int.hh` and `c++/trigger_ext.hh` provide direct software control and status visibility for the low-level trigger PIOs

For lower-level trigger implementation details, see `details.md`.

## Gating and output override

`st_interface.sv` adds two important runtime control features on top of the raw streamer core.

### Gating

Gating decides whether output-side advancement is allowed.

The effective gate can be sourced from:

* `gate_in`
* a masked subset of `trigger_in`

This is useful when the output should be paused by an external signal without resetting the stream.

### Output override

`qout_select` lets software present a manual override value instead of the normal streamer output.

This is mainly useful for debugging, bring-up, and simple manual output control without rebuilding a full sequence.

## Register summary

The authoritative register enums live in `ip/streamer/config.vh`.

Write-side registers in `st_interface.sv`:

| Name | Address | Purpose |
| ---- | ------- | ------- |
| `IF_CTRL` | `0` | stop, internal trigger control, reset, output-select, stop-on-buffer-error |
| `INIT_VAL` | `4` | output value visible before triggering |
| `QOUT_OVERRIDE` | `6` | manual output value used when override is selected |
| `GATING_W` | `7` | gating enable, gate source selection, and trigger-bit mask |

Read-side registers:

| Name | Address | Purpose |
| ---- | ------- | ------- |
| `IF_STATUS` | `0` | armed, triggered, done, buffer-error state |
| `EXT_TRIG_IN` | `1` | live trigger-input observation |
| `QOUT_STREAMER` | `2` | raw streamer output before override mux |
| `EXT_TRIG_CTRL` | `3` | external trigger-control inputs |
| `QOUT` | `4` | final visible output word |
| `OVERFLOW` | `5` | input FIFO overflow flags |
| `CRC32` | `6` | output-stream CRC |
| `GATING_R` | `7` | live gating state and selected signals |
| `ST_INF1_*`, `ST_INF2_*`, `ST_OUTF_*` | `8`-`19` | FIFO traffic counters |

The addresses above are register word indices as used by the integrated Avalon-MM interface.

## Clocking and reset

The streamer spans two important domains:

* `clk` - control-side logic, ingress buffering, decode path, register programming
* `streamer_clk` - trigger evaluation, output pacing, output-valid/strobe timing

The main reset crossing is `streamer_rst`, which is synchronized from the top-level `reset` signal into `streamer_clk`.

For the broader system clock tree and ownership model, see `clock_domain.md`.

## Software integration

On the host side, the main C++ entry points are:

* `c++/streamer_control.hh` - control/status wrapper for `st_interface.sv`
* `c++/streamer_fifo.hh` and [DMA](https://en.wikipedia.org/wiki/Direct_memory_access)-backed streamer wrappers - sequence transport into the ingress FIFO or memory-backed path
* `c++/ppworkflow.hh` - shared send/trigger/check flow used by multiple tools
* `c++/sequence.hh` and `c++/elements.hh` - host-side sequence representation

The DMA-backed path stages encoded sequence data in SDRAM and lets the FPGA-side [Intel/Altera Modular Scatter-Gather DMA](https://www.intel.com/content/www/us/en/docs/programmable/683130/21-4/modular-scatter-gather-dma-core.html) engine feed the streamer path. This is mainly useful for longer or repeated transfers where CPU-driven FIFO writes would add avoidable host-side overhead.

CLI tools such as `ppfg`, `ppdelay`, `ppplay`, and `pptest` all eventually program this subsystem.

## How to customize it safely

If you want to modify the streamer for long-term maintainability, the safest entry points are:

* external programming model changes - start with `st_interface.sv` and `config.vh`
* new element semantics - update `config.vh`, `rl_decoder.sv`, and the host-side sequence model together
* trigger behavior - update `chain_trigger.sv` and verify software assumptions about arming/forcing/retriggering
* throughput or buffering changes - review both FIFO sizing in `config.vh` and the completion/overflow checks exposed to software

When changing behavior, update both the hardware docs and the host-side assumptions in `c++/` at the same time.

## Related files and docs

* `ip/streamer/README.md`
* `ip/streamer/st_interface.sv`
* `ip/streamer/streamer.sv`
* `ip/streamer/config.vh`
* `latency.md`
* `readback.md`
* `clock_domain.md`
* `cpp.md`
