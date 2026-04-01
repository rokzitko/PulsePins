# PP_PMOD Connectors And Pinout

This page is the board-level connector inventory for `PP_PMOD`.

For detailed signal behavior, use the functional pages alongside this inventory:

* [triggering](pp_pmod_triggering.md)
* [clocking and PPS](pp_pmod_clocking.md)
* [outputs](pp_pmod_outputs.md)
* [I2C and onboard peripherals](pp_pmod_i2c.md)

## Conventions

The design uses 3.3 V logic on the shield-side digital interfaces.

The connector descriptions below use the exact net-group names from the schematic where possible. For pin-by-pin routing details, the KiCad sheets remain the source of truth.

## DE10-Nano host interface

The shield plugs into both 40-pin DE10-Nano GPIO headers and routes PulsePins I/O through the `GPIO` schematic sheet.

Important board-level signals exported from the FPGA side include:

* `Q0..Q31`
* `AUX0..AUX7`
* `TRIG_IN0..TRIG_IN7`
* `TRIG_ENABLE`, `TRIG_FORCE`, `TRIG_RESET`
* `TRIG_ARMED`, `TRIG_ACTIV`
* `OE`, `STROBE`, `QOUT_VALID`, `STREAMER_CLK`
* `PPCLK1`, `PPCLK2`
* `EXT_CLK`, `PPS_IN`

## Connector inventory

| Ref | Interface | Type | Function |
| --- | --- | --- | --- |
| `J3` | `QOUT_0-7` | 2x6 right-angle socket | output bits `Q0..Q7` |
| `J4` | `QOUT_8-15` | 2x6 right-angle socket | output bits `Q8..Q15` |
| `J5` | `QOUT_16-23` | 2x6 right-angle socket | output bits `Q16..Q23` |
| `J6` | `QOUT_24-31` | 2x6 right-angle socket | output bits `Q24..Q31` |
| `J8` | QOUT sideband | 1x6 right-angle socket | output control/status breakout |
| `J12` | `AUX` | 2x6 right-angle socket | auxiliary bus `AUX0..AUX7` |
| `J7` | `TRIG_IN` | 2x6 right-angle socket | trigger inputs `TRIG_IN0..TRIG_IN7` |
| `J10` | trigger control/status | 1x6 vertical socket | trigger control/status breakout |
| `J9` | `EXT_CLK` | SMA | external clock input |
| clocking SMA on `ext_clk` sheet | `PPS_IN` | SMA | pulse-per-second input |
| `J13` | buffered output | SMA | buffered output channel |
| `J14` | buffered output | SMA | buffered output channel |
| `J18` | Qwiic-compatible I2C | JST-SH 1x4 | external I2C module connection |

The QOUT, AUX, and trigger bus connectors use the same 2x6 mechanical style as a PMOD-style expansion connector, but not every other control breakout on the board is a PMOD connector. In particular, the clock and trigger-control breakouts are simple headers rather than standard PMOD ports.

## Output connectors

The four output connectors divide the 32-bit output bus into four 8-bit groups:

| Connector | Bits |
| --- | --- |
| `J3` | `Q0..Q7` |
| `J4` | `Q8..Q15` |
| `J5` | `Q16..Q23` |
| `J6` | `Q24..Q31` |

Within each 2x6 connector, eight positions carry the named data bits and the remaining positions are utility power/ground pins as shown in the schematic.

The output-control and output-status signals are brought out on `J8`:

| Connector | Signals |
| --- | --- |
| `J8` | `QOUT_VALID`, `STREAMER_CLK`, `OE`, `STROBE`, plus utility power/ground pins |

Related output-control and output-status signals are documented on [PP_PMOD outputs](pp_pmod_outputs.md).

## Trigger connectors

The trigger interfaces are split into a bus connector and a control/status header:

| Connector | Signals |
| --- | --- |
| `J7` | `TRIG_IN0..TRIG_IN7`, plus utility power/ground pins |
| `J10` | `TRIG_ENABLE`, `TRIG_FORCE`, `TRIG_RESET`, `TRIG_ARMED`, `TRIG_ACTIV`, `TRIG_NC` |

`J7` groups the trigger inputs by row:

| Row | Signals |
| --- | --- |
| odd pins | `TRIG_IN3`, `TRIG_IN2`, `TRIG_IN1`, `TRIG_IN0`, then utility pins |
| even pins | `TRIG_IN7`, `TRIG_IN6`, `TRIG_IN5`, `TRIG_IN4`, then utility pins |

The board also includes a separate SMA trigger input path with comparator threshold control; see [PP_PMOD triggering](pp_pmod_triggering.md).

## AUX connector

`J12` exports the auxiliary bus:

| Connector | Signals |
| --- | --- |
| `J12` | `AUX0..AUX7` |

The AUX connector grouping is:

| Row | Signals |
| --- | --- |
| odd pins | `AUX3`, `AUX2`, `AUX1`, `AUX0`, then utility pins |
| even pins | `AUX7`, `AUX6`, `AUX5`, `AUX4`, then utility pins |

This bus is useful for additional digital I/O, timestamp pairing experiments, or project-specific instrumentation.

## SMA connectors

| Connector | Function |
| --- | --- |
| `J9` | `EXT_CLK` |
| clocking SMA on `ext_clk` sheet | `PPS_IN` |
| trigger SMA on `Triggering` sheet | external thresholded trigger input |
| `J13` | buffered output channel |
| `J14` | buffered output channel |

The exact net assignment of the two SMA output channels should be taken from the `output buffers` sheet during hardware bring-up.

## Qwiic connector

`J18` is the board's 4-pin JST-SH Qwiic-style connector. Its schematic pin order is:

| Pin | Signal |
| --- | --- |
| 1 | `GND` |
| 2 | `+3.3V` |
| 3 | `SDA` |
| 4 | `SCL` |

This connector is meant for external I2C peripherals. The board's onboard I2C devices are documented separately on [PP_PMOD I2C and onboard peripherals](pp_pmod_i2c.md).

## Notes and caveats

* Treat the KiCad schematics as authoritative for final pin-by-pin wiring.
* Document jumper positions together with connector usage whenever an experiment depends on `EXT_CLK`, `PPS_IN`, or the SMA trigger path.
* When writing examples, separate onboard peripherals from external Qwiic modules.
