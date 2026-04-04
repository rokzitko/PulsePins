# ppplay

`ppplay` is the general sequence-file playback tool for PulsePins.

It loads a sequence from a file, converts it into the internal `Sequence` representation if needed,
and streams it through the normal PulsePins playback path.

Current supported formats:

* `vcd`
* PulsePins text sequence format (`text`)

Binary sequence-file support is planned, but not implemented yet.

## Common options

* `-file PATH`: input sequence file to load
* `-format vcd|text`: explicitly select the file format
* `-force`: force triggering after loading the sequence; for text input this overrides the in-file `f` flag

Shared playback options such as `-check`, `-read`, `-timeout`, `-t`, and `-dont_wait` behave as they do for the other streaming tools.

## VCD-specific options

* `-target NAME`: VCD signal name to convert; defaults to `outs`
* `-scale N`: output period in ns for each VCD time unit; defaults to `10`

These options are only valid for VCD input.

## Format selection

If `-format` is not provided, `ppplay` tries to infer the format from the file extension:

* `.vcd` -> `vcd`
* `.seq` -> `text`
* `.txt` -> `text`

If the extension is ambiguous, `ppplay` exits with an error and asks for `-format`.

## Examples

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
