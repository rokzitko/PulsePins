## Timestamp capture

PulsePins includes a dedicated timestamp-capture block for recording the clock-cycle position of selected events.

The main hardware block is `ip/ts_core/ts_core.sv`, and the main software interface is `c++/timestamp.hh`.

For maintainers, the important split is:

* `ip/ts_core/ts_core.sv` owns the free-running counter and edge-capture logic
* `c++/timestamp.hh` owns source routing, FIFO draining, and reconstruction of 64-bit event records

### Hardware model

`ts_core` maintains a free-running counter and captures its value when monitored asynchronous inputs change.

The current implementation exposes two capture paths:

* `PPS` path
* `sigA` path

Each path produces timestamp values over its own Avalon-ST output FIFO.

In the current implementation, both capture paths trigger on rising edges only.

Internally the block:

* synchronizes each asynchronous input through a short flip-flop chain
* detects a rising edge
* copies the current free-running counter value into a capture register
* asserts an Avalon-ST valid pulse for one cycle when the downstream FIFO is ready

This makes the block simple and deterministic, but it also means that timestamps are quantized to the `ts_core` clock.

### Source selection

The timestamp software interface exposes selectable signal sources for `sigA` and for the PPS reference path.

Supported `sigA` source selectors include:

* streamer trigger activated
* streamer trigger input 0
* external trigger input 0
* auxiliary input 0
* generated pulse sources from `1 ms` up to `1 s`

The PPS input can be switched between:

* external PPS input
* crystal-derived PPS source

The selector values exposed in `c++/timestamp.hh` are:

* `0` - streamer trigger activated
* `1` - streamer trigger input 0
* `2` - external trigger input 0
* `3` - auxiliary input 0
* `4` - 1 s pulse
* `5` - 100 ms pulse
* `6` - 10 ms pulse
* `7` - 1 ms pulse

### C++ interface

The `timestamp` class in `c++/timestamp.hh` wraps two FIFOs and a configuration PIO block.

Important operations:

* `sel_pps_in()` / `sel_pps_xtal()` - choose the PPS source
* `selA()` - choose the `sigA` source
* `get_cfg()` - return a readable summary of the current routing
* `filled()` / `filledA()` - report whether FIFO data is available
* `clear_fifo()` / `clear_fifoA()` - drain old samples
* `read()` / `readA()` - read one 64-bit timestamp from each path
* `read_with_timeout()` / `readA_with_timeout()` - wait for a sample with timeout handling

Each timestamp is assembled from two 32-bit FIFO words into a 64-bit counter value.

The timeout-based read functions poll until two FIFO words are available, then reconstruct the full 64-bit timestamp. This is why the timeout is applied to a complete event record rather than to a single 32-bit transfer.

The constructor clears both FIFOs on startup, which helps avoid stale samples after reset or reconfiguration.

The main user-facing command implementations live in `c++/pptool_measurement.cc` (`ppts` and `ppgpsdo`).

### Timing semantics

The returned timestamps are counter values, not elapsed times in seconds. To convert them into time units, use the clock frequency of the `ts_core` timebase.

Differences between successive timestamps are often more useful than the absolute counter values:

* PPS-to-PPS differences show the effective period of the selected PPS source
* `sigA`-to-`sigA` differences show the period of the selected event source
* comparisons between the two paths are useful for phase or drift analysis

### Tool integration

`ppts` is the direct command-line tool for live timestamp observation.

`ppgpsdo` reuses the same timestamp hardware to compare PPS against another timing reference when disciplining an oscillator.

`ppgpsdo` effectively consumes the two timestamp streams as paired event series and computes phase-error updates from their differences.

### Typical use cases

Use the timestamp block for:

* observing PPS alignment and jitter
* measuring the relative timing of internal and external trigger sources
* debugging trigger routing
* building higher-level closed-loop timing tools such as `ppgpsdo`

Because the capture logic is edge-based and FIFO-backed, this subsystem is best suited to sparse event streams such as PPS pulses, trigger events, and derived low-rate timing markers rather than dense arbitrary waveforms.

### Related pages

* `ppts.md`
* `ppgpsdo.md`
* `cpp.md`
