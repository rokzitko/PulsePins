# ppts

`ppts` reads timestamp events from the `ts_core` hardware block. It is useful for checking that an event source is present, estimating event periods from timestamp differences, and validating selector routing before using higher-level timing tools.

For the underlying capture architecture, see [Timestamp capture](timestamp.md).

The tool uses the shared host startup path, configures the timestamp-routing PIO, clears the timestamp FIFOs and overflow state after routing is selected, and then starts one reader thread per enabled timestamp stream.

The implementation lives in [`c++/pptool_measurement.cc`]({{ source_file("c++/pptool_measurement.cc") }}) and uses the timestamp interface from [`c++/timestamp.hh`]({{ source_file("c++/timestamp.hh") }}).

### Syntax

```bash
ppts [options]
```

### Stream selection

By default `ppts` reads the (pulse per second) PPS stream.

Use these switches to control which streams are enabled:

* `-nopps`: disable PPS capture entirely
* `-sigA`: enable the secondary `sigA` stream

This means the most common modes are:

* PPS-only capture
* `sigA`-only capture using `-nopps -sigA`
* simultaneous PPS + `sigA` capture using `-sigA`

Common options:

* `-nopps`: disable PPS capture
* `-sigA`: enable capture on the `sigA` path
* `-selA N`: select the source routed to `sigA`
* `-pps_in`: use the external PPS input
* `-pps_xtal`: use the crystal-derived PPS source
* `-timeout T`: timeout for waiting on new timestamp samples
* `-nr N`: stop after a limited number of samples; `0` means run continuously
* `-ignore_ts_overflow`: continue reading even if `ts_core` reports timestamp overflow

The `-nr` limit is applied per active stream reader.

### `sigA` Sources

The `sigA` stream is a secondary timestamp path with a selectable source. The selector values are documented here; the lower-level routing model is described in [Timestamp capture](timestamp.md).

| `-selA` | Source | Typical Use |
|---:|---|---|
| `0` | Streamer trigger activated | Timestamp when the streamer trigger logic fires. |
| `1` | Streamer trigger input 0 | Observe trigger input 0 at the streamer trigger block. |
| `2` | External trigger input 0 | Observe external trigger input 0. |
| `3` | Auxiliary input 0 | Observe AUX input 0. |
| `4` | 1 s generated pulse | Sanity-check timestamp cadence against a slow internal source. |
| `5` | 100 ms generated pulse | Check 10 Hz internal timing. |
| `6` | 10 ms generated pulse | Check 100 Hz internal timing. |
| `7` | 1 ms generated pulse | Stress-test 1 kHz internal timing; use `-ignore_ts_overflow` when overflow is expected. |

If `-selA` is omitted, selector `0` is used.

### PPS Sources

The PPS path has a separate two-way source selector:

| Option | PPS Source |
|---|---|
| `-pps_xtal` | crystal-derived PPS source |
| `-pps_in` | external PPS input |

After reset, the hardware default is the crystal-derived PPS source. If both `-pps_in` and
`-pps_xtal` are omitted, `ppts` leaves the current PPS selector unchanged.

If both `-pps_in` and `-pps_xtal` are supplied, the later command-handler check selects `-pps_xtal` because `ppts` applies `-pps_xtal` after `-pps_in`.

### Output format

`ppts` prints lines of the form:

* stream label (`PPS` or `sigA`)
* wall-clock timestamp
* sample counter
* captured timestamp value
* difference from the previous sample on the same stream

The difference field is empty for the first sample and populated afterwards.

Output behavior:

* PPS samples are printed with the label `PPS`
* auxiliary samples are printed with the label `sigA`
* the default formatted output includes the sample counter, the timestamp value, and the difference from the previous sample

If both paths are enabled, the tool reads them concurrently using separate threads.

Because the two streams are read independently, the printed lines from PPS and `sigA` may interleave.

Because the hardware capture core has one pending slot per path and reports overflow if later events arrive before that slot is accepted, `ppts` is intended for sparse timing signals rather than dense pulse trains.

By default, timestamp overflow is reported as a command failure. Use `-ignore_ts_overflow` for deliberate stress tests or fast generated sources where dropped timestamp events are expected and the goal is to inspect the samples that software did receive.

Use [Timestamp capture](timestamp.md) for counter-width, edge-capture, FIFO, and timing-semantics details.

### Typical examples

Read PPS timestamps continuously:

```bash
ppts
```

Read only the `sigA` stream from selector `3` with a timeout:

```bash
ppts -nopps -sigA -selA 3 -timeout 2
```

Read only the `sigA` stream from the generated 1 s pulse:

```bash
ppts -nopps -sigA -selA 4 -nr 5
```

Read both streams and stop after 10 samples per stream:

```bash
ppts -sigA -nr 10
```

Read the fast 1 ms generated `sigA` source without failing on expected timestamp overflow:

```bash
ppts -nopps -sigA -selA 7 -nr 10 -timeout 10 -ignore_ts_overflow
```
