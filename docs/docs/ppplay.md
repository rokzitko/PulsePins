# ppplay

`ppplay` is the general sequence-file playback tool for PulsePins.

It loads a sequence from a file, converts it into the internal `Sequence` representation if needed,
and streams it through the normal PulsePins playback path.

[`ppread`](ppread.md) can export captures in all supported playback formats, so a common workflow is
to record a readback stream with `ppread` and replay the saved file with `ppplay`.

Current supported formats:

* `vcd`
* PulsePins text sequence format (`text`)
* PulsePins binary sequence format (`binary`)

The binary format is exact and lossless: it preserves the full internal sequence representation, including control-flow elements and the force-trigger flag.

## Common options

* `-file PATH`: input sequence file to load
* `-format vcd|text|binary`: explicitly select the file format
* `-force`: force triggering after loading the sequence; for text input this overrides the in-file `f` flag

Shared playback options such as `-check`, `-read`, `-timeout`, `-t`, `-random_final`, and `-dont_wait` behave as they do for the other streaming tools. With `-dont_wait`, playback returns after queueing the sequence, activating or arming the trigger, and completing any requested readback phase; it skips the normal wait and post-run cleanup, so forced or armed trigger state may remain active until reset, reconfiguration, or explicit deactivation. If neither `-t`, `-random_final`/`PP_RANDOM_FINAL`, nor an explicit `final V` record is provided, playback appends a no-modify final terminator and leaves outputs at the last sequence value. Like the other finite playback commands, `ppplay` also enforces an internal 10 s streamer-completion timeout after the sequence has been queued.

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

Record a capture in all replayable formats:

```bash
ppread -timeout 1 -save-vcd capture.vcd -save-text capture.seq -save-binary capture.ppbin
```

Replay one of the saved files:

```bash
ppplay -file capture.vcd
ppplay -file capture.seq
ppplay -file capture.ppbin
```

`ppread` VCD exports use the default `outs` signal and `$timescale 10ns`, so normal captures replay
with `ppplay -file capture.vcd` without extra VCD options. Use `capture.seq` when you want an
editable text sequence and `capture.ppbin` when you want exact lossless replay.

For a fuller workflow, see [Example 3: Capture a waveform and replay it exactly](examples.md#example-3-capture-a-waveform-and-replay-it-exactly).

### Other playback examples

Replay a waveform from a VCD file:

```bash
ppplay -file waveform.vcd
```

Replay a PulsePins text sequence:

```bash
ppplay -file capture.seq
```

Replay a text sequence with explicit format selection:

```bash
ppplay -file capture.txt -format text
```

Replay a VCD using a specific signal and scale factor:

```bash
ppplay -file waveform.vcd -target outs -scale 10
```

Force playback even when a text file does not request force-triggering:

```bash
ppplay -file capture.seq -force
```

Replay an exact binary sequence capture:

```bash
ppplay -file capture.ppbin
```
