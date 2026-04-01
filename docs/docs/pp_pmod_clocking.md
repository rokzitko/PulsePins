# PP_PMOD Clocking And PPS

The `PP_PMOD` shield provides dedicated external timing inputs for `EXT_CLK` and `PPS_IN`, plus optional onboard oscillator-module footprints.

These features make the board useful both for simple external-reference experiments and for GPSDO-style workflows.

## Clocking overview

The clock-related hardware on the board is centered around:

* one SMA input for `EXT_CLK` on `J9`
* one SMA input for `PPS_IN` on `J26`
* optional terminations
* optional oscillator-module footprints
* exported internal board signals `PPCLK1` and `PPCLK2`

## `EXT_CLK` input

The external clock path lives on the `ext_clk` schematic sheet and includes:

* SMA connector `J9`
* external-facing protection `U20`
* a clock buffer stage `U19`
* an optional 50 ohm termination resistor `R17`
* nearby 2-pin configuration headers `J20` and `J25`, plus 0 ohm links on the clock path
* routing into the board clock-selection logic

Unlike the SMA trigger input, this path does not include an adjustable threshold comparator.

## `PPS_IN` input

`PPS_IN` is brought in through SMA connector `J26` on the same clocking sheet.

The PPS side also has:

* external-facing protection `U21`
* an optional 50 ohm termination resistor `R6`
* a dedicated monitoring LED path on the clocking sheet

`J20` is an in-series 2-pin patch/disconnect point in the `EXT_CLK` path between the external-input side and the downstream clocking logic.

`J25` is the matching in-series 2-pin patch/disconnect point in the `PPS_IN` path.

As with the external clock path, treat `J20`, `J25`, and the 0 ohm links as schematic-controlled configuration points rather than fixed user-facing defaults.

The board provides a practical PPS entry point for:

* timestamp validation
* clock comparison against GNSS-derived references
* GPSDO experiments where PPS timing and DAC control are combined

## Optional oscillator modules

The `ext_clk` sheet also contains footprints for optional oscillator modules.

These are active CMOS oscillator-module footprints, not bare crystal resonators. When describing the board, prefer wording such as "optional oscillator modules" or "optional CMOS oscillator modules" rather than "crystal oscillators".

The two oscillator footprints shown in the schematic are:

* `Y1`: `SG-5032CBN`
* `Y2`: `SG-7050CAN`

## Clock-related exported signals

The shield schematic exports `PPCLK1` and `PPCLK2` through the FPGA-side header mapping.

These signals are board-internal clocking resources that are relevant when tracing the shield wiring or adapting the design for a derivative board.

## Typical external sources

The documented or plausible external timing sources for this board include:

* lab signal generators
* OCXO or GPSDO references
* GNSS receiver PPS outputs
* populated oscillator modules on the board itself

## Validation with `ppfreq` and `ppts`

Useful host-side tools:

* [`ppfreq`](ppfreq.md) for external clock checks
* [`ppts`](ppts.md) for PPS capture checks
* [`ppgpsdo`](ppgpsdo.md) for combined PPS and DAC workflows

Record these details when validating a timing setup:

* source type and nominal frequency
* SMA cabling
* jumper or termination settings
* whether an onboard oscillator module is populated
* exact commands and measured results

See also [PP_PMOD I2C and onboard peripherals](pp_pmod_i2c.md) for the DAC used in GPSDO-style experiments.
