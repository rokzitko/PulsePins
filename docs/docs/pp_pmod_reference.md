# PP_PMOD Hardware Reference

This page is the compact hardware reference for `PP_PMOD`.

For board purpose, project links, and example workflows, see [PP_PMOD reference shield](pp_pmod.md).

## Conventions

The design uses 3.3 V logic on the shield-side digital interfaces.

The connector and signal names below follow the KiCad schematics. For final pin-by-pin verification, the KiCad sheets remain the source of truth.

## Connector inventory

| Ref | Interface | Type | Function |
| --- | --- | --- | --- |
| `J3` | `QOUT_0-7` | 2 × 6 right-angle socket | output bits `Q0..Q7` |
| `J4` | `QOUT_8-15` | 2 × 6 right-angle socket | output bits `Q8..Q15` |
| `J5` | `QOUT_16-23` | 2 × 6 right-angle socket | output bits `Q16..Q23` |
| `J6` | `QOUT_24-31` | 2 × 6 right-angle socket | output bits `Q24..Q31` |
| `J7` | `TRIG_IN` | 2 × 6 right-angle socket | trigger inputs `TRIG_IN0..TRIG_IN7` |
| `J8` | QOUT sideband | 1 × 6 right-angle socket | output control/status breakout |
| `J9` | `EXT_CLK` | SMA | external clock input |
| `J10` | trigger control | 1 × 6 vertical socket | trigger control breakout |
| `J11` | trigger status | 1 × 6 vertical socket | trigger status/service breakout |
| `J12` | `AUX` | 2 × 6 right-angle socket | auxiliary bus `AUX0..AUX7` |
| `J13` | buffered output | SMA | buffered `Q0` output |
| `J14` | buffered output | SMA | buffered `Q1` output |
| `J17` | trigger SMA | SMA | thresholded trigger input |
| `J18` | Qwiic-compatible I2C | JST-SH 1 × 4 | external I2C module connection |
| `J26` | `PPS_IN` | SMA | pulse-per-second input |

The QOUT, AUX, and trigger bus connectors use the same 2 × 6 mechanical style as a [Pmod-style expansion connector](https://digilent.com/reference/_media/reference/pmod/pmod-interface-specification-1_3_1.pdf), but the clock and trigger-control breakouts are simple headers rather than standard Pmod ports.

## QOUT And AUX

The four output connectors divide the 32-bit output bus into four 8-bit groups:

| Connector | Bits |
| --- | --- |
| `J3` | `Q0..Q7` |
| `J4` | `Q8..Q15` |
| `J5` | `Q16..Q23` |
| `J6` | `Q24..Q31` |

Signal grouping on the four 2 × 6 output connectors:

| Connector | Odd-row data pins | Even-row data pins |
| --- | --- | --- |
| `J3` | `Q0`, `Q1`, `Q2`, `Q3` | `Q4`, `Q5`, `Q6`, `Q7` |
| `J4` | `Q8`, `Q9`, `Q10`, `Q11` | `Q12`, `Q13`, `Q14`, `Q15` |
| `J5` | `Q16`, `Q17`, `Q18`, `Q19` | `Q20`, `Q21`, `Q22`, `Q23` |
| `J6` | `Q24`, `Q25`, `Q26`, `Q27` | `Q28`, `Q29`, `Q30`, `Q31` |

Each 2 × 6 connector also includes utility power and ground pins.

`J12` exports the auxiliary bus:

| Row | Signals |
| --- | --- |
| odd pins | `AUX3`, `AUX2`, `AUX1`, `AUX0`, then utility pins |
| even pins | `AUX7`, `AUX6`, `AUX5`, `AUX4`, then utility pins |

QOUT sideband signals are presented on `J8`:

| Position | Signal |
| --- | --- |
| 1 | `STROBE` |
| 2 | `OE` |
| 3 | `STREAMER_CLK` |
| 4 | `QOUT_VALID` |
| 5 | `GND` |
| 6 | `+3.3V` |

`OE` is the physical output enable, `QOUT_VALID` is the `qout_valid` sample qualifier, and `STROBE` carries the `qout_strobe` sampling pulse.

Buffered SMA outputs:

* `J13` is tied to the buffered `Q0` path.
* `J14` is tied to the buffered `Q1` path.

The documentation intentionally uses "buffered SMA outputs" here. If you need to claim a specific source impedance or exact 50 Ω driver behavior, verify and document that explicitly from the circuit and measurement setup.

## Triggering

`PP_PMOD` exposes the trigger subsystem in two ways:

* an 8-bit digital trigger bus on `J7`
* a dedicated SMA trigger input on `J17` with comparator threshold control

Trigger bus grouping on `J7`:

| Row | Signals |
| --- | --- |
| odd pins | `TRIG_IN3`, `TRIG_IN2`, `TRIG_IN1`, `TRIG_IN0`, then utility pins |
| even pins | `TRIG_IN7`, `TRIG_IN6`, `TRIG_IN5`, `TRIG_IN4`, then utility pins |

Trigger control header `J10`, positions `1` through `6` in the schematic:

| Position | Signal |
| --- | --- |
| 1 | `+3.3V` |
| 2 | `GND` |
| 3 | `GATE_IN` |
| 4 | `TRIG_RESET` |
| 5 | `TRIG_FORCE` |
| 6 | `TRIG_ENABLE` |

`GATE_IN` is the external gate input that permits or blocks streamer output advancement.

Trigger status header `J11`, positions `1` through `6` in the schematic:

| Position | Signal |
| --- | --- |
| 1 | `+3.3V` |
| 2 | `GND` |
| 3 | `BUFFER_ERROR` |
| 4 | `DONE` |
| 5 | `TRIG_ACTIV` |
| 6 | `TRIG_ARMED` |

The SMA trigger path includes:

* SMA connector `J17`
* ESD protection
* optional input termination
* `ADCMP604` comparator
* a trimmer potentiometer for the threshold
* a monitoring LED

This threshold control belongs to the trigger input path. It is not part of the `EXT_CLK` path.

Trigger-side configuration points:

* `J16` is an in-series 2-pin patch/disconnect point on the `TRIG_IN0` path.
* `J19` is an in-series 2-pin patch/disconnect point on the SMA trigger-signal path that also reaches `J17`.

## Clocking And PPS

The clock-related hardware is centered around:

* SMA input `J9` for `EXT_CLK`
* SMA input `J26` for `PPS_IN`
* optional terminations
* optional oscillator-module footprints
* exported internal timing signals `PPCLK1` and `PPCLK2`

`EXT_CLK` path:

* `J9`
* protection device `U20`
* clock buffer `U19`
* optional 50 Ω termination resistor `R17`
* in-series 2-pin patch/disconnect header `J20`

`PPS_IN` path:

* `J26`
* protection device `U21`
* optional 50 Ω termination resistor `R6`
* monitoring LED path
* in-series 2-pin patch/disconnect header `J25`

The `ext_clk` sheet also contains optional oscillator-module footprints:

* `Y1`: `SG-5032CBN`
* `Y2`: `SG-7050CAN`

These are active CMOS oscillator modules, not bare crystal resonators.

## I2C And Onboard Peripherals

The `I2C` sheet serves two roles:

* external I2C via the Qwiic-compatible connector `J18`
* optional onboard peripherals: `MCP9808` and `AD5693`

[Qwiic](https://www.sparkfun.com/qwiic) is SparkFun's 4-pin JST-SH connector and cable ecosystem for I2C modules. The external connector shares the shield's 3.3 V I2C bus, so attached modules should be compatible with the bus voltage and address plan. For protocol background, see NXP's [I2C-bus specification and user manual](https://www.nxp.com/docs/en/user-guide/UM10204.pdf).

`J18` pin order:

| Pin | Signal |
| --- | --- |
| 1 | `GND` |
| 2 | `+3.3V` |
| 3 | `SDA` |
| 4 | `SCL` |

Onboard devices:

* onboard temperature sensor: `MCP9808`
* onboard DAC: `AD5693`

External examples should describe `TMP117` as an external Qwiic module, not as the onboard temperature sensor.

## LEDs, Test Points, And Service Notes

Indicator LEDs include:

* `TRIG_ARMED`
* `TRIG_ACTIV`
* `DONE`
* `BUFFER_ERROR`
* `ACTIVITY`
* `HEARTBEAT`

Named test points:

* `Misc` sheet: `TP1`, `TP2`, `TP3`, `TP4`, `TP8`, `TP9`, `TP10`, `TP11`, `TP12`
* `I2C` sheet: `TP5`, `TP6`, `TP7`

Confirmed test point roles:

| Ref | Sheet | Role |
| --- | --- | --- |
| `TP1` | `Misc` | ground probe point |
| `TP2` | `Misc` | ground loop-style probe point |
| `TP3` | `Misc` | `TRIG_ACTIV` |
| `TP4` | `Misc` | `STROBE` |
| `TP8` | `Misc` | ground probe point |
| `TP9` | `Misc` | ground probe point |
| `TP10` | `Misc` | ground probe point |
| `TP11` | `Misc` | ground probe point |
| `TP12` | `Misc` | ground probe point |
| `TP5` | `I2C` | DAC `VOUT` |
| `TP6` | `I2C` | DAC `VREF` |
| `TP7` | `I2C` | local regulated `+3.3 V` |

The cluster `TP1`, `TP8`, `TP9`, `TP10`, `TP11`, and `TP12` is primarily useful as distributed scope ground attachment points. `TP5` is the main analog observation point in the DAC subsection.

When documenting a setup, record:

* optional parts populated
* jumper and termination positions
* signal source and cabling
* exact commands used
* observed LED and test point behavior
