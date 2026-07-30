## ppread

`ppread` is a qualified-sample readback capture tool. It can dump captured run-length-encoded data to console,
or it can switch into export mode and save the capture in the [PulsePins text sequence format](sequence_format.md), as a VCD projection,
as a current-build binary sequence snapshot, or as any combination of those.

Files exported by `ppread` can be replayed with [`ppplay`](ppplay.md). VCD exports use the
default `outs` signal and `$timescale 10ns`, matching `ppplay`'s default VCD target and scale.
All exports represent only samples for which `qout_valid` was high, not the authored operators or control flow. Readback flushes a run when validity drops but records no duration for the invalid interval, so no-strobe states and trigger, gate, or idle gaps are absent from replay. The binary snapshot preserves normalized current-width captured elements and a force-trigger flag, which `ppread` writes as false, but loading requires matching control, count, value, and trigger widths and execution still depends on external hardware context.

Command line arguments:

* ``-oe``: physical output enable (bool). If true, we are reading internally generated data. If false, we are
reading external data on the target-board I/O pins. If unspecified, leave the current hardware setting
unchanged; after reset the default is false.
* ``-timeout``: controls readback idle wait bounds. If omitted, `ppread` uses a conservative default timeout: 2 s waiting for the first readback element and 2 s for later idle gaps. A positive value is interpreted as time after the last element read. ``-timeout 0`` disables idle-timeout protection. For compatibility, a negative value is interpreted as a total timeout from the start of the current readback phase, in seconds; prefer ``-hard-timeout`` for new commands.
* ``-hard-timeout T``: total timeout from the start of the current readback phase. Time units such as ``ms``, ``s``, and ``min`` are accepted. This bounds the readback phase, not setup or sequence transmission that occurs before it.
* ``-save-vcd <file>``: capture the qualified readback runs and project them into a VCD file. The default C++ VCD export uses ``$timescale 10ns`` so captures replay through ``ppplay`` with its default VCD scale.
* ``-save-text <file>``: capture the qualified normalized runs in PulsePins text sequence format.
* ``-save-binary <file>``: save the captured `Sequence` as a current-build binary snapshot.

If any of ``-save-vcd``, ``-save-text``, or ``-save-binary`` is specified, `ppread` switches into export mode: it captures
the readback stream into a `Sequence`, writes the requested file(s), and prints a concise summary
instead of dumping every captured run to the terminal.

A timeout does not force the hardware encoder to flush its active run. A run becomes available only when the value changes, its counter saturates, or `qout_valid` drops. Consequently, a constant valid level can produce an empty bounded capture, and the run active at timeout is omitted. Arrange a validity drop or value transition when the final run must be saved.

## Examples

Bound the readback phase to one second and save completed runs as VCD:

```bash
ppread -hard-timeout 1s -save-vcd capture.vcd
```

Bound the readback phase to one second and save completed runs as PulsePins text:

```bash
ppread -hard-timeout 1s -save-text capture.seq
```

Save both formats at the same time:

```bash
ppread -hard-timeout 1s -save-vcd capture.vcd -save-text capture.seq
```

Save a current-build binary snapshot:

```bash
ppread -hard-timeout 1s -save-binary capture.ppbin
```

### Record and replay

Follow the [capture and replay procedure](manual/capture_replay.md). Use two board terminals: start `ppread -veryverbose ...` in the foreground in terminal 1 and wait for its initial `Readback status:` line before starting the waveform source in terminal 2. Do not background `ppread` or substitute a guessed sleep for that readiness check.

Use `capture.vcd` when you want waveform viewing as well as replay, `capture.seq` when you want
an editable normalized text sequence, and `capture.ppbin` when you need a matching-build snapshot
of the qualified captured `Sequence`. None of these restores authored control flow, invalid gaps,
or no-strobe states. An unmodified capture has no trigger records and its force flag is false, so
replay it with `-force`. To wait for a configured condition, add trigger records to a text sequence
rather than only omitting `-force`.

`ppread` captures only samples produced while it is active. It cannot recover a finite waveform that completed before the readback encoder was reset at command startup.

When `-oe 0` is used, this becomes a simple external logic-analyzer capture workflow for the
`qout` bus and `qout_valid` sample qualifier.
