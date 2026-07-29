# Counter subsystem

The counter subsystem provides on-chip measurement blocks for statistics, sequence analysis, and timing analysis of streamed or externally supplied digital signals.

The main integration block is [`ip/counter/counter_if.sv`]({{ source_file("ip/counter/counter_if.sv") }}), with the software interface in [`c++/counter.hh`]({{ source_file("c++/counter.hh") }}).

For a maintainer-oriented map of the RTL directory, see [`ip/counter/README.md`]({{ source_file("ip/counter/README.md") }}).

At a high level, `counter_if` acts as a measurement backplane: one Avalon-MM slave selects an instrument, chooses channel indices, optionally latches or resets all counters, and returns the selected result word.

### Main RTL blocks

[`ip/counter/counter_if.sv`]({{ source_file("ip/counter/counter_if.sv") }}) multiplexes selected input channels into several instruments and exposes them through a single Avalon-MM control/readout interface.

The main instruments are:

* [`basic_counter.sv`]({{ source_file("ip/counter/basic_counter.sv") }}) - totals, low/high occupancy, and edge counts
* [`runs_counter.sv`]({{ source_file("ip/counter/runs_counter.sv") }}) - run-length statistics for low and high runs
* [`packet_stats.sv`]({{ source_file("ip/counter/packet_stats.sv") }}) - valid/idle accounting and packet-length statistics
* [`time_counter.sv`]({{ source_file("ip/counter/time_counter.sv") }}) - elapsed-time capture between asynchronous start/stop events
* [`seq_counter.sv`]({{ source_file("ip/counter/seq_counter.sv") }}) - short-sequence histogramming
* [`autocorrelation.sv`]({{ source_file("ip/counter/autocorrelation.sv") }}) - short-depth autocorrelation counters
* [`crosscorrelation.sv`]({{ source_file("ip/counter/crosscorrelation.sv") }}) - optional correlation between two selected channels when `COUNTER_CC` is enabled

The wrapper is intentionally backplane-like: the instruments run in parallel, but software sees them through one compact selector-based Avalon-MM interface instead of through separate bus wrappers.

The instrument selector in [`counter_if.sv`]({{ source_file("ip/counter/counter_if.sv") }}) maps these numeric IDs:

| ID | Instrument |
| -- | ---------- |
| `1` | basic counter |
| `2` | runs counter |
| `3` | short-sequence counter |
| `5` | packet statistics |
| `6` | autocorrelation |
| `7` | crosscorrelation, if `COUNTER_CC` is enabled |
| `8` | time counter 1 |
| `9` | time counter 2 |

### Data and clock domains

The subsystem explicitly bridges between:

* a system/control clock domain (`clk`)
* a data clock domain (`d_clk`)

[`counter_if.sv`]({{ source_file("ip/counter/counter_if.sv") }}) synchronizes global reset and latch operations into the data domain before they are consumed by the individual counters. Channel selector updates also cross into the data domain through a coherent latest-value CDC update before they drive the selected-channel muxes.

This matters because the counters are designed to inspect signals that may be produced by the streamer clock rather than the Avalon control clock.

The interface also supports three channel selectors:

* `sel0` - main single-bit channel used by `basic_counter`, `runs_counter`, `packet_stats`, `seq_counter`, and `autocorrelation`
* `sel1`, `sel2` - second pair of selected channels used for optional two-input measurements such as `crosscorrelation` and for asynchronous timing capture

For timing measurements, `counter_if` synchronizes the selected channel levels into the system clock domain before edge detection, then feeds the resulting start/stop pulses into two `time_counter` instances.

For counter CDC and readout latency notes, see [RTL latency and timing](latency.md).

### Avalon-MM programming model

The write-side control registers in [`counter_if.sv`]({{ source_file("ip/counter/counter_if.sv") }}) are:

| Address | Meaning |
| ------- | ------- |
| `1` | instrument number |
| `2` | select low/high view bit |
| `3` | per-instrument result address |
| `4` | `sel0` |
| `5` | `sel1` |
| `6` | `sel2` |
| `7` | `{ latch_all, reset_all }` |

The read-side view is simpler:

| Address | Meaning |
| ------- | ------- |
| `0` | selected result word |
| `1` | overflow flag from `basic_counter` |
| `2` | overflow flag from `packet_stats` |
| `3` | ready flags from the two time counters |

The `high_low` control bit is reused by several instruments to switch between statistics for low and high states.

### C++ interface

The `counter` class in [`c++/counter.hh`]({{ source_file("c++/counter.hh") }}) groups the hardware instruments into convenient sub-objects:

* `bc` - `basic_counter`
* `rc` - `runs_counter`
* `ps` - `packet_stats`
* `sc` - `seq_counter`
* `ac` - `autocorrelation`
* `cc` - `crosscorrelation` helper for builds that enable `COUNTER_CC`

Important operations:

* `reset_all()` - synchronously reset all instruments
* `latch_all()` - latch current statistics into a consistent snapshot for subsequent readout
* `report()` - print a full multi-instrument report
* `short_report()` - print a smaller report focused on basic/run statistics

The wrapper also embeds typed helper objects for each instrument. Those helpers know how to turn low/high words and instrument-local addresses into usable 64-bit statistics.

The default control-software usage pattern is:

1. choose observed channels
2. optionally reset the instrument bank
3. let the experiment or test waveform run
4. latch all counters
5. read the desired instrument reports

### What each instrument measures

`basic_counter` reports:

* total sampled ticks
* number of low samples
* number of high samples
* low-to-high transitions
* high-to-low transitions

`runs_counter` reports:

* number of runs
* run counts for low and high levels
* total run lengths
* maximum run lengths
* glitch counts

In practice this block is useful for questions like “how long do high pulses usually last?” or “how often do very short glitches occur?”.

`packet_stats` reports:

* total ticks
* valid ticks
* idle ticks
* packet begin/end counts
* accumulated packet length statistics

The packet-statistics block is useful for streams that carry an explicit valid/idle notion through `d_valid`. A packet is defined purely by that validity signal: an assertion starts a packet and a deassertion ends it.

`seq_counter` builds a histogram of short bit patterns. In the integrated design it is configured for 4-bit sequences, which is why the C++ wrapper prints 16 bins.

The implementation supports both overlapping and non-overlapping windows, although the integrated instance in [`counter_if.sv`]({{ source_file("ip/counter/counter_if.sv") }}) uses non-overlapping windows.

`autocorrelation` exposes compact lag-based correlation counts. In the integrated design, [`counter_if.sv`]({{ source_file("ip/counter/counter_if.sv") }}) sets `c_len = 3`, so address 0 reports the total number of valid samples and addresses 1 through 3 report lag-1 through lag-3 match counts. The standalone RTL modules are parameterized for other depths, but the current PulsePins integration exposes this short depth. If `c_len` changes, update the matching constants in [`c++/counter.hh`]({{ source_file("c++/counter.hh") }}) and the maintenance note in [`ip/counter/README.md`]({{ source_file("ip/counter/README.md") }}). `crosscorrelation` follows the same address pattern only in builds that enable the `COUNTER_CC` integration path.

The time-counter path is slightly different from the other instruments: it measures elapsed system-clock ticks between start and stop edges derived from selected channels, and exposes ready flags separately.

### Reading workflow

A typical measurement cycle is:

1. choose the observed channel(s)
2. optionally `reset_all()` before a new experiment
3. run the waveform or observe external signals
4. `latch_all()` to freeze a consistent snapshot
5. read the desired instrument report

This latch-then-read model is important because several counters are continuously updated in the data clock domain.

### Instrument summary

| Block | Main question it answers |
| ----- | ------------------------ |
| `basic_counter` | how much time was the signal high/low, and how many edges occurred? |
| `runs_counter` | how long are runs, and how often do glitches appear? |
| `packet_stats` | how much valid traffic exists, and what are the packet lengths? |
| `seq_counter` | which short bit patterns occur most often? |
| `autocorrelation` | how strongly does the signal correlate with delayed versions of itself? |
| `crosscorrelation` | how strongly do two selected channels correlate, if `COUNTER_CC` is enabled? |
| `time_counter` | how many system-clock ticks elapsed between start and stop events? |

### Tool integration

`ppcounter` is the main command-line interface for this subsystem.

It can:

* generate built-in test sequences
* stream them through the normal output path
* latch and report the resulting statistics
* verify a known-good reference case with `-check`

The built-in `-test1` path uses a short deterministic sequence from [`c++/counter.hh`]({{ source_file("c++/counter.hh") }}), while `-test2` can drive a longer pseudo-random sequence.

`-check` validates the deterministic reference case by comparing several instrument outputs against expected values.

### Typical use cases

Use the counter subsystem for:

* validating streamer output activity
* measuring duty cycle or burst structure
* characterizing random or pseudo-random output streams
* debugging packetized or valid-gated signal sources
* timing external event relationships using the time-counter path

### Related pages

* [ppcounter](ppcounter.md)
* [Testing procedures](testing.md)
* [C++ application programming interface](cpp.md)
