## ppread

`ppread` is the user-facing readback capture tool. It can dump captured run-length encoded data,
or it can switch into export mode and save the capture as PulsePins text sequence format, as VCD,
as the exact binary sequence format, or as any combination of those.

Command line arguments:

* ``-oe``: output enable (bool). If true, we are reading internally generated data. If false, we are
reading external data on the device I/O pins. If unspecified, use the hardware default (false).
* ``-timeout``: timeout in seconds (floating point number). If positive, interpreted as time after
the last element read. If negative, interpreted as time after starting the tool.
* ``-save-vcd <file>``: capture the readback stream and save it as a VCD waveform file.
* ``-save-text <file>``: capture the readback stream and save it in PulsePins text sequence format.
* ``-save-binary <file>``: capture the readback stream and save it in the exact PulsePins binary sequence format.

If any of ``-save-vcd``, ``-save-text``, or ``-save-binary`` is specified, `ppread` switches into export mode: it captures
the readback stream into a `Sequence`, writes the requested file(s), and prints a concise summary
instead of dumping every captured run to the terminal.

## Examples

Capture for one second and save the waveform as VCD:

```bash
ppread -timeout 1 -save-vcd capture.vcd
```

Capture for one second and save the readback stream in PulsePins text sequence format:

```bash
ppread -timeout 1 -save-text capture.seq
```

Save both formats at the same time:

```bash
ppread -timeout 1 -save-vcd capture.vcd -save-text capture.seq
```

Save an exact binary capture:

```bash
ppread -timeout 1 -save-binary capture.ppbin
```

When `-oe 0` is used, this becomes a simple external logic-analyzer capture workflow for the
qout bus and valid signal.
