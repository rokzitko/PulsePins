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
* `-format F`: output format string, default `%t %e`

Gate configuration follows this order:

* if `-gate_time` is present, it is used
* otherwise `-gate_len` is used, defaulting to `500000`

The command uses an output format string. By default it uses `%t %e`, so it prints a wall-clock timestamp plus the external-clock reading.

### Displayed channels

The default output format prints:

* wall-clock timestamp
* external clock frequency

Format tokens:

| Token | Meaning |
| --- | --- |
| `%t` | wall-clock timestamp |
| `%c` | sample counter |
| `%e` | external clock frequency |
| `%i` | internal clock frequency |
| `%s` | selected streamer clock frequency |

Output behavior:

* each printed line is rendered from the selected output format
* frequency values use the same formatting as the shared frequency-meter wrapper

Longer gate settings give more stable readings, while shorter gate settings give faster updates.

Use `%%` in the format string for a literal percent sign.

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

Show all common clock channels:

```bash
ppfreq -gate_time 1s -format "%t ext=%e int=%i streamer=%s"
```
