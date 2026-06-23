# ppvcd

`ppvcd` is the VCD-focused entry point for `ppplay` when replaying [Value Change Dump (VCD)](https://en.wikipedia.org/wiki/Value_change_dump) files.

VCD import parses `$timescale`; `-scale` is the PulsePins output period in ns and defaults to `10`.

Equivalent usage:

```bash
ppvcd -file waveform.vcd -target outs -scale 10 -force
```

corresponds to:

```bash
ppplay -format vcd -file waveform.vcd -target outs -scale 10 -force
```

For format support, option descriptions, and examples, see [ppplay](ppplay.md).
