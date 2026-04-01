# PP_PMOD Outputs

The main digital output side of `PP_PMOD` consists of a 32-bit output bus grouped into four 8-bit connector blocks, plus two buffered SMA output channels for instrument hookup.

## QOUT overview

The `QOUT` schematic sheet carries the primary PulsePins output bus `Q0..Q31`.

Those 32 bits are grouped into four 8-bit connector blocks to make experimentation with PMOD-style modules, LEDs, logic analyzers, and custom daughterboards straightforward.

## QOUT connector mapping

| Connector | Signals |
| --- | --- |
| `J3` | `Q0..Q7` |
| `J4` | `Q8..Q15` |
| `J5` | `Q16..Q23` |
| `J6` | `Q24..Q31` |

The signal grouping on the four 2x6 output connectors is:

| Connector | Odd-row data pins | Even-row data pins |
| --- | --- | --- |
| `J3` | `Q0`, `Q1`, `Q2`, `Q3` | `Q4`, `Q5`, `Q6`, `Q7` |
| `J4` | `Q8`, `Q9`, `Q10`, `Q11` | `Q12`, `Q13`, `Q14`, `Q15` |
| `J5` | `Q16`, `Q17`, `Q18`, `Q19` | `Q20`, `Q21`, `Q22`, `Q23` |
| `J6` | `Q24`, `Q25`, `Q26`, `Q27` | `Q28`, `Q29`, `Q30`, `Q31` |

Each 2x6 connector also includes utility power/ground pins alongside the eight data signals.

The same sheet also breaks out several output-related control and observation signals that are useful during integration.

## Output control and status signals

| Signal | Purpose |
| --- | --- |
| `OE` | output-enable related control path |
| `STROBE` | output timing strobe |
| `QOUT_VALID` | valid indicator for output activity |
| `STREAMER_CLK` | active streamer clock exported to the shield |

These sideband signals are presented on the dedicated 1x6 header `J8`, together with utility power/ground pins.

Physical order on `J8`, top to bottom in the schematic:

| Position | Signal |
| --- | --- |
| top | `STROBE` |
| 2 | `OE` |
| 3 | `STREAMER_CLK` |
| 4 | `QOUT_VALID` |
| 5 | `GND` |
| bottom | `+3.3V` |

These signals are especially useful during board bring-up and when aligning output timing to external equipment.

## Buffered SMA outputs

The `output buffers` sheet routes two board signals to SMA connectors through logic buffers.

Documented facts from the schematic:

* there are two SMA output connectors
* `J13` is tied to the buffered `Q0` path
* `J14` is tied to the buffered `Q1` path
* the path is buffered
* indicator LEDs are present on the output-buffer section

The documentation intentionally uses "buffered SMA outputs" here. If you need to claim a specific source impedance or exact 50 ohm driver behavior for a publication or assembly note, verify and document that explicitly from the circuit and measurement setup.

## Measurement notes

The buffered SMA outputs are useful for:

* oscilloscopes
* logic analyzers
* counters
* spectrum analyzers

For repeatable measurements, record:

* output connector used
* whether the measurement is on buffered `Q0` or buffered `Q1`
* instrument input impedance
* cable type and length
* output-enable settings
* sequence and clock settings

## Validation with `ppqout`

Useful host-side tools:

* [`ppqout`](ppqout.md) for combiner and output-path setup
* [`pptest`](pptest.md) for visible QOUT exercises such as LED PMOD tests

See also [PP_PMOD connectors and pinout](pp_pmod_connectors.md).
