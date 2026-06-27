# Python bindings

PulsePins has two Python interfaces:

* `pulsepins` - a pure-Python, host-side [SCPI](https://www.ivifoundation.org/About-IVI/scpi.html) client for scripts and Jupyter notebooks talking to a board running `ppscpi`
* `pp` - nanobind extension module for the underlying C++ interface

## Host-side SCPI client

The host-side client lives in [`python/pulsepins/`]({{ source_file("python/pulsepins/") }}) and has no dependency beyond the Python standard library. It is the recommended Python entry point for notebooks running on a laptop or workstation while the DE10-Nano runs `ppscpi`.

SCPI is here used as a lightweight ASCII command protocol over TCP for PulsePins-specific commands rather than as a complete SCPI instrument-class implementation.

From a repository checkout, either set `PYTHONPATH`:

```bash
export PYTHONPATH=/path/to/PulsePins/python
```

or install the pure-Python host package in editable mode:

```bash
python3 -m pip install -e /path/to/PulsePins/python
```

The editable install also provides small example commands: `pulsepins-ppscpi-check`, `pulsepins-ppscpi-hello`, `pulsepins-notebook-workflow`, `pulsepins-timeline-preview`, `pulsepins-timeline-stream`, and `pulsepins-timeline-sweep`. Use `pulsepins-ppscpi-check --self-test` when you want the connectivity check to also run the built-in `TEST1` hardware smoke path.

Minimal example:

```python
from pulsepins import PulsePins

with PulsePins("de10nano") as pp:
    print(pp.idn())
    pp.reset()
    pp.load_sequence("""
    d 10 0xff
    d 5 0x00
    f
    """)
    pp.stream()
```

The `f` line requests forced triggering; it does not choose the final output value. If no `final ...` line is present, `STREAM` appends a no-modify final terminator and leaves the outputs at the last sequence value.

The client exposes `idn()`, `reset()`, `clear_status()`, `streamer_clock_hz()`, `timeline(...)`, `load_sequence(...)`, `load(...)`, `stream()`, `run(...)`, `test1()`, `check(...)`, `check_enabled()`, `system_error()`, and `errors()`. `load_sequence(...)` flattens multiline sequence text into one `SEQ ...` command, so the uploaded command must fit within the `ppscpi` 64 KiB SCPI line limit.

The same package also includes a `Timeline` builder for simple named-channel pulse programs:

```python
from pulsepins import PulsePins

with PulsePins("de10nano") as pp:
    timeline = pp.timeline(unit="us")
    timeline.channel("laser", bit=0)
    timeline.channel("camera", bit=1)
    timeline.pulse("laser", start=10, duration=5)
    timeline.pulse("camera", start=20, duration=10)
    pp.reset()
    pp.run(timeline, force_trigger=True, include_final=True)

timeline
```

In a notebook, evaluating `timeline` renders an SVG preview. `Timeline.to_sequence(...)` returns the generated PulsePins text sequence for inspection or manual editing; pass `include_final=True` for upload-ready finite pulse programs that return owned channels to their resting value. `Timeline.to_csv()` / `Timeline.from_csv(...)` use the same `channel,bit,start,duration,color` pulse-table CSV format as the browser Timeline Composer. `Timeline.to_draft_json()` / `Timeline.from_draft_json(...)` use the browser draft JSON format. `Timeline.to_vcd(...)` exports a scalar [Value Change Dump (VCD)](cpp.md#data-types-for-sequence-representation) trace for waveform viewers. Same-channel overlapping pulses are rejected; adjacent pulses are allowed.

Runnable examples:

```bash
PYTHONPATH=python python3 python/examples/timeline_preview.py --svg timeline.svg --csv timeline.csv --draft timeline.json --vcd timeline.vcd
PYTHONPATH=python python3 python/examples/timeline_stream.py de10nano --print-sequence
PYTHONPATH=python python3 python/examples/timeline_sweep.py de10nano --delays-us 0 5 10
PYTHONPATH=python python3 python/examples/notebook_workflow.py --output-dir previews
```

After `python3 -m pip install -e python`, the same workflows can be run as:

```bash
pulsepins-ppscpi-check de10nano --self-test
pulsepins-notebook-workflow de10nano --output-dir previews --run
pulsepins-timeline-preview --svg timeline.svg --csv timeline.csv --draft timeline.json --vcd timeline.vcd
pulsepins-timeline-stream de10nano --print-sequence
pulsepins-timeline-sweep de10nano --delays-us 0 5 10
```

## Board-native bindings

PulsePins uses [nanobind](https://nanobind.readthedocs.io/en/latest/) to provide Python bindings for the underlying C++ interface.

The Python binding tree lives in [`python/`]({{ source_file("python/") }}) and builds two modules:

* `pp`
* `pp_impl`

At the sequence-serialization level, Python exposes the same practical formats as the core C++ layer:

* PulsePins text sequence format via `parse_sequence_text(...)` and `write_sequence_text(...)`
* VCD import/export via `Sequence.load_VCD(...)` and `Sequence.write_VCD_file(...)`
* exact binary sequence import/export via `read_sequence_binary(...)` and `Sequence.write_binary_file(...)`

Text and binary sequence helpers preserve explicit `final`, trigger, replay, retrigger, and pseudo-random records exactly. VCD export is narrower: `Sequence.write_VCD_file(...)` only accepts deterministic regular sequences and rejects trigger/final/control-flow elements. The underlying C++ VCD export default uses `$timescale 10ns`, matching the VCD import default of a 10 ns PulsePins output period.

## Supported build modes

There are two practical ways to build/test the Python bindings:

* build on the DE10-Nano board - this is the supported production path
* build on a host machine - useful for syntax/import/API testing only

True cross-compilation of the Python bindings is not supported.

## Board build

On the DE10-Nano, the normal workflow is:

```bash
cd python
python3 -m pip install pytest nanobind
make
make test
```

The default build uses `-O2` and omits `-g` to reduce memory pressure on the board.
If you need debug symbols while developing the bindings, use:

```bash
make PY_DEBUG=1
```

## Host-side testing

Host-side builds are useful for checking that the binding code compiles and imports cleanly.

The recommended host-side command is:

```bash
make -C python USE_PREGENERATED=1 build test-host
```

`USE_PREGENERATED=1` uses the checked-in [`c++/artifacts/hps_0.h`]({{ source_file("c++/artifacts/hps_0.h") }}) header instead of the top-level generated [`hps_0.h`]({{ source_file("hps_0.h") }}), which is ignored and normally produced by the Quartus/Qsys hardware build. `test-host` intentionally skips tests marked `hardware`, which require `/dev/mem`, board-backed MMIO, or a live PulsePins runtime.

## Sequence I/O examples

```python
import pp

seq, force_trigger = pp.parse_sequence_text("d 3 0x12\nfinal 0x34\n")
text = pp.write_sequence_text(seq)
seq.write_VCD_file("capture.vcd")
seq.write_binary_file("capture.ppbin")
seq2, force_trigger2 = pp.read_sequence_binary("capture.ppbin")
```

## Sequence element API

Python exposes the same sequence-element model as the C++ layer. The supported construction paths follow the flattened `el` design and do not include the raw `(el_type, Counter, Value, control)` constructor.

Supported `pp.el(...)` constructors include:

* `pp.el()` - final element with the default final output value
* `pp.el(value)` - final element with an explicit final output value
* `pp.el(count, value)` - regular `BITLOAD` element
* `pp.el(counter, value)` - regular `BITLOAD` element with explicit `Counter` policy
* `pp.el(counter, value_wrapper)` - regular element with explicit `Counter` and `Value` wrapper semantics
* `pp.el(pattern, mask, final)` - trigger element
* `pp.el(pp.Replay(), repetitions, length)` - replay element
* `pp.el(pp.Retrig(), value=...)` - retrigger element
* `pp.el(pp.PseudoRandom(), count)` - pseudo-random element

Useful element inspectors and helpers exposed in Python:

* `kind()`, `mode()`, `no_strobe()`
* `is_stored()`, `store_slot()`, `stored_in(...)`
* `trigger_pattern()`, `trigger_mask()`, `trigger_is_final()`
* `regular_token()`, `sequence_record()`
* `with_control(...)`, `with_count(...)`, `with_counter(...)`, `with_regular_value(...)`, `as_bitload_after(...)`

The mutating methods `store(...)`, `set_control(...)`, `set_count(...)`, and `set_value(...)` are also available. Prefer the immutable helpers above when constructing transformed elements.

Static reconstruction helpers are also bound:

* `pp.el.classify_control(...)`
* `pp.el.from_raw_triplet(...)`
* `pp.el.from_regular_token(...)`
* `pp.el.is_regular_token(...)`

Example:

```python
import pp
import pp_impl

e = pp.el(pp.NoStrobe(3), pp.BitXor(0x12))
assert e.mode() == pp_impl.BITXOR

converted = e.as_bitload_after(0x01)
assert converted.regular_token() == "dn"
assert converted.sequence_record() == "dn 3 0x13"

decoded = pp.el.from_regular_token("xr", 7, 0x55)
assert decoded.sequence_record() == "xr 7 0x55"
```

Bindings that wrap MMIO-backed hardware objects own long-lived board resources. The module keeps the immediate `mm`/`FPGA` constructor arguments alive for the wrapper object.

The small helper scripts in [`python/pptool.py`]({{ source_file("python/pptool.py") }}) and [`tests/test2.py`]({{ source_file("tests/test2.py") }}) use the same conservative defaults as the shared C++ workflow: 2s readback timeout protection and a 10s streamer-completion timeout, with `timeout=0` disabling the readback timeout.

## Testing expectations

`make -C python test` runs the Python test files listed in [`python/Makefile`]({{ source_file("python/Makefile") }}): [`test.py`]({{ source_file("python/test.py") }}), [`test_cli.py`]({{ source_file("python/test_cli.py") }}), [`test_scpi_client.py`]({{ source_file("python/test_scpi_client.py") }}), and [`test_timeline.py`]({{ source_file("python/test_timeline.py") }}).

Some Python tests exercise board-backed MMIO/FPGA behavior, so they should not be treated as a
strictly hardware-free test battery.

See also:

* [`python/README`]({{ source_file("python/README") }})
* [`python/README.devel`]({{ source_file("python/README.devel") }})
* [Build and deployment](build.md)
