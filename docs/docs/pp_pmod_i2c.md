# PP_PMOD I2C And Onboard Peripherals

The `I2C` sheet on `PP_PMOD` serves two roles:

* it provides a Qwiic-compatible connector for external modules
* it hosts two optional onboard peripherals: an `MCP9808` temperature sensor and an `AD5693` DAC

## I2C overview

The shield carries the I2C signals `SCL` and `SDA` together with `+3.3V` and `GND` to the external Qwiic connector.

The same bus also serves the optional onboard peripherals.

## Qwiic connector

`J18` is the board's JST-SH 1x4 Qwiic-style connector.

The schematic pin order is:

| Pin | Signal |
| --- | --- |
| 1 | `GND` |
| 2 | `+3.3V` |
| 3 | `SDA` |
| 4 | `SCL` |

It is intended for external modules such as:

* temperature sensors
* simple environmental sensors
* low-speed support hardware used during lab bring-up

When documenting experiments that use this connector, say explicitly whether the I2C device is onboard or external.

## Onboard temperature sensor

The onboard temperature sensor shown in the schematic is `MCP9808`, not `TMP117`.

Relevant software/docs:

* [`pptemp`](pptemp.md)
* `I2C/mcp9808.py`
* `I2C/mcp9808-once.py`

This is the correct board-specific temperature-monitoring path for `PP_PMOD` when the sensor is populated.

## Onboard DAC

The onboard DAC shown in the schematic is `AD5693`.

The design also includes a dedicated low-noise regulator for the DAC path, which makes the DAC suitable for tasks such as slow oscillator tuning and GPSDO experiments.

Relevant software/docs:

* `I2C/ad5693_set_vout.py`
* [`ppgpsdo`](ppgpsdo.md)

## External Qwiic modules

A `TMP117` can still be a perfectly valid example device on `PP_PMOD`, but it should be described as an external Qwiic module rather than as the onboard temperature sensor.

Relevant software:

* `I2C/tmp117.py`

## Software and tooling links

Useful software around this bus includes:

* [`pptemp`](pptemp.md) for periodic temperature reads using the onboard `MCP9808`
* `I2C/mcp9808.py` for direct onboard sensor access
* `I2C/ad5693_set_vout.py` for direct DAC control
* `I2C/tmp117.py` for an external `TMP117` module

## Onboard vs external clarification

Use the wording below consistently:

* onboard temperature sensor: `MCP9808`
* onboard DAC: `AD5693`
* external Qwiic example sensor: `TMP117`

This distinction avoids one of the main sources of confusion in the current documentation set.
