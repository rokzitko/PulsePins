# RTL latency and timing

## Summary

Use this page when checking how quickly an RTL event can become visible.

* Streamer output timing is owned by `streamer_clk`, not by the Avalon/control `clk`.
* The streamer output FIFO is a CDC boundary. Trigger and gate changes affect output FIFO reads in `streamer_clk`, but FIFO and read-side state can still be visible after a gate closes.
* Treat `qout_valid` and `qout_strobe` as the output activity indicators. `qout` can hold its previous value when output advancement stops.
* Trigger force, trigger reset, retrigger, `done`, and `buffer_error` are state-machine effects, not combinational shortcuts.
* Fixed cycle counts are listed only for simple single-domain registered paths. FIFO, synchronizer, and CDC paths are described by observable behavior instead of one universal cycle number.

For CDC terminology used in the tables below, see [CDC background terms](clock_domain.md#cdc-background-terms). Here, CDC paths have observable behavior rather than one universal cycle count because FIFO state, synchronizer phase, and source/destination clock relationships can vary.

For documentation waveforms focused on streamer output, triggering, gating, retriggering, and `qout_strobe`, see [Streamer timing diagrams](streamer_timing.md).

## Streamer Timing

| Area | Timing behavior | Practical consequence |
| ---- | --------------- | --------------------- |
| Ingress | [`st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }}) accepts Avalon-ST sequence data in `clk`; [`input_fifo.sv`]({{ source_file("ip/streamer/input_fifo.sv") }}) buffers it before decode. | Host-side writes are decoupled from output timing. `asi_ready` is a transport backpressure signal, not an output-cycle promise. |
| RLE decode | [`rl_decoder.sv`]({{ source_file("ip/streamer/rl_decoder.sv") }}) runs in `clk`. While active and not backpressured, it can write one decoded update per `clk`; loading a new run can insert a bubble. | Decode throughput is not the same thing as pin timing. Decoded updates still cross into `streamer_clk`. |
| Output CDC | [`output_fifo.sv`]({{ source_file("ip/streamer/output_fifo.sv") }}) writes decoded updates in `clk` and reads them in `streamer_clk`. | There is no fixed control-clock-to-output-clock latency. Use FIFO status, valid, and completion signals rather than a hard-coded cycle count. |
| Trigger program | Trigger elements are loaded in `clk` and consumed by [`chain_trigger.sv`]({{ source_file("ip/streamer/chain_trigger.sv") }}) in `streamer_clk`. | Trigger-program availability crosses a FIFO, so arming latency depends on the clock relationship and FIFO state. |
| Simple trigger match | [`and_trigger.sv`]({{ source_file("ip/streamer/and_trigger.sv") }}) samples `trigger_in` in `streamer_clk` and latches when the masked pattern matches while enabled. | Trigger inputs are synchronous to the trigger chain only after sampling in `streamer_clk`. |
| Trigger activation | [`chain_trigger.sv`]({{ source_file("ip/streamer/chain_trigger.sv") }}) is a `streamer_clk` state machine. `trigger_force` moves it to the triggered state, subject to reset. | Forced trigger is not a combinational path to the pins; output still advances through the streamer state and FIFO read path. |
| Output read request | In [`streamer.sv`]({{ source_file("ip/streamer/streamer.sv") }}), the output FIFO read request is `trigger_activated && gate_enable`. | Streaming advances only after trigger activation and while the effective gate is open. |
| Gate open | [`st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }}) forms `gate_enable` from optional `gate_in` and masked `trigger_in` sources. | Opening the gate permits output FIFO reads; it does not reset or reload the stream. |
| Gate close | Closing the gate stops new output FIFO reads. Registered/read-side state may remain visible, and `qout` may hold its last value. | Do not expect a zero-latency visible pause. Use `qout_valid` or strobe activity to detect advancement. |
| `qout` before trigger | [`streamer.sv`]({{ source_file("ip/streamer/streamer.sv") }}) drives `initial_value` until a `streamer_clk` latch observes trigger activation. | The initial output value is intentional pre-trigger state. |
| `qout_valid` | [`output_fifo.sv`]({{ source_file("ip/streamer/output_fifo.sv") }}) registers `qout_valid` for valid data elements read from the output FIFO. No-strobe elements suppress it. | Treat `qout_valid` as the sampled-output qualifier. |
| `qout_strobe` | [`output_fifo.sv`]({{ source_file("ip/streamer/output_fifo.sv") }}) derives the strobe from `qout_valid` and the inverted read clock. | Strobes are tied to valid output-domain data, not to every held `qout` value. |
| `done` | [`output_fifo.sv`]({{ source_file("ip/streamer/output_fifo.sv") }}) asserts `done` when the terminating element is read without a prior buffer error. | Clean completion is observed in the output domain after the terminator reaches the FIFO read side. |
| `buffer_error` | [`output_fifo.sv`]({{ source_file("ip/streamer/output_fifo.sv") }}) asserts `buffer_error` when a data read is attempted while the output FIFO is empty before completion. | Buffer errors are output-domain underrun observations. |
| Retrigger | A retrigger element raises `retrig_requested`; the trigger chain returns to idle and waits for a later trigger. | Retrigger pauses advancement through trigger state, not by rewinding already decoded FIFO contents. |

## Other RTL Blocks

| Block | Domain or path | Timing behavior |
| ----- | -------------- | --------------- |
| [`crc32.sv`]({{ source_file("ip/misc/crc32.sv") }}) | Single `clk` domain; used from [`st_interface.sv`]({{ source_file("ip/streamer/st_interface.sv") }}) in `streamer_clk`. | `crc_valid` is asserted one cycle after `data_en`; `crc_out` is the registered [CRC32](readback.md#crc32-integrity-checks) value for that input word. |
| [`combiner.sv`]({{ source_file("ip/combiner/combiner.sv") }}) | Control `clock_clk`, signal `clk`. | The normal output combiner registers inputs and registers the final output, so it is part of the timed datapath. |
| [`combiner_comb.sv`]({{ source_file("ip/combiner_comb/combiner_comb.sv") }}) | Control `clock_clk`, combinational signal path. | The output path is combinational after the programmed configuration. Use only when that tradeoff is desired. |
| [`combiner_trig.sv`]({{ source_file("ip/combiner_trig/combiner_trig.sv") }}) | Control `clock_clk`, trigger signal `clk`. | Trigger groups follow the registered combiner structure, with registered inputs and final output. |
| [`st_mux_if.sv`]({{ source_file("ip/st_mux/st_mux_if.sv") }}) | Single `clk` domain. | Selected Avalon-ST `data`, `valid`, and `ready` are combinational; selected channel, counters, and readback are registered. |
| [`rl_encoder.sv`]({{ source_file("ip/rl_encoder_if/rl_encoder.sv") }}) | Sample input clock to software-side `clk` through a dual-clock FIFO. | Samples while `valid` is high, emits a run on value change, count saturation, or validity drop, then crosses the FIFO. |
| [`counter_if.sv`]({{ source_file("ip/counter/counter_if.sv") }}) | Data `d_clk`, control/readout `clk`. | Input data is registered in `d_clk`; latch/reset controls cross into `d_clk`; Avalon readout is registered in `clk`. |
| [`time_counter.sv`]({{ source_file("ip/counter/time_counter.sv") }}) | Control `clk`. | Start/stop inputs are synchronized, edge-detected, and counted in `clk`; `ready` latches after a completed stop event. |
| [`ts_core.sv`]({{ source_file("ip/ts_core/ts_core.sv") }}) | Timebase `clk`. | Asynchronous inputs use three-stage synchronizers; rising edges capture into a one-event pending slot. Avalon-ST `valid` stays asserted until downstream accepts the event; a later edge while pending sets the per-path overflow latch/counter. |
| [`freq_meter.sv`]({{ source_file("ip/freq_meter/freq_meter.sv") }}) | Input clocks to `cnt_clk` to Avalon `avs_clk`. | Edge counters cross by Gray-code synchronization; results update once per gate interval and then cross back by update toggles. `clear` restarts the count-domain baseline and publishes zeroed results through the same CDC update path. |
| `sync_bit_2stage` | Destination `clk_dest`. | Single-bit synchronizer with two destination-clock stages. |
| `sync_bit_3stage` | Destination `clk_dest`. | Single-bit synchronizer with three destination-clock stages. |

## Reading The Tables

| Pattern | How to interpret latency |
| ------- | ------------------------ |
| Single-domain registered logic | Count cycles in the owning clock domain when the table gives a fixed behavior. |
| Dual-clock FIFO | Expect variable visibility based on FIFO state, synchronizer depth, and clock relationship. Use valid/ready/status signals. |
| Single-bit synchronizer | Delay is measured in destination-clock samples and depends on source transition phase. |
| Multi-bit CDC or software-stable control | Do not infer safety from timing constraints alone. The RTL must provide a FIFO, mailbox, synchronizer protocol, or documented software-stable behavior. |

For clock ownership and timing constraints, see [Clock domains](clock_domain.md).
