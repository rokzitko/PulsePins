# ppdelay

`ppdelay` is the simplest PulsePins one-shot delay generator.

It waits for a trigger, delays for a configurable time, emits one pulse, and then leaves the outputs in the selected final state.

## When to use it

Use `ppdelay` when you need:

* a single delayed pulse after a trigger event
* a quick hardware-level timing check without writing a sequence file by hand
* a simple trigger-to-output delay workflow for shutters, cameras, or synchronized instruments

For continuously repeating signals, use [`ppfg`](ppfg.md) instead.

## Common options

* `-p`, `-m`: trigger pattern and trigger mask
* `-delay`: delay between trigger and pulse start
* `-duration`: pulse width
* `-v1`: value during the pulse
* `-v0`: value immediately after the pulse
* `-t`: final persistent output value after completion

All standard trigger-combiner and clock-selection options are also accepted.

## Examples

Immediate 1 ms pulse after the selected trigger source:

```bash
ppdelay -veryverbose -trig_misc -core_pll 10M -duration 1ms
```

1 ms pulse delayed by 20 ms after the trigger:

```bash
ppdelay -veryverbose -trig_misc -core_pll 10M -duration 1ms -delay 20ms
```

Pulse with explicit on/off/final values:

```bash
ppdelay -trig_misc -delay 5ms -duration 500us -v1 0x1 -v0 0x0 -t 0x0
```

## What to expect

* the command arms the trigger path and transmits a finite sequence
* once the trigger event occurs, the configured delay elapses first
* the outputs are then driven to `-v1` for the configured duration
* after the pulse, the outputs go to `-v0` and finally settle to `-t`

With `-veryverbose`, the generated sequence is printed before playback.

## Related pages

* [`ppfg`](ppfg.md)
* [`pptrig`](pptrig.md)
* [`examples.md`](examples.md)
