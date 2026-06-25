# ppfreq

`ppfreq` reads the on-board frequency-meter block and prints measured frequencies periodically.

For the underlying measurement block and API, see `freq_meter.md`.

The tool creates the shared `pp_freq_meter` wrapper, optionally reprograms the gate settings, waits one full measurement interval, and then prints formatted readings in a loop.

It uses the interface in [`c++/freq_meter.hh`]({{ source_file("c++/freq_meter.hh") }}), and the command implementation lives in [`c++/pptool_measurement.cc`]({{ source_file("c++/pptool_measurement.cc") }}).

Common options:

* `-gate_time T`: set the measurement gate time, for example `1s`
* `-gate_len N`: set the measurement gate length directly in clock cycles
* `-nr N`: number of measurements to print; `0` means run continuously
* `-freq_rescale X`: apply correction factor `X` to the reported frequencies; can also be set with `PP_FREQ_RESCALE`

Gate configuration follows this order:

* if `-gate_time` is present, it is used
* otherwise `-gate_len` is used, defaulting to `500000`

The command uses a fixed output format equivalent to `%t %e`, so by default it prints a wall-clock timestamp plus the external-clock reading. The formatter infrastructure also has access to the internal and streamer channels.

### Displayed channels

The output format prints:

* wall-clock timestamp
* external clock frequency

The formatter also has access to internal and streamer clock channels, but the default format shows only the timestamp and the external clock reading.

Output behavior:

* each printed line includes a wall-clock timestamp and the formatted frequency reading
* the command reports the primary displayed channel through the shared frequency-meter wrapper

Longer gate settings give more stable readings, while shorter gate settings give faster updates.

The formatted output includes a timestamp and the measured frequency string.

### Typical examples

Measure using a 1-second gate:

```bash
ppfreq -gate_time 1s
```

Measure using a raw gate length of 1,000,000 counter-clock cycles:

```bash
ppfreq -gate_len 1000000
```

Print 20 measurements and stop:

```bash
ppfreq -nr 20
```

Apply a correction factor:

```bash
ppfreq -gate_time 1s -freq_rescale 0.99995
```
