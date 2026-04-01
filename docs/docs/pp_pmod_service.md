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

When documenting a setup, record:

* whether the optional terminations are enabled
* whether an onboard oscillator module is populated
* any source-selection or configuration links relevant to `EXT_CLK`, `PPS_IN`, or the SMA trigger path

## Testpoints

The `Misc` and `I2C` sheets include dedicated testpoints that support board bring-up and troubleshooting.

Typical uses include:

* verifying local supply rails
* checking DAC or sensor-related nodes
* confirming trigger or timing-path activity
* attaching probes without disturbing the connector wiring under test

## Grounding features

The board includes dedicated ground connection points intended to make oscilloscope probing cleaner and more repeatable.

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
