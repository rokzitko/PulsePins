# PP_PMOD LEDs, Jumpers, And Testpoints

This page collects the practical service and bring-up details that tend to matter once the board is assembled and connected to instruments.

## LED meanings

The `LEDs` sheet includes indicators for:

* `TRIG_ARMED`
* `TRIG_ACTIV`
* `DONE`
* `BUFFER_ERROR`
* `ACTIVITY`
* `HEARTBEAT`

These are useful for quick visual confirmation that the board is alive, armed, triggered, or reporting a buffer problem before moving to more detailed probing.

## Jumpers and configuration links

The board includes small headers or links associated with optional termination and source-selection behavior, especially on the trigger and clock-input paths.

Named configuration references visible in the current schematic include:

* clocking sheet: `J20` is an in-series patch/disconnect point on `EXT_CLK`
* clocking sheet: `J25` is an in-series patch/disconnect point on `PPS_IN`
* trigger sheet: `J16` is an in-series patch/disconnect point on `TRIG_IN0`
* trigger sheet: `J19` is an in-series patch/disconnect point on the SMA trigger path

There are also 0 ohm link parts in these configurable paths, for example `R30`, `R31`, `R26`, and `R27`.

When documenting a setup, record:

* whether the optional terminations are enabled
* whether an onboard oscillator module is populated
* any source-selection or configuration links relevant to `EXT_CLK`, `PPS_IN`, or the SMA trigger path

Before changing any jumper or link, verify its exact role in the relevant schematic sheet rather than relying on shorthand board descriptions.

## Testpoints

The `Misc` and `I2C` sheets include dedicated testpoints that support board bring-up and troubleshooting.

Named testpoints visible in the current schematic include:

* `Misc` sheet: `TP1`, `TP2`, `TP3`, `TP4`, `TP8`, `TP9`, `TP10`, `TP11`, `TP12`
* `I2C` sheet: `TP5`, `TP6`, `TP7`

Confirmed testpoint roles from the current schematics:

| Ref | Sheet | Observed role |
| --- | --- | --- |
| `TP1` | `Misc` | ground probe point |
| `TP2` | `Misc` | ground loop-style probe point |
| `TP3` | `Misc` | `TRIG_ACTIV` testpoint |
| `TP4` | `Misc` | `STROBE` testpoint |
| `TP8` | `Misc` | ground probe point |
| `TP9` | `Misc` | ground probe point |
| `TP10` | `Misc` | ground probe point |
| `TP11` | `Misc` | ground probe point |
| `TP12` | `Misc` | ground probe point |
| `TP5` | `I2C` | DAC `VOUT` testpoint, carrying the programmed analog output voltage |
| `TP6` | `I2C` | DAC `VREF` testpoint |
| `TP7` | `I2C` | local regulated `+3.3 V` testpoint in the DAC/I2C subsection |

`TP5` is the one testpoint in this set that should be treated as an analog signal observation point rather than as a convenience ground or fixed-rail check.

Typical uses include:

* verifying local supply rails
* checking DAC or sensor-related nodes
* confirming trigger or timing-path activity
* attaching probes without disturbing the connector wiring under test

## Grounding features

The board includes dedicated ground connection points intended to make oscilloscope probing cleaner and more repeatable.

Several of the `Misc`-sheet probe features are implemented as loop-style or through-hole testpoints intended for easier instrument hookup, including the beaded loop footprints used on `TP2`, `TP3`, and `TP4`.

The cluster `TP1`, `TP8`, `TP9`, `TP10`, `TP11`, and `TP12` is primarily useful as a set of distributed ground probe attachment points.

Use these features when checking fast edges on the trigger, clock, or output paths rather than relying on long clip leads.

## Bring-up checklist

For a first hardware check, record at least:

1. board revision and assembly state
2. which optional parts are populated
3. jumper and termination positions
4. power rails verified at testpoints
5. LED behavior at idle and during a short test
6. exact host commands used during the check

## Troubleshooting hints

If a board-level experiment is not behaving as expected:

* confirm the intended connector path first
* confirm optional components are actually populated
* confirm trigger threshold and termination settings
* confirm the clock source and jumper state
* use the LEDs and testpoints before assuming a software problem

See also [PP_PMOD triggering](pp_pmod_triggering.md) and [PP_PMOD clocking and PPS](pp_pmod_clocking.md).
