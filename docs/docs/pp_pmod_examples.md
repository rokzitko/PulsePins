# PP_PMOD Validated Examples

This page collects board-specific example ideas and validation workflows for `PP_PMOD`.

The emphasis is on examples that state the hardware assumptions clearly enough that another contributor can reproduce them.

## How to read these examples

For every example, record:

* board revision and population
* external modules used
* jumper and termination settings
* external wiring
* exact command line
* expected and observed behavior

## LED PMOD example

Use one of the output connector groups with a simple LED-oriented PMOD or test fixture.

Suggested tools:

* [`pptest`](pptest.md)

Good things to record:

* which QOUT connector is used
* bit mapping on the fixture
* sequence used to generate the LED pattern

## Onboard `MCP9808` example

If the onboard temperature sensor is populated, use:

* [`pptemp`](pptemp.md)
* `I2C/mcp9808.py`

This is the correct board-native temperature example for `PP_PMOD`.

## External `TMP117` example

If a `TMP117` is attached through the Qwiic connector, use:

* `I2C/tmp117.py`

Document it explicitly as an external Qwiic module rather than as an onboard peripheral.

## `AD5693` DAC example

If the onboard DAC is populated, a simple fixed-voltage or swept-voltage example can be built around:

* `I2C/ad5693_set_vout.py`
* [`ppgpsdo`](ppgpsdo.md) for DAC-backed timing control workflows

## PPS validation example

Use a GNSS receiver PPS output or another suitable PPS source on `PPS_IN`.

Suggested tools:

* [`ppts`](ppts.md)

Record the PPS source, cable, level assumptions, and measured stability.

## External clock validation example

Drive `EXT_CLK` from a known source such as a signal generator or OCXO-derived reference.

Suggested tools:

* [`ppfreq`](ppfreq.md)

Record the nominal frequency, source type, any enabled termination, and measured result.

## Trigger experiment example

Try both of these paths separately:

* digital trigger bus on the trigger connector
* thresholded SMA trigger path

Suggested tools:

* [`pptrig`](pptrig.md)
* [`pptest`](pptest.md)

Record the source waveform, threshold setting, and whether the digital or SMA trigger path was used.
