# Frequency meter

PulsePins includes a multi-channel frequency meter for reporting the observed frequencies of important on-board clocks.

The hardware implementation is in `ip/freq_meter/freq_meter.sv`, and the C++ interface is in `c++/freq_meter.hh`.

For a maintainer-oriented RTL map, see `ip/freq_meter/README.md`.

For maintainers, the most important split is:

* `ip/freq_meter/freq_meter.sv` owns the CDC-heavy hardware measurement pipeline
* `c++/freq_meter.hh` owns gate configuration, Hz conversion, and PulsePins-specific channel naming

### Hardware model

The current block is an Avalon-MM controlled frequency meter with:

* `N_CH = 4` channels by default
* a programmable gate length in reference-clock cycles
* per-channel sampled results exposed through a small register file

The design counts transitions in each input clock domain and transfers per-channel counts into the gate/reference domain using [Gray-coded](https://en.wikipedia.org/wiki/Gray_code) counters and synchronization stages. A Gray-coded counter changes only one bit per increment, which makes it suitable for sampling a changing count across a [CDC](clock_domain.md#cdc-background-terms) boundary.

The measurement path is intentionally CDC-heavy because each observed signal is itself a running clock. Rather than sampling those clocks directly in the Avalon domain, the hardware increments a per-channel counter in the input-clock domain, converts it to Gray code, synchronizes that Gray value into the gate/reference clock domain, and computes a delta at the end of each gate interval.

One important consequence is that the reported values are per-gate edge counts, not continuously updated instantaneous frequencies. The host-side wrapper is responsible for turning those per-gate counts into Hz.

For gate-update and CDC latency notes, see [RTL latency and timing](latency.md).

### Register model

The key register groups are:

* control register - enable and clear
* gate length register - measurement window in reference-clock cycles
* number-of-channels register
* one result register per channel

More concretely, the current word offsets are:

* `0x00` - control (`enable`, `clear`)
* `0x04` - gate length
* `0x08` - number of channels
* `0x10` and up - per-channel results

The `clear` control behaves like a pulse request, while `enable` keeps periodic measurements running.

The C++ wrapper treats the gate length as the main measurement configuration knob.

### C++ interface

The `freq_meter` class provides:

* `set_gate_len()` - configure the measurement window in cycles
* `set_gate_time()` - configure the same window in seconds
* `read()` - read the raw result counter
* `read_freq()` - convert the raw count to Hz
* `read_freq_str()` - return a formatted frequency string
* `wait_one_gate_time()` - wait for one fresh measurement interval

The class also keeps track of:

* the nominal counter clock frequency used for conversion to Hz
* an optional correction factor for calibration
* the gate length currently programmed into the hardware

The higher-level `pp_freq_meter` wrapper additionally:

* applies an optional correction factor
* reads all four standard channels
* stores the measured streamer clock in the `FPGA` object

That last step is important because other software layers may rely on the measured streamer clock when converting counts to time-based values.

The shared executable bootstrap in `c++/host_runtime.hh` constructs this wrapper during startup and reports the initial frequencies before command execution begins.

The user-facing command implementation lives in `c++/pptool_measurement.cc`.

### Standard channels

The current C++ layer names the four channels as:

* external clock
* internal clock
* streamer clock
* core clock

The wrapper currently expects all four channels to be present.

The standard channel IDs in `c++/freq_meter.hh` are:

* `0` - external clock
* `1` - internal clock
* `2` - streamer clock
* `3` - core clock

### Tool integration

`ppfreq` is the user-facing command-line interface to this subsystem.

It can be configured either by:

* gate time in seconds, or
* gate length in clock cycles

If both are available conceptually, gate time is the more user-friendly view and gate length is the lower-level hardware-facing view.

Longer gate intervals improve frequency resolution but reduce update rate. Shorter gate intervals provide faster feedback but coarser measurements.

### Typical use cases

Use the frequency meter for:

* verifying external clock presence
* checking PLL and streamer clock configuration
* measuring startup or runtime clock conditions before streaming
* calibrating clock scaling using the optional correction factor

The correction factor is especially useful when the nominal reference clock is known to differ slightly from its ideal value.

### Related pages

* `ppfreq.md`
* `cpp.md`
* `build.md`
