## ppread

`ppread` is a readback capture tool. It can dump captured run-length encoded data to console,
or it can switch into export mode and save the capture as PulsePins text sequence format, as VCD,
as the exact binary sequence format, or as any combination of those.

Files exported by `ppread` can be replayed with [`ppplay`](ppplay.md). VCD exports use the
default `outs` signal and `$timescale 10ns`, matching `ppplay`'s default VCD target and scale.

Command line arguments:

* ``-oe``: output enable (bool). If true, we are reading internally generated data. If false, we are
reading external data on the device I/O pins. If unspecified, use the hardware default (false).
* ``-timeout``: controls readback idle wait bounds. If omitted, `ppread` uses a conservative default timeout: 2s waiting for the first readback element and 2s for later idle gaps. A positive value is interpreted as time after the last element read. ``-timeout 0`` disables idle-timeout protection. For compatibility, a negative value is interpreted as an absolute timeout from tool start, in seconds; prefer ``-hard-timeout`` for new commands.
* ``-hard-timeout T``: absolute readback timeout from tool start. Time units such as ``ms``, ``s``, and ``min`` are accepted. This is the preferred way to say "capture for at most T".
* ``-save-vcd <file>``: capture the readback stream and save it as a VCD waveform file. The default C++ VCD export uses ``$timescale 10ns`` so captures replay through ``ppplay`` with its default VCD scale.
* ``-save-text <file>``: capture the readback stream and save it in PulsePins text sequence format.
* ``-save-binary <file>``: capture the readback stream and save it in the exact PulsePins binary sequence format.

If any of ``-save-vcd``, ``-save-text``, or ``-save-binary`` is specified, `ppread` switches into export mode: it captures
the readback stream into a `Sequence`, writes the requested file(s), and prints a concise summary
instead of dumping every captured run to the terminal.

## Examples

Capture for one second and save the waveform as VCD:

```bash
ppread -hard-timeout 1s -save-vcd capture.vcd
```

Capture for one second and save the readback stream in PulsePins text sequence format:

```bash
ppread -hard-timeout 1s -save-text capture.seq
```

Save both formats at the same time:

```bash
ppread -hard-timeout 1s -save-vcd capture.vcd -save-text capture.seq
```

Save an exact binary capture:

```bash
ppread -hard-timeout 1s -save-binary capture.ppbin
```

### Record and replay

Capture once, save all replayable formats, then replay the capture with `ppplay`:

```bash
ppread -hard-timeout 1s -save-vcd capture.vcd -save-text capture.seq -save-binary capture.ppbin
ppplay -file capture.vcd
ppplay -file capture.seq
ppplay -file capture.ppbin
```

Use `capture.vcd` when you want waveform viewing as well as replay, `capture.seq` when you want
an editable text sequence, and `capture.ppbin` when you want exact lossless replay.

For a fuller workflow, see [Example 3: Capture a waveform and replay it exactly](examples.md#example-3-capture-a-waveform-and-replay-it-exactly).

When `-oe 0` is used, this becomes a simple external logic-analyzer capture workflow for the
qout bus and valid signal.
