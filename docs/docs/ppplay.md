# ppplay

`ppplay` is the general sequence-file playback tool for PulsePins.

It loads a sequence from a file, converts it into the internal `Sequence` representation if needed,
and streams it through the normal PulsePins playback path.

[`ppread`](ppread.md) can export captures in all supported playback formats, so a common workflow is
to record a readback stream with `ppread` and replay the saved file with `ppplay`.

Current supported formats:

* [Value Change Dump (VCD)](https://en.wikipedia.org/wiki/Value_change_dump) waveform interchange (`vcd`)
* [PulsePins text sequence format](sequence_format.md) (`text`)
* PulsePins current-build binary sequence snapshot (`binary`)

Text represents the parser-representable normalized subset, and VCD imports effective output states rather than authored operators or control flow. The binary snapshot preserves normalized current-width element triplets and the force-trigger flag, but loading requires matching control, count, value, and trigger widths. The streamer clock, initial output, trigger and gate routing, and output configuration remain external execution context for every format.

## Common options

* `-file PATH`: input sequence file to load
* `-format vcd|text|binary`: explicitly select the file format
* `-force`: force triggering after loading the sequence; for text and binary input this overrides any in-file force-trigger flag

Shared playback options such as `-check`, `-read`, `-timeout`, `-hard-timeout`, `-t`, `-random_final`, and `-dont_wait` behave as they do for the other streaming tools. With `-dont_wait`, playback returns after queueing the sequence, activating or arming the trigger, and completing any requested readback phase; it skips the normal wait and post-run cleanup, so forced or armed trigger state may remain active until reset, reconfiguration, or explicit deactivation. If neither `-t`, `-random_final`/`PP_RANDOM_FINAL`, nor an explicit terminal `final V` record is provided, playback appends a no-modify final terminator and leaves outputs at the last sequence value. Like the other finite playback commands, `ppplay` also enforces an internal 10 s streamer-completion timeout after the sequence has been queued.

## VCD-specific options

* `-target NAME`: VCD signal name to convert; defaults to `outs`
* `-scale N`: PulsePins output period in ns; VCD `$timescale` is parsed before timestamps are divided by this value. It must be greater than zero and defaults to `10`.

These options are only valid for VCD input.

## Format selection

If `-format` is not provided, `ppplay` tries to infer the format from the file extension:

* `.vcd` -> `vcd`
* `.seq` -> `text`
* `.txt` -> `text`
* `.bin` -> `binary`
* `.ppbin` -> `binary`

If the extension is ambiguous, `ppplay` exits with an error and asks for `-format`.

## Examples

### Replay a `ppread` capture

Follow the [capture and replay procedure](manual/capture_replay.md). Use two board terminals: run `ppread -veryverbose ...` in the foreground in terminal 1 and wait for its initial `Readback status:` line before starting the waveform source in terminal 2. Do not coordinate capture with a background process and a guessed sleep.

After capture completes, replay one of the saved files:

```bash
ppplay -force -file capture.vcd
ppplay -force -file capture.seq
ppplay -force -file capture.ppbin
```

`ppread` VCD exports use the default `outs` signal and `$timescale 10ns`, so normal captures replay
with `ppplay -force -file capture.vcd` without extra VCD options. All three exports describe the
`qout_valid`-qualified sample stream: text contains editable normalized runs, VCD projects those
runs, and binary is a current-build snapshot of that captured `Sequence`. Invalid gaps and
no-strobe states are absent, so replay compresses those gaps rather than reproducing their elapsed
time. A captured binary does not restore authored operators, triggers, replay structure, control
flow, or force request. An unmodified `ppread` capture has no trigger records and stores its force
flag as false, so supply `-force` when replaying it. To arm on a condition instead, author trigger
records in a text sequence and configure their trigger routing.

### Other playback examples

Replay a waveform from a VCD file:

```bash
ppplay -force -file waveform.vcd
```

Replay a PulsePins text sequence:

```bash
ppplay -force -file capture.seq
```

Replay a text sequence with explicit format selection:

```bash
ppplay -force -file capture.txt -format text
```

Replay a VCD using a specific signal and scale factor:

```bash
ppplay -force -file waveform.vcd -target outs -scale 10
```

Force playback even when a text file does not request force-triggering:

```bash
ppplay -force -file capture.seq
```

Replay a current-build binary sequence snapshot:

```bash
ppplay -force -file capture.ppbin
```
