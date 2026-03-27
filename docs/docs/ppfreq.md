## ppfreq

`ppfreq` reads the on-board frequency-meter block and prints measured frequencies periodically.

For the underlying measurement block and API, see `freq_meter.md`.

The tool creates the shared `pp_freq_meter` wrapper, optionally reprograms the gate settings, waits one full measurement interval, and then prints formatted readings in a loop.

It is implemented in `c++/pptool.cc` and uses the interface in `c++/freq_meter.hh`.

Common options:

* `-gate_time T`: set the measurement gate time, for example `1s`
* `-gate_len N`: set the measurement gate length directly in clock cycles
* `-nr N`: number of measurements to print; `0` means run continuously

The current implementation chooses gate configuration this way:

* if `-gate_time` is present, it is used
* otherwise `-gate_len` is used, defaulting to `500000`

### Displayed channels

The current output format prints:

* wall-clock timestamp
* external clock frequency

Internally the formatter also has access to internal and streamer clock channels, but the default format currently shows only the timestamp and the external clock reading.

Output behavior:

* each printed line includes a wall-clock timestamp and the formatted frequency reading
* the default implementation reports the primary displayed channel through the shared frequency-meter wrapper

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
