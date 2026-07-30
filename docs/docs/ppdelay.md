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

* `-p`: trigger pattern (default: `0b00000001`)
* `-m`: trigger mask (default: `0b00000001`)
* `-delay`: delay between trigger and pulse start (default: `0`)
* `-duration`: pulse width (default: `0`; resulting width is at least one output-clock cycle)
* `-v1`: value during the pulse (default: `0xFFFFFFFF`)
* `-v0`: value immediately after the pulse (default: `0x00000000`)
* `-t`: final persistent output value after completion (default: `0`)

All standard trigger-combiner and clock-selection options are also accepted.

## Examples

Immediate 1 ms pulse after the selected trigger source:

```bash
ppdelay -veryverbose -trig_misc -int_pll 10M -duration 1ms -v1 0x1 -v0 0x0 -t 0x0
```

1 ms pulse delayed by 20 ms after the trigger:

```bash
ppdelay -veryverbose -trig_misc -int_pll 10M -duration 1ms -delay 20ms -v1 0x1 -v0 0x0 -t 0x0
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

`ppdelay` returns after queueing the sequence and arming the trigger; it does not wait for the trigger or pulse to complete. If the intended trigger is not supplied, the wait remains armed indefinitely and a later event can release the pulse. Run `ppreset -i 0x0` to cancel an abandoned attempt. Running `pptrig` also resets the primary streamer, so it cannot inspect this sequence without cancelling it.

## Related pages

* [`ppfg`](ppfg.md)
* [`pptrig`](pptrig.md)
* [User manual](examples.md)
* [Triggered one-shot delay](manual/triggered_delay.md)
