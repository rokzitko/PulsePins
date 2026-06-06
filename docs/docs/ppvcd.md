# ppvcd

`ppvcd` is the VCD-specific compatibility alias for `ppplay`.

It preserves the traditional PulsePins workflow of loading a VCD waveform and replaying it through
the streamer hardware, but the canonical general-purpose tool is now `ppplay`.

VCD import parses `$timescale`; `-scale` is the PulsePins output period in ns and defaults to `10`.

Equivalent usage:

```bash
ppvcd -file waveform.vcd -target outs -scale 10 -force
```

corresponds to:

```bash
ppplay -format vcd -file waveform.vcd -target outs -scale 10 -force
```

For current format support, option descriptions, and examples, see [ppplay](ppplay.md).
