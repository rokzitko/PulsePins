# Streamer subsystem

The streamer is the core PulsePins output engine.

It accepts a compact sequence of encoded elements on the control side, expands them into timed output updates, waits for the configured trigger program, and emits the final `qout` stream in the active `streamer_clk` domain.

The main integration wrapper is [`ip/streamer/st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }}), and the main internal glue module is [`ip/streamer/streamer.sv`]({{ source_file("ip/streamer/streamer.sv") }}).

## Why this block exists

PulsePins is built around the idea that long, deterministic digital output programs should be cheap to store, easy to generate in software, and reliable to replay in hardware.

The streamer achieves that by separating responsibilities:

* software builds or parses compact [run-length-encoded sequences](sequencer_model.md#data-path-and-run-length-encoding)
* the streamer decodes them into output updates on the FPGA
* trigger and gate logic decide when those updates are allowed to advance
* readback and counters provide verification and diagnostic visibility

## External interfaces

The streamer uses Intel Avalon interfaces: [Avalon-ST](https://www.intel.com/content/www/us/en/docs/programmable/683091/22-3/avalon-streaming-interfaces.html) for sequence ingress and [Avalon-MM](https://www.intel.com/content/www/us/en/docs/programmable/683091/22-3/avalon-memory-mapped-interfaces.html) for control/status registers.

### Avalon-ST sequence ingress

The data ingress port on [`st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }}) receives encoded `{control, counter, data}` elements.

The ingress also accepts an Avalon-ST channel tag. The streamer does not interpret this tag; it is present so generated width adapters can keep partial elements from different upstream sources separate before data reaches `st_interface.sv`.

Conceptually each regular element says:

* what output update to perform
* what value or mask to use
* how many `streamer_clk` periods that state should persist

Trigger elements reuse the same ingress path but are diverted into the trigger-program loader rather than the normal decode path.

### Avalon-MM control/status interface

[`st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }}) provides a small register map for:

* trigger control
* output override and initial output value
* gating control
* FIFO statistics and overflow state
* CRC readout
* live output visibility

The register constants live in [`ip/streamer/config.vh`]({{ source_file("ip/streamer/config.vh") }}).

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

[`input_fifo.sv`]({{ source_file("ip/streamer/input_fifo.sv") }}) buffers incoming encoded elements in the `clk` domain.

This decouples ARM-side write burstiness from the decode path and allows the software interface to rely on normal backpressure through `asi_ready` rather than cycle-perfect pacing.

### 2. Element classification

[`streamer.sv`]({{ source_file("ip/streamer/streamer.sv") }}) examines the incoming control word:

* regular elements go to [`rl_decoder.sv`]({{ source_file("ip/streamer/rl_decoder.sv") }})
* trigger elements go to [`chain_trigger.sv`]({{ source_file("ip/streamer/chain_trigger.sv") }})

This split is one of the most important maintenance facts in the subsystem: the trigger program is carried in the same encoded transport format as data elements, but it is not part of the normal decode/output stream.

### 3. Decode and preprocessing

[`rl_decoder.sv`]({{ source_file("ip/streamer/rl_decoder.sv") }}) expands regular elements into output updates.

The supported operations are defined by the control-bit layout and operation enum in [`config.vh`]({{ source_file("ip/streamer/config.vh") }}). These include direct loads, bitwise updates, replay-related control, retrigger behavior, and PRNG-backed output generation.

[`preprocessor.sv`]({{ source_file("ip/streamer/preprocessor.sv") }}) exists to support second-level compression features such as short stored subsequences and replay.

### 4. Output-domain crossing

Decoded output updates are written into [`output_fifo.sv`]({{ source_file("ip/streamer/output_fifo.sv") }}) in the control clock domain and read out in `streamer_clk`.

This FIFO is the key CDC boundary in the subsystem. It is also where underrun behavior and completion tracking become visible through `buffer_error` and `done`.

`done` is intentionally clean-completion-only. If `buffer_error` has latched, a later terminator can still stop output progression internally, but public `done` remains low. Software waits may stop on `done || buffer_error`, while successful completion is `done && !buffer_error`.

At the top level, four streamer cores share one combined output/status path. The board-visible aggregate `streamer_done` is controlled by two software masks in `pio_cfg`:

* `streamer_active_mask`: streamers included in aggregate done evaluation
* `streamer_armed_live_mask`: selected streamers whose armed state counts as live; activation always counts as live

The aggregate `streamer_done` is high when `streamer_active_mask` is nonzero, at least one selected streamer has cleanly completed, and no selected streamer is live under its configured live definition. Setting `streamer_armed_live_mask` to zero supports mutually exclusive trigger programs where only the streamer that actually activates should block or complete the aggregate run. Software must set these masks during run setup and must not change them while selected streamers are armed or activated.

### 5. Trigger and gating behavior

[`chain_trigger.sv`]({{ source_file("ip/streamer/chain_trigger.sv") }}) runs in the output domain and controls when streaming is allowed to start.

Once triggered, output advancement is additionally qualified by:

* `gate_enable`
* `stop`
* optional stop-on-buffer-error setting

In [`streamer.sv`]({{ source_file("ip/streamer/streamer.sv") }}), the output FIFO read request is gated by:

* trigger active
* gate open

This makes gating an output-side pacing mechanism rather than an input-side buffering mechanism.

#### Gate, trigger reset, and stop

`gate`, `trigger_reset`, and `stop` can all stop visible output advancement, but they act at
different layers and have different resume behavior.

| Control | Stops by | Preserves | How flow can continue | Intended use |
| ------- | -------- | --------- | --------------------- | ------------ |
| `gate` | Closing `gate_enable`, so `rdreq` stays low even while the trigger remains active | Trigger state, FIFO contents, and current stream position | Reopen the gate; no new trigger is required | External pacing/readiness windows; pause and resume output without resetting the trigger or stream |
| `stop` | Masking `trigger_activated`, so `rdreq` stays low | Trigger latch, FIFO contents, and current stream position; the unfinished stream is still considered active, not idle | Clear `stop`; if the trigger is still latched and no terminator was consumed, output resumes immediately | Software halt/abort control, especially before streamer reset during timeout recovery |
| `trigger_reset` | Resetting the trigger-chain FSM so the trigger output deasserts | Streamer/output FIFOs are not cleared; the active trigger condition and remaining queued trigger conditions are preserved | Release reset, then activate the trigger again by force or by the same active trigger condition | Trigger re-arm, interlocks, and flows that should wait for the same trigger again rather than simply unpause |

The main practical difference is that `gate` and `stop` pause an already triggered stream without
resetting the trigger chain. `trigger_reset` clears the trigger state itself while preserving the
loaded trigger condition. Holding `trigger_reset` asserted prevents triggering, including forced
triggering, but does not clear the streamer FIFOs.

None of these controls consumes the terminator while output reads are stopped, so `done` does not
assert until playback later reaches the final element. None of them forces `qout` to a safe value;
`qout` holds the last driven value unless a final element, output override, or streamer reset
changes it.

The trigger chain latches its triggered state until trigger reset or streamer reset. For that reason,
software timeout handling must not rely only on clearing trigger-force or trigger-enable bits after an
already-triggered stream. The shared control-software workflow treats a streamer-completion timeout as an abort:
it asserts the runtime `stop` control to halt output progression, clears or disables the internal
trigger control path, then pulses streamer reset before returning `RC_TIMEOUT`.

For the concise trigger, gate, and output-valid timing summary, see [RTL latency and timing](latency.md). For idealized waveform diagrams of these conventions, see [Streamer timing diagrams](streamer_timing.md).

## Trigger model

The trigger subsystem supports more than a single mask/pattern comparison.

[`chain_trigger.sv`]({{ source_file("ip/streamer/chain_trigger.sv") }}) loads a trigger program from the same ingress stream used for regular sequence elements. Each trigger stage contains a pattern and mask, and the chain advances through those stages until the final condition is satisfied.

Important behavioral facts:

* trigger configuration is loaded from sequence elements before streaming begins
* the trigger chain runs in `streamer_clk`
* forced trigger can bypass normal input-condition detection
* retrigger support lets a sequence pause and wait for a later trigger event

### `trigger_reset` versus retrigger

`trigger_reset` and a retrigger sequence element both return the trigger-chain FSM to idle, but
they are not equivalent.

| Mechanism | Source | Active trigger condition | Queued trigger conditions | Intended use |
| --------- | ------ | ------------------------ | ------------------------- | ------------ |
| `trigger_reset` signal | Runtime control signal from software, external input, or switch/combiner path | Preserved. If the chain was armed or already triggered on a loaded condition, releasing reset re-arms that same condition. | Preserved. The trigger FIFO is not cleared. | Interlocks, manual re-arm, and abort/retry flows that should wait for the same trigger condition again. |
| Retrigger element | Encoded sequence element consumed from the output stream after triggering | Discarded. The just-satisfied trigger condition or subsequence is complete. | Preserved. The next queued trigger condition or subsequence is loaded after retriggering. | Sequence-controlled pauses between output bursts, where each burst should wait for the next trigger subsequence. |

Use `trigger_reset` when the trigger program should remain logically at the same stage. Use a
retrigger element when the sequence has completed one triggered segment and should advance to the
next trigger segment. Use streamer reset, not `trigger_reset`, when the trigger FIFO itself must be
cleared.

The ARM-side control software has two related trigger-control paths:

* [`c++/trigger.hh`]({{ source_file("c++/trigger.hh") }}) configures the trigger combiner that selects and conditions the upstream trigger sources
* [`c++/trigger_int.hh`]({{ source_file("c++/trigger_int.hh") }}) and [`c++/trigger_ext.hh`]({{ source_file("c++/trigger_ext.hh") }}) provide direct software control and status visibility for the low-level trigger PIOs

For lower-level trigger implementation details, see `details.md`.

## Gating and output override

[`st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }}) adds two important runtime control features on top of the raw streamer core.

### Gating

Gating decides whether output-side advancement is allowed.

The effective gate can be sourced from:

* `gate_in`
* a masked subset of `trigger_in`

This is useful when the output should be paused by an external signal without resetting the stream.

### Output override

`qout_select` lets software present a manual override value instead of the normal streamer output.

This is mainly useful for debugging, bring-up, and simple manual output control without rebuilding a full sequence.

Output override, initial output value, gating configuration, and the stop-on-buffer-error setting are
static streamer configuration. Writes are accepted by the Avalon-MM register file immediately, but the
streamer-clock shadow configuration is updated only while the streamer is idle or held in streamer
reset. Writes made during active playback therefore take effect on the next idle/reset window, not in
the middle of the current output sequence.

## Register summary

The authoritative register enums live in [`ip/streamer/config.vh`]({{ source_file("ip/streamer/config.vh") }}).

Write-side registers in [`st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }}):

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
| `OVERFLOW` | `5` | input FIFO invariant/overflow flags |
| `CRC32` | `6` | output-stream CRC |
| `GATING_R` | `7` | live gating state and selected signals |
| `ST_INF1_*`, `ST_INF2_*`, `ST_OUTF_*` | `8`-`19` | FIFO traffic counters |

The addresses above are register word indices as used by the integrated Avalon-MM interface.

Readback registers that observe `streamer_clk` state (`IF_STATUS`, `QOUT`, `QOUT_STREAMER`,
`CRC32`, `GATING_R`, trigger input visibility, and output-FIFO read counters) return synchronized
snapshots in the Avalon/control clock domain. They are coherent CDC samples with a small crossing
latency, not cycle-exact instantaneous taps.

The `OVERFLOW` bits are diagnostics for conditions that should be impossible by construction during
normal operation. Legal `valid && !ready` input backpressure and output FIFO `almost_full`
backpressure are expected flow-control states and do not indicate overflow.

## Clocking and reset

The streamer spans two important domains:

* `clk` - control-side logic, ingress buffering, decode path, register programming
* `streamer_clk` - trigger evaluation, output pacing, and `qout_valid`/`qout_strobe` timing

The main reset crossing is `streamer_rst`, which is synchronized from the top-level reset and the
software streamer-reset request into `streamer_clk`. Runtime trigger, stop, gate, and trigger-input
levels are synchronized into `streamer_clk`; static output/gating configuration crosses through a
latest-value CDC update helper and commits only while idle/reset.

For the broader system clock tree and ownership model, see `clock_domain.md`.

## Software integration

In the ARM-side control software, the main C++ interfaces are:

* [`c++/streamer_control.hh`]({{ source_file("c++/streamer_control.hh") }}) - control/status wrapper for [`st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }})
* [`c++/streamer_fifo.hh`]({{ source_file("c++/streamer_fifo.hh") }}) and [DMA](https://en.wikipedia.org/wiki/Direct_memory_access)-backed streamer wrappers - sequence transport into the ingress FIFO or memory-backed path
* [`c++/ppworkflow.hh`]({{ source_file("c++/ppworkflow.hh") }}) - shared send/trigger/check flow used by multiple tools
* [`c++/sequence.hh`]({{ source_file("c++/sequence.hh") }}) and [`c++/elements.hh`]({{ source_file("c++/elements.hh") }}) - C++ sequence representation

The DMA-backed path stages encoded sequence data in SDRAM and lets the FPGA-side [Intel/Altera Modular Scatter-Gather DMA](https://www.intel.com/content/www/us/en/docs/programmable/683130/21-4/modular-scatter-gather-dma-core.html) engine feed the streamer path. This is mainly useful for longer or repeated transfers where CPU-driven FIFO writes would add avoidable ARM-side overhead.

CLI tools such as `ppfg`, `ppdelay`, `ppplay`, and `pptest` all eventually program this subsystem.

## How to customize it safely

If you want to modify the streamer for long-term maintainability, the safest starting files are:

* external programming model changes - start with [`st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }}) and [`config.vh`]({{ source_file("ip/streamer/config.vh") }})
* new element semantics - update [`config.vh`]({{ source_file("ip/streamer/config.vh") }}), [`rl_decoder.sv`]({{ source_file("ip/streamer/rl_decoder.sv") }}), and the software-side sequence model together
* trigger behavior - update [`chain_trigger.sv`]({{ source_file("ip/streamer/chain_trigger.sv") }}) and verify software assumptions about arming/forcing/retriggering
* throughput or buffering changes - review both FIFO sizing in [`config.vh`]({{ source_file("ip/streamer/config.vh") }}) and the completion/overflow checks exposed to software

When changing behavior, update both the hardware docs and the control-software assumptions in [`c++/`]({{ source_file("c++/") }}) at the same time.

## Related files and docs

* [`ip/streamer/README.md`]({{ source_file("ip/streamer/README.md") }})
* [`ip/streamer/st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }})
* [`ip/streamer/streamer.sv`]({{ source_file("ip/streamer/streamer.sv") }})
* [`ip/streamer/config.vh`]({{ source_file("ip/streamer/config.vh") }})
* [RTL latency and timing](latency.md)
* [Readback](readback.md)
* [Clock domains, clock selection, and CDC](clock_domain.md)
* [C++ application programming interface](cpp.md)
