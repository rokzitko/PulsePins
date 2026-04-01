# PP_PMOD Reference Shield

`PP_PMOD` is the reference KiCad shield design for attaching PulsePins to the DE10-Nano GPIO headers and bringing the FPGA I/O out to lab-friendly connectors.

It is an optional hardware profile rather than a required baseline. The board is intended both as a usable shield and as a starting point for derivative designs with different connectors, buffering, or peripheral choices.

## Revision and source

The current top-level KiCad project is `pcb/ppshield_pmod/shield.kicad_sch` with title `PP_PMOD: GPIO shield for DE10-Nano`, revision `1`, and date `2025-09-20`.

Project files:

* KiCad sources: [`pcb/ppshield_pmod/`](https://github.com/rokzitko/PulsePins/tree/main/pcb/ppshield_pmod)
* Top-level schematic: [`shield.kicad_sch`](https://github.com/rokzitko/PulsePins/blob/main/pcb/ppshield_pmod/shield.kicad_sch)
* PCB layout: [`shield.kicad_pcb`](https://github.com/rokzitko/PulsePins/blob/main/pcb/ppshield_pmod/shield.kicad_pcb)

![PP_PMOD board](img/IMG_0064.jpeg){: style="height:400px"}

## Feature summary

The reference design includes:

* four 8-bit output connector groups carrying `Q0..Q31`
* one 8-bit auxiliary connector carrying `AUX0..AUX7`
* a trigger-input connector group plus a separate SMA trigger input path
* two buffered SMA outputs for instrument connection
* SMA inputs for `EXT_CLK` and `PPS_IN`
* a Qwiic-compatible I2C connector for external modules
* an onboard `MCP9808` temperature sensor
* an onboard `AD5693` DAC with a separate low-noise regulator
* status, activity, and heartbeat LEDs
* testpoints and probe-grounding features
* optional oscillator-module footprints and optional input terminations

All of the external connector-facing signal groups in the design are protected with ESD devices.

## Board architecture

The KiCad hierarchy is already a good map of the board:

* `GPIO` connects the shield to the DE10-Nano headers
* `QOUT` handles the main 32-bit output bus and related control/status lines
* `output buffers` drives the two SMA output channels
* `Triggering` handles trigger connectors, trigger control/status, and the thresholded SMA trigger path
* `ext_clk` handles `EXT_CLK`, `PPS_IN`, optional termination, and oscillator-module options
* `I2C` contains the Qwiic connector, onboard `MCP9808`, and onboard `AD5693`
* `AUX` brings out the auxiliary bus
* `LEDs` drives the board indicators
* `Misc` contains testpoints and grounding aids

## Optional features

Several parts of the board are optional or configuration-dependent:

* the onboard DAC path is optional
* the onboard temperature sensor is optional
* the oscillator footprints are optional
* some terminations are selectable or optional
* the exact shield assembly may differ from the fully populated schematic

When documenting or validating the board, record which optional parts are populated and which jumper positions are used.

## Related pages

* [PP_PMOD connectors and pinout](pp_pmod_connectors.md)
* [PP_PMOD triggering](pp_pmod_triggering.md)
* [PP_PMOD clocking and PPS](pp_pmod_clocking.md)
* [PP_PMOD outputs](pp_pmod_outputs.md)
* [PP_PMOD I2C and onboard peripherals](pp_pmod_i2c.md)
* [PP_PMOD LEDs, jumpers, and testpoints](pp_pmod_service.md)
* [PP_PMOD validated examples](pp_pmod_examples.md)
