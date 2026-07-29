# ppscpi

`ppscpi` is a standalone network server that runs on a PulsePins target board and accepts remote-control commands using
the [Standard Commands for Programmable Instruments
(SCPI)](https://www.ivifoundation.org/About-IVI/scpi.html) protocol. It provides a minimal SCPI
interface for simple sequence streaming.

The IVI Foundation maintains the SCPI standard and hosts the [SCPI-99 specification](https://www.ivifoundation.org/downloads/SCPI/scpi-99.pdf); for a quick non-normative overview, see the [SCPI overview](https://en.wikipedia.org/wiki/Standard_Commands_for_Programmable_Instruments).

`ppscpi` is also convenient for controlling PulsePins from remote clients, including Python or Jupyter notebooks and any environment that can use TCP sockets.

The implementation is in [`c++/ppscpi.cc`]({{ source_file("c++/ppscpi.cc") }}) and the SCPI session/server helpers are in [`c++/scpi_server.hh`]({{ source_file("c++/scpi_server.hh") }}).

## Transport and startup

`ppscpi` listens on standard SCPI TCP port `5025`. By default it binds to
`0.0.0.0`, which accepts connections on all interfaces.

Command-line options:

* `-ip <addr>`: bind address, default `0.0.0.0`

When bound to `0.0.0.0`, `ppscpi` prints a red `WARNING` line, states `waiting one second`, and waits one second before accepting client traffic. Use `-ip 127.0.0.1` for local-only access or another specific interface address when remote access should be limited.

`ppscpi` does not provide authentication or transport encryption. Treat the default bind as network-exposed hardware control and use it only on a trusted network, with a restricted bind address, or behind appropriate network controls.

On startup it:

* configures realtime scheduling and locks memory pages
* creates the shared `HostRuntime` and top-level `FPGA` wrapper
* applies the requested clock and PLL settings; the FPGA reset-manager pulse runs only if `-reset_FPGA` or `PP_RESET_FPGA` is requested
* reports the measured clocks using the frequency-meter block
* accepts SCPI-style commands over the network

The transport is line-oriented. A single line may contain multiple commands separated by
semicolons (`;`); `ppscpi` trims and dispatches each segment in order, with responses emitted
in the same order. Empty segments are ignored. Each segment is parsed as a complete command
path, so relative SCPI path continuation is not implemented.

## Session model

Each client connection gets its own SCPI session object.

Session state includes:

* one `streamer`, `readback`, and `counter` wrapper set bound to the shared `FPGA`
* the loaded `Sequence`
* whether readback checking is enabled
* whether streaming should use forced triggering

The session does not persist across reconnects.

## Supported commands

Standard commands:

* `*IDN?` - identify the instrument
* `*RST` - clear loaded sequence/session state and reset the hardware run path (streamer, readback, counters, mux, output combiner, and trigger combiner)
* `*CLS` - clear status and error queue
* `*OPC` / `*OPC?` - operation complete flag/query
* `*WAI` - no-op; `STREAM` itself is synchronous
* `*ESR?` - standard event status register
* `*STB?` - status byte
* `SYST:ERR?` - query and drain the error queue

PulsePins-specific commands:

* `TEST1` - run a built-in short self-test sequence
* `SEQ <data>` - parse and load a sequence from textual representation, including `f`, `final`, and control-flow records supported by `parse_sequence_from_stream(...)`
* `CHECK <bool>` - enable or disable readback checking during `STREAM`
* `CHECK?` - query the check setting
* `CLOCK:STREAMER?` - query the measured streamer clock frequency in Hz
* `STREAM` - send the loaded sequence and trigger execution
* `DISCONNECT` - close the client session; the `ppscpi` server keeps running
* `TERMINATE` - stop the `ppscpi` server process

## Typical flow

1. Connect to TCP port `5025`
2. Send `*RST`
3. Send `SEQ ...` with the sequence payload
4. Optionally send `CHECK ON`
5. Send `STREAM`
6. Query `SYST:ERR?` if needed

## Workstation Python and Jupyter

Jupyter should normally run on a workstation, not on the DE10-Nano. The target board only needs to run `ppscpi`; the notebook talks to it over Ethernet.

The lightweight workstation Python client lives in [`python/pulsepins/`]({{ source_file("python/pulsepins/") }}) and uses only the Python standard library. From a checkout, make that directory importable first:

```bash
export PYTHONPATH=/path/to/PulsePins/python
```

For notebooks, an editable install on the workstation is often more convenient:

```bash
python3 -m pip install -e /path/to/PulsePins/python
```

That install also provides example commands such as `pulsepins-ppscpi-check`, `pulsepins-ppscpi-hello`, `pulsepins-notebook-workflow`, `pulsepins-timeline-preview`, `pulsepins-timeline-stream`, and `pulsepins-timeline-sweep`. Add `--self-test` to `pulsepins-ppscpi-check` to run the built-in `TEST1` hardware smoke path after connecting.

Then a notebook or script can drive the target board with:

```python
from pulsepins import PulsePins

with PulsePins("de10nano") as pp:
    print(pp.idn())
    print(pp.streamer_clock_hz())
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

`load_sequence(...)` accepts normal multiline PulsePins text sequence input and flattens it into the single-line `SEQ ...` command that `ppscpi` expects. Because the SCPI transport is line-oriented, one uploaded sequence command must fit within the server's 64 KiB line limit and must not contain semicolons.

If a high-level Python client command receives an error or failure response, it drains `SYST:ERR?` and raises `PulsePinsCommandError` with the queued server-side diagnostic text.

For notebook-oriented pulse construction, the same package provides `Timeline`:

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
```

`Timeline.to_sequence(...)` returns the generated text sequence, `Timeline.to_csv()` writes browser-compatible Timeline CSV, `Timeline.to_draft_json()` writes browser-compatible draft JSON, `Timeline.to_vcd(...)` writes a scalar waveform trace, and notebooks render a lightweight SVG preview when the timeline object is evaluated. Use `include_final=True` when streaming finite Timeline pulses so owned channels return to their resting value after the last pulse.

The same workflow is available as runnable examples:

```bash
PYTHONPATH=python python3 python/examples/timeline_preview.py --svg timeline.svg --csv timeline.csv --draft timeline.json --vcd timeline.vcd
PYTHONPATH=python python3 python/examples/timeline_stream.py de10nano --print-sequence
PYTHONPATH=python python3 python/examples/timeline_sweep.py de10nano --delays-us 0 5 10
```

With the editable install, use the installed command names instead:

```bash
pulsepins-ppscpi-check de10nano --self-test
pulsepins-notebook-workflow de10nano --output-dir previews --run
pulsepins-timeline-preview --svg timeline.svg --csv timeline.csv --draft timeline.json --vcd timeline.vcd
pulsepins-timeline-stream de10nano --print-sequence
pulsepins-timeline-sweep de10nano --delays-us 0 5 10
```

Live Timeline stream/sweep commands query `CLOCK:STREAMER?` before converting absolute-time pulses; pass `--clock-hz` only when you need to override the streamer-clock frequency measured by `ppscpi` at startup. Hardware-free preview and sweep `--dry-run` use the supplied/default dry-run clock.

## Notes

* `STREAM` uses the same send/trigger path as the local tools, including optional readback verification.
* `*RST` restores a clean remote-control run state; it does not reprogram clocks or PLLs.
* `SEQ` stores the parsed sequence in memory; nothing is transmitted to the streamer until `STREAM` is issued.
* Semicolons separate SCPI commands before command parsing, so they are not valid inside a `SEQ` payload.
* If the loaded sequence does not end with terminal `final V` and the server was not started with `-t`, `-random_final`, or `PP_RANDOM_FINAL`, `STREAM` appends a no-modify final terminator so outputs remain at the last sequence value. The `f` record only requests forced triggering.
* Repeated `STREAM` commands reuse the stored sequence exactly as parsed; the cached session sequence is not rewritten by readback checking or final-output preparation.
* Before each hardware-touching run (`TEST1` and `STREAM`), `ppscpi` resets the streamer core, readback encoder, and counters so repeated commands in the same process/session start from a clean hardware state.
* If `TEST1` or `STREAM` returns `FAILURE`, `ppscpi` also pushes an execution-error record into `SYST:ERR?` with the aggregated PulsePins return-code bits so remote clients can distinguish timeout, readback, CRC, buffer, and overflow failures.
* When `CHECK ON` is active and no explicit startup `-timeout` or `-hard-timeout` was supplied, the shared workflow uses a conservative default readback timeout: 2 s for the first readback element and 2 s for later idle gaps. Start `ppscpi` with `-timeout 0` to disable idle-timeout protection, `-timeout <value>` to set the idle-gap timeout, or `-hard-timeout <value>` to set an absolute readback timeout from command start.
* Finite `STREAM` runs also inherit the internal 10 s streamer-completion timeout from the shared playback path. When that timeout fires, the user-facing message is `timed out waiting for streamer completion (10 s internal limit)`.
* Hardware-touching commands are serialized across sessions through the shared FPGA lock, so multiple clients can stay connected without racing each other on streamer/reset state.
* The server is intended for remote orchestration, not for high-throughput binary bulk transfer.
* Command-handler exceptions are converted into SCPI error/status state instead of tearing down the whole server process.
* After `DISCONNECT`, clients can reconnect and start a fresh independent session.
* `TERMINATE` is the explicit server-shutdown command; it closes the client session and stops the process.

## Related pages

* [pptool](pptool.md)
* [C++ application programming interface](cpp.md)
* [Readback](readback.md)
* [Build and deployment](build.md)
