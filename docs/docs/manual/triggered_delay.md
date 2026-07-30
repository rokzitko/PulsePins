# Triggered one-shot delay

## Target profile/status

This chapter targets current `main`, host/FPGA ABI 6, and the released 32-bit DE10-Nano profile. Older release images may differ in available commands, options, startup output, or FPGA behavior; use documentation that matches the installed image when it is not a current-source build.

Status: this is a source-aligned procedure, not a claim of bench validation.

## Goal

Arm one finite delayed pulse from the on-board MISC trigger path. Pressing KEY0 satisfies MISC trigger bit 0; the program requests a 20 ms delay, drives only `qout[0]` high for 1 ms, drives it low again, and finishes with the complete output bus at `0x00000000`.

## Safety

PulsePins uses 3.3 V LVTTL signaling. Connect the board and instrument to a common ground, use a high-impedance scope or logic-analyzer input, and do not apply a direct 50 ohm termination to an unbuffered FPGA output. Disconnect any external source that could drive `qout[0]` while PulsePins output enable is active.

Set on-board switches SW0, SW1, and SW2 off before arming. SW1 is MISC trigger force and SW2 is MISC trigger reset; leaving either asserted changes the intended KEY0 workflow.

## What you need

* A DE10-Nano running a current ABI 6 PulsePins host build and matching FPGA image
* Access to on-board KEY0 and switches SW0 through SW2
* A board shell with `ppdelay`, `ppreset`, and `ppread` available
* A high-impedance oscilloscope or logic analyzer

## Wiring/setup table

| Item | Board control or connection | Role in this procedure |
| --- | --- | --- |
| Output | `qout[0]`: GPIO 1 (`JP7`) pin 5 | High-impedance instrument channel |
| Reference | GPIO 1 (`JP7`) pin 12 or pin 30 | Common ground with the instrument |
| Trigger | KEY0, the right button next to the Ethernet side | MISC trigger input bit 0 |
| Negative-control trigger | KEY1, the left button closer to the GPIO connector | MISC trigger input bit 1; masked out here |
| SW0 | Off, toward the Arduino connector | External MISC enable is not needed; `ppdelay` arms through software |
| SW1 | Off, toward the Arduino connector | Prevent MISC force |
| SW2 | Off, toward the Arduino connector | Release MISC reset |

## Procedure

1. Put SW0, SW1, and SW2 in the off position, then establish a zero idle state:

   ```bash
   ppreset -i 0x0
   ```

2. Queue and arm one delayed pulse. Pattern `0x1` with mask `0x1` selects only MISC bit 0, which is KEY0:

   ```bash
   ppdelay -veryverbose -trig_misc -p 0x1 -m 0x1 -i 0x0 -delay 20ms -duration 1ms -v1 0x1 -v0 0x0 -t 0x0
   ```

3. After the command has armed the hardware, press and release KEY0 once, then wait until the instrument has observed the pulse. `ppdelay` returns after arming and provides no hardware-completion indication. The sequence is a one-shot; do not use a repeating generator for this procedure.

4. If KEY0 will not be pressed, run the cleanup command immediately. The queued trigger wait has no automatic expiration, so a manual reset is required to keep the workflow bounded when it is left armed.

## Expected result

After KEY0 satisfies trigger bit 0, the sequence schedules `0x00000000` for the programmed delay, `0x00000001` for the programmed pulse duration, then `0x00000000`, with final value `0x00000000`. Bits `qout[31:1]` remain low.

The `-delay 20ms` setting is the programmed streamer delay after the trigger condition is accepted. It is not a statement that measured KEY0-to-pin latency is exactly 20 ms. Physical latency can also include button behavior, input synchronization, trigger-combiner and streamer pipelines, output buffering, cabling, and instrument thresholding. Measure both relevant physical nodes if end-to-end latency matters.

## Cleanup

Reset after the one-shot, and always reset if the sequence was armed but never triggered:

```bash
ppreset -i 0x0
```

The reset actively drives zero. Leave SW0 through SW2 off. Before connecting another driver, power down or release the bidirectional pins:

```bash
ppread -oe 0 -hard-timeout 100ms
```

## Troubleshooting/negative control

For a negative control, arm the same command, press KEY1 instead of KEY0, verify that `qout[0]` does not run the pulse, and then issue `ppreset -i 0x0` because the KEY0 condition is still armed. KEY1 is MISC bit 1 and is excluded by mask `0x1`.

If KEY0 does not trigger, check that `-trig_misc`, pattern `0x1`, and mask `0x1` are present, SW2 is off, and the command reached the armed state. If the pulse starts without KEY0, check that SW1 is off. Use the trigger status LEDs while the one-shot is armed. Do not run `pptrig` to inspect that state: constructing the diagnostic resets the primary streamer and cancels the queued one-shot.

## Reproducibility record

| Field | Record |
| --- | --- |
| Date and operator | |
| PulsePins commit or release image | |
| Host version and commit from startup | |
| Bitstream timestamp and ABI version | |
| Reported streamer-clock frequency | |
| Exact `ppdelay` command | Preserve the command used |
| KEY and switch positions | |
| Connector and instrument input mode | |
| Programmed delay and pulse duration | `20 ms`; `1 ms` |
| Programmed high, low, and final values | `0x1`; `0x0`; `0x0` |
| Observed result or measured data | |

## Related pages

* [Sequence file format](../sequence_format.md)
* [`ppdelay`](../ppdelay.md)
* [`pptrig`](../pptrig.md)
* [`ppreset`](../ppreset.md)
* [DE10-Nano signal reference](../de10_nano_reference.md)
* [Sequencer model](../sequencer_model.md)
* [RTL latency and timing](../latency.md)
