# PP_PMOD Triggering

`PP_PMOD` exposes the trigger subsystem in two ways:

* an 8-bit digital trigger bus on the shield connector set
* a dedicated SMA trigger input with a fast comparator and adjustable threshold

This is the main board-specific feature that turns the generic trigger logic in PulsePins into a practical lab interface.

## Trigger overview

The trigger bus exported by the board is `TRIG_IN0..TRIG_IN7`.

The board also breaks out the core trigger control lines:

* `TRIG_ENABLE`
* `TRIG_FORCE`
* `TRIG_RESET`

And it exposes trigger status signals used elsewhere on the board:

* `TRIG_ARMED`
* `TRIG_ACTIV`

## Trigger SMA input path

The dedicated SMA trigger path lives on the `Triggering` schematic sheet and includes:

* an SMA input connector
* ESD protection on the external-facing path
* optional input termination
* a fast `ADCMP604` comparator
* a trimmer potentiometer used to set the comparator threshold
* a monitoring LED

This threshold control belongs to the trigger input path. It is not part of the `EXT_CLK` input path.

## Trigger digital connector path

The trigger bus connector presents the logic-level trigger inputs `TRIG_IN0..TRIG_IN7` on a 2x6 expansion-style header.

Use this path when the trigger source is already digital and referenced to the board's logic domain, or when a PMOD-style trigger breakout is more convenient than coaxial wiring.

The connector grouping on `J7` is:

| Pins | Signals |
| --- | --- |
| odd row | `TRIG_IN3`, `TRIG_IN2`, `TRIG_IN1`, `TRIG_IN0`, then utility pins |
| even row | `TRIG_IN7`, `TRIG_IN6`, `TRIG_IN5`, `TRIG_IN4`, then utility pins |

## Trigger control signals

The board exposes the main trigger-control signals directly:

| Signal | Meaning |
| --- | --- |
| `TRIG_ENABLE` | arm/enable the trigger-detection path |
| `TRIG_FORCE` | force a trigger event |
| `TRIG_RESET` | reset the trigger path |

These are useful during bring-up, debugging, and experiments where hardware-side trigger control is preferable to software-only control.

These signals appear on the dedicated `J10` trigger control/status header together with `TRIG_ARMED`, `TRIG_ACTIV`, and one spare `TRIG_NC` position.

## Trigger status signals

| Signal | Meaning |
| --- | --- |
| `TRIG_ARMED` | waiting for a trigger event |
| `TRIG_ACTIV` | trigger condition has fired |

Both status signals are also used for board indicators.

## Threshold control

The SMA trigger path can accept analog-like or edge-shaped external signals and convert them into a digital trigger input using the onboard comparator.

The threshold potentiometer makes the trigger path suitable for experiments where a fixed CMOS threshold is not ideal. When documenting or reproducing a setup, record the comparator threshold setting alongside the signal source and termination setting.

## Validation with `pptrig` and `pptest`

Useful host-side tools:

* [`pptrig`](pptrig.md) for trigger-path troubleshooting and source selection
* [`pptest`](pptest.md) for end-to-end trigger exercises

Typical validation notes worth recording:

* trigger source and wiring
* whether the SMA or digital trigger path was used
* threshold setting
* termination setting
* exact command line and observed behavior

See also [PP_PMOD connectors and pinout](pp_pmod_connectors.md).
