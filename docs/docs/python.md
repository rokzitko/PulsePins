# Python bindings

PulsePins has two Python interfaces:

* `pulsepins` - a pure-Python [SCPI](https://www.ivifoundation.org/About-IVI/scpi.html) client and Timeline authoring helper for scripts and Jupyter notebooks running on a workstation and talking to a board running `ppscpi`
* `pp` - a board-native nanobind extension module for the underlying C++ interface

## Workstation SCPI client

The workstation client lives in [`python/pulsepins/`]({{ source_file("python/pulsepins/") }}) and has no dependency beyond the Python standard library. It is the recommended Python interface for notebooks running on a laptop or workstation while the DE10-Nano runs `ppscpi`.

SCPI is here used as a lightweight ASCII command protocol over TCP for PulsePins-specific commands rather than as a complete SCPI instrument-class implementation.

From a repository checkout, either set `PYTHONPATH`:

```bash
export PYTHONPATH=/path/to/PulsePins/python
```

or install the pure-Python client package in editable mode:

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

The client exposes `idn()`, `reset()`, `clear_status()`, `streamer_clock_hz()`, `timeline(...)`, `load_sequence(...)`, `load(...)`, `stream()`, `run(...)`, `test1()`, `check(...)`, `check_enabled()`, `system_error()`, and `errors()`. `load_sequence(...)` accepts the [PulsePins text sequence format](sequence_format.md) and flattens multiline text into one `SEQ ...` command, so the uploaded command must fit within the `ppscpi` 64 KiB SCPI line limit. The workstation package transports text through SCPI; it does not expose the board-native C++ `Sequence` binary and VCD serializers.

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

In a notebook, evaluating `timeline` renders an SVG preview. `Timeline.to_sequence(...)` returns a generated waveform subset of the PulsePins text format for inspection or manual editing; pass `include_final=True` for upload-ready finite pulse programs that return owned channels to their resting value. `Timeline.to_csv()` / `Timeline.from_csv(...)` use the same `channel,bit,start,duration,color` pulse-table CSV format as the browser Timeline Composer. `Timeline.to_draft_json()` / `Timeline.from_draft_json(...)` use the browser draft JSON format. `Timeline.to_vcd(...)` exports a scalar VCD waveform preview for waveform viewers. Same-channel overlapping pulses are rejected; adjacent pulses are allowed.

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

Live Timeline stream/sweep commands query `CLOCK:STREAMER?` before converting absolute-time pulses; pass `--clock-hz` only when you need to override the board-reported clock. Offline preview and sweep `--dry-run` use the supplied/default dry-run clock.

## Board-native bindings

PulsePins uses [nanobind](https://nanobind.readthedocs.io/en/latest/) to provide Python bindings for the underlying C++ interface.

The board-native `pp` / `pp_impl` bindings are intended for trusted board-local code. Accounts that can run these bindings should be treated as privileged/root-equivalent: the low-level hardware bindings expose `/dev/mem`-backed MMIO access, and the DMA helpers accept raw physical addresses/descriptors. Use the workstation `pulsepins` SCPI client with a controlled `ppscpi` service for non-root, remote, or less-trusted workflows.

The Python binding tree lives in [`python/`]({{ source_file("python/") }}) and builds two modules:

* `pp`
* `pp_impl`

At the sequence-serialization level, the board-native `pp` module exposes the C++ formats below; these APIs are not part of the workstation `pulsepins` SCPI/Timeline package:

* [PulsePins text sequence format](sequence_format.md) via `parse_sequence_text(...)` and `write_sequence_text(...)`
* VCD regular-record projection import/export via `Sequence.load_VCD(...)` and `Sequence.write_VCD_file(...)`
* current-build binary sequence snapshots via `read_sequence_binary(...)` and `Sequence.write_binary_file(...)`

Text helpers round-trip only the parser-representable normalized subset. The writer preserves represented fields, but not arbitrary raw control or payload fields; raw retrigger payloads are one omitted case, and the separate force-trigger Boolean is emitted only when requested. The binary snapshot preserves normalized current-width element triplets and the supplied force-trigger flag, but loading requires matching control, count, value, and trigger widths, while execution context remains external. `Sequence.write_VCD_file(...)` rejects non-regular elements, but its flattened projection does not execute store semantics, evaluates zero-count operations before dropping their runs, and evaluates relative operations from initial value zero. Use emitted, positive-count BITLOAD records, or evaluate relative operations against the actual runtime initial state first, for a hardware-aligned export. The underlying C++ VCD export default uses `$timescale 10ns`, matching the VCD import default of a 10 ns PulsePins output period.

## Supported build modes

There are two practical ways to build/test the Python bindings:

* build on the DE10-Nano board - this is the supported production path
* build on a development machine - useful for syntax/import/API testing only

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

## Testing on a development machine

Builds on a development machine are useful for checking that the binding code compiles and imports cleanly.

The recommended command for this build is:

```bash
make -C python USE_PREGENERATED=1 build test-host
```

`USE_PREGENERATED=1` uses the checked-in [`c++/artifacts/hps_0.h`]({{ source_file("c++/artifacts/hps_0.h") }}) header instead of the top-level generated `hps_0.h`, which is ignored and normally produced by the Quartus/Qsys hardware build for compilation. `test-host` intentionally skips tests marked `hardware`, which require `/dev/mem`, board-backed MMIO, or a live PulsePins runtime.

## Sequence I/O examples

```python
import pp

seq, force_trigger = pp.parse_sequence_text("d 3 0x12\n")
text = pp.write_sequence_text(seq)
seq.write_VCD_file("capture.vcd")
seq.write_binary_file("capture.ppbin")
seq2, force_trigger2 = pp.read_sequence_binary("capture.ppbin")
```

The VCD example intentionally contains only an emitted, positive-count regular BITLOAD record. `write_VCD_file(...)` rejects terminal `final`, trigger, replay, retrigger, and pseudo-random elements.

## Sequence element API

Python exposes the same sequence-element model as the C++ layer. The supported construction paths follow the flattened `el` design and do not include the raw `(el_type, Counter, Value, control)` constructor.

Supported `pp.el(...)` constructors include:

* `pp.el()` - final element with the default final output value
* `pp.el(value)` - final element with an explicit final output value
* `pp.el(count, value)` - regular `BITLOAD` element
* `pp.el(counter, value)` - regular `BITLOAD` element with an explicit `Counter` object
* `pp.el(counter, value_wrapper)` - regular element with explicit `Counter` and `Value` wrapper semantics
* `pp.el(pattern, mask, final)` - trigger element
* `pp.el(pp.Replay(), repetitions, length)` - replay element; `length` must not exceed the fast-memory depth (`POSITIONS`)
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

Bindings that wrap MMIO-backed hardware objects own long-lived board resources. The module keeps the immediate `mm`/`FPGA` constructor arguments alive for the wrapper object. `loc` objects returned by `mm.get_loc(...)` also keep their source `mm` mapping alive, so `loc = pp.mm(...).get_loc(...)` remains valid as long as `loc` is alive.

The board-native API exposes the structured C++ option types used by clock and trigger helpers. Use `pp.PllOptions()` with `profile`, `charge_pump`, and `bandwidth` for `pll_core_clk.set_core_clk(...)` and `pll_int_clk.set_int_clk(...)`. Use `pp.TriggerOptions()` with `mode`, invert fields, and mask fields for `trigger(...)` and `trigger.set(...)`; `mode` values come from `pp.TriggerModeOption`, for example `pp.TriggerModeOption.external`. Optional fields use `None` when unset.

Python method defaults match the C++ defaults for `loc.read(...)`, `loc.write(...)`, `mm.get_ptr(...)`, `mm.get_loc(...)`, `streamer_fifo.out(...)`, and `streamer_dma.send_sequence(...)`.

The Python bindings expose the shared C++ sequence-preparation helpers as `pp.prepare_sequence_for_streaming(...)` and `pp.prepare_sequence_for_readback_check(...)`. These are the preferred helpers when Python code needs the same appended-final, inferred-final, and normalized readback-reference behavior as the C++ streaming workflow. `streamer_control.trigger_clear()` is also available for forced-trigger cleanup.

The small helper scripts in [`python/pptool.py`]({{ source_file("python/pptool.py") }}) and [`tests/test2.py`]({{ source_file("tests/test2.py") }}) use the same conservative defaults as the shared C++ workflow: 2 s readback timeout protection and a 10 s streamer-completion timeout, with `timeout=0` disabling the readback timeout.

## Testing expectations

`make -C python test` runs the Python test files listed in [`python/Makefile`]({{ source_file("python/Makefile") }}): [`test.py`]({{ source_file("python/test.py") }}), [`test_cli.py`]({{ source_file("python/test_cli.py") }}), [`test_scpi_client.py`]({{ source_file("python/test_scpi_client.py") }}), and [`test_timeline.py`]({{ source_file("python/test_timeline.py") }}).

Some Python tests exercise board-backed MMIO/FPGA behavior, so they should not be treated as a
strictly hardware-independent test battery.

See also:

* [`python/README`]({{ source_file("python/README") }})
* [`python/README.devel`]({{ source_file("python/README.devel") }})
* [Build and deployment](build.md)
