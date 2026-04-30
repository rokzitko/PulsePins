## Worked examples

This page collects concrete PulsePins workflows that are useful for first contact, lab bring-up, and contributor exploration. The goal is not to replace the per-command manual pages, but to show how the pieces fit together in practice.

Unless stated otherwise, these examples assume the standard DE10-Nano PulsePins runtime environment described in `INSTALL-quick_start.md` and `getting_started_hardware.md`.

### Example 1: Generate a continuous square wave with `ppfg`

Goal: make `qout[0]` toggle continuously without writing any sequence files.

Command:

```bash
ppfg -core_pll 10M -cont -trig -freq 10Hz -v1 0x1 -v0 0x0
```

What it does:

* configures the core clock for a simple 10 MHz working point
* auto-triggers (`-trig`) instead of waiting for an external trigger
* generates a continuous (`-cont`) square wave at 10 Hz
* drives `qout[0]` high in the `on` phase and low in the `off` phase

What to expect:

* on a scope or logic analyzer, `qout[0]` alternates between `0` and `1`
* with an LED attached to `qout[0]`, the output visibly blinks

Useful variations:

```bash
ppfg -core_pll 10M -cont -trig -period 1ms -duty 5 -v1 0x1 -v0 0x0
ppfg -core_pll 10M -burst 3 -period 1ms -trig -t 0x0 -v1 0x1 -v0 0x0
```

See also: `ppfg.md` and `recipes/ppfg`.

### Example 2: Use PulsePins as a triggered delay generator with `ppdelay`

Goal: wait for a trigger, then emit one pulse after a controlled delay.

Command:

```bash
ppdelay -veryverbose -trig_misc -core_pll 10M -duration 1ms -delay 20ms
```

What it does:

* waits for the trigger source selected by `-trig_misc`
* delays for 20 ms after the trigger event
* outputs a 1 ms pulse

What to expect:

* after the trigger event, the chosen output line goes high for 1 ms
* the pulse starts 20 ms after the trigger rather than immediately

This is a good starting point for camera triggering, shutter delays, and simple time-resolved experiments.

See also: `ppdelay.md` and `recipes/ppdelay`.

### Example 3: Capture a waveform and replay it exactly

Goal: use the readback path as a simple logic-analyzer capture, then replay the exact captured sequence.

Capture commands:

```bash
ppread -timeout 1 -save-vcd capture.vcd -save-text capture.seq -save-binary capture.ppbin
```

Replay commands:

```bash
ppplay -file capture.seq
ppplay -file capture.ppbin
```

What it does:

* `ppread` captures one second of readback data
* the same capture is exported in three formats:
  * `capture.vcd` for waveform viewing
  * `capture.seq` for editable text form
  * `capture.ppbin` for exact lossless replay
* `ppplay` then replays the saved sequence

When to use which format:

* use `capture.vcd` when you want to inspect timing visually
* use `capture.seq` when you want to edit the sequence by hand
* use `capture.ppbin` when you want exact replay without losing control-flow information

If you are capturing external signals rather than internally generated ones, use the normal `ppread` external-capture path (`-oe 0` / default hardware input configuration).

See also: `ppread.md`, `ppplay.md`, and `readback.md`.

### Example 4: Measure an external clock and inspect PPS timing

Goal: validate that an external reference is present and that PPS timing is sane.

Measure the external clock:

```bash
ppfreq -gate_time 1s
```

Inspect PPS timestamps:

```bash
ppts -nr 10
```

What to expect:

* `ppfreq` prints a timestamp plus the measured external-clock frequency
* `ppts` prints timestamp samples and the difference from the previous sample
* for a stable PPS source, the differences should cluster around one second in timestamp units

Useful variations:

```bash
ppfreq -gate_len 1000000
ppts -nopps -sigA -selA 3 -timeout 2
```

These tools are especially useful during board bring-up, external-clock validation, and troubleshooting trigger/timestamp routing.

See also: `ppfreq.md`, `ppts.md`, `freq_meter.md`, and `timestamp.md`.

### Example 5: Read the onboard temperature sensor on `PP_PMOD`

Goal: confirm that the optional `PP_PMOD` shield and its onboard `MCP9808` are working.

Command:

```bash
pptemp
```

What it does:

* reads the default `MCP9808` on I2C bus 1 at address `0x18`
* prints temperature samples continuously in human-readable form

What to expect:

* one temperature reading per second
* values track board temperature changes over time

This is a good sanity check for the PP_PMOD I2C path before attempting DAC or external Qwiic workflows.

See also: `pptemp.md`, `pp_pmod.md`, and `pp_pmod_reference.md`.

### Example 6: Generate an SPI/DDS programming sequence from host-side helper code

Goal: use the standalone sequence generators in `tools/spi_payload/` to control a peripheral from PulsePins.

For the AD9833 example:

```bash
make -C tools/spi_payload
./tools/spi_payload/spi_ad9833
scp tools/spi_payload/sequence de10nano:
ssh de10nano 'pptest 42 -f sequence -veryverbose'
```

What it does:

* builds the standalone SPI payload generators
* generates a PulsePins text sequence that programs an AD9833 for a 5 MHz sine output
* copies that sequence to the board
* plays it through the normal streaming path using `pptest`

When to use this workflow:

* bringing up SPI peripherals from PulsePins outputs
* validating board wiring and pin assignments
* prototyping peripheral-control sequences before integrating them elsewhere

The exact qout wiring and module-specific notes live in `tools/spi_payload/README`.

### Example 7: Drive `ppscpi` from a host-side Python notebook

Goal: keep Jupyter on a laptop or workstation while controlling a DE10-Nano over Ethernet.

On the board, start the SCPI server:

```bash
ppscpi
```

On the host, make the repository Python helpers importable:

```bash
export PYTHONPATH=/path/to/PulsePins/python
```

In Python or a notebook:

```python
from pulsepins import PulsePins

with PulsePins("de10nano") as pp:
    print(pp.idn())
    pp.reset()
    pp.load_sequence("""
    d 10 0xff
    d 5 0x00
    d 2 0b0101
    f
    """)
    pp.check(False)
    pp.stream()
```

The same minimal workflow is available as `python/examples/ppscpi_hello.py`:

```bash
PYTHONPATH=python python3 python/examples/ppscpi_hello.py de10nano
```

What it does:

* connects to TCP port `5025`
* uploads a PulsePins text sequence with `SEQ ...`
* requests forced triggering using the sequence's `f` record
* starts playback with `STREAM`

This workflow is the best first step for notebook integration. It avoids installing Jupyter on the board and keeps plotting, parameter sweeps, and data analysis on the host computer.

For named-channel pulse construction, use `Timeline`:

```python
from pulsepins import PulsePins, Timeline

timeline = Timeline(unit="us", clock_hz=100_000_000)
timeline.channel("laser", bit=0)
timeline.channel("camera", bit=1)
timeline.pulse("laser", start=10, duration=5)
timeline.pulse("camera", start=20, duration=10)

with PulsePins("de10nano") as pp:
    pp.reset()
    pp.run(timeline, force_trigger=True)
```

Two runnable Timeline examples are included:

```bash
PYTHONPATH=python python3 python/examples/timeline_preview.py --svg timeline.svg --csv timeline.csv
PYTHONPATH=python python3 python/examples/timeline_stream.py de10nano --print-sequence
PYTHONPATH=python python3 python/examples/timeline_sweep.py de10nano --delays-us 0 5 10
```

`timeline_preview.py` is hardware-free: it prints the generated text sequence and can write SVG plus browser-compatible CSV previews. `timeline_stream.py` uses the same timeline but uploads it to `ppscpi` and streams it with forced triggering. `timeline_sweep.py` shows the notebook-style pattern of rebuilding and streaming a timeline inside a parameter loop.

See also: `ppscpi.md` and `python.md`.

### Choosing the right kind of example

As a rule of thumb:

* use `ppfg` and `ppdelay` for immediate signal-generation tasks
* use `ppread` / `ppplay` when capture and replay matter more than manual signal description
* use `ppfreq` and `ppts` for validation of timing sources and timing observability
* use the `tools/` helpers when you want to prototype device-specific bus transactions or payload generation

### Contributions welcome

The examples above are intended as stable starting points, not an exhaustive catalog. High-value future additions would include:

* lasers / shutters / detectors
* synchronized instrument triggering
* DAC/DDS sweeps
* PMOD module bring-up notes
* logic-analyzer and scope screenshots for validated workflows

For board-specific workflows, especially around `PP_PMOD`, it is very helpful to record:

* exact wiring
* command lines
* expected output
* observed measurements
* board revision and optional populated parts
