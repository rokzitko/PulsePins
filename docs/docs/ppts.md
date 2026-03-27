## ppts

`ppts` reads timestamp events from the `ts_core` hardware block.

For the underlying capture architecture, see `timestamp.md`.

The tool resets the FPGA fabric, configures the timestamp-routing PIO, and then starts one reader thread per enabled timestamp stream.

### Stream selection

By default `ppts` reads the PPS stream.

Use these switches to control which streams are enabled:

* `-nopps`: disable PPS capture entirely
* `-sigA`: enable the secondary `sigA` stream

This means the most common modes are:

* PPS-only capture
* `sigA`-only capture using `-nopps -sigA`
* simultaneous PPS + `sigA` capture using `-sigA`

It can monitor the PPS input, the auxiliary `sigA` input, or both. The implementation lives in `c++/pptool.cc` and
uses the timestamp interface from `c++/timestamp.hh`.

Common options:

* `-nopps`: disable PPS capture
* `-sigA`: enable capture on the `sigA` path
* `-selA N`: select the source routed to `sigA`
* `-pps_in`: use the external PPS input
* `-pps_xtal`: use the crystal-derived PPS source
* `-timeout T`: timeout for waiting on new timestamp samples
* `-nr N`: stop after a limited number of samples; `0` means run continuously

The `-nr` limit is applied per active stream reader.

### Output format

The current implementation prints lines of the form:

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

### Typical examples

Read PPS timestamps continuously:

```bash
ppts
```

Read only the `sigA` stream from selector `3` with a timeout:

```bash
ppts -nopps -sigA -selA 3 -timeout 2
```

Read both streams and stop after 10 samples per stream:

```bash
ppts -sigA -nr 10
```

This tool is especially useful for checking that an event source is present, estimating event periods from timestamp differences, and validating selector routing before using higher-level timing tools.
