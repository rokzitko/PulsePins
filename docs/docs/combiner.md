# Combiner subsystem

PulsePins can combine the outputs of multiple streamer instances before they reach the external pins.

The main hardware blocks are:

* [`ip/combiner/combiner.sv`]({{ source_file("ip/combiner/combiner.sv") }}) - output-data combiner
* [`ip/combiner_trig/combiner_trig.sv`]({{ source_file("ip/combiner_trig/combiner_trig.sv") }}) - trigger-signal combiner
* [`ip/combiner_comb/`]({{ source_file("ip/combiner_comb/") }}) - combinational variant of the output combiner

The main C++ interfaces are:

* [`c++/combiner.hh`]({{ source_file("c++/combiner.hh") }})
* [`c++/qout.hh`]({{ source_file("c++/qout.hh") }})
* [`c++/trigger.hh`]({{ source_file("c++/trigger.hh") }})

For a maintainer-oriented RTL map, see [`ip/combiner/README.md`]({{ source_file("ip/combiner/README.md") }}).

### Output combiner

The output combiner accepts four input buses and produces one output bus.

The hardware supports:

* input selection (`SEL1` .. `SEL4`)
* bitwise logic (`AND`, `OR`, `XOR`, `XNOR`)
* majority vote (`MAJ`)
* block concatenation (`BLOCK8`, `BLOCK16`)
* arithmetic composition (`SUM12`, `SUM1234`, `DIFF12`)
* per-input and output inversion masks
* per-input and output bit masks
* forced values on each input and on the final output
* readback of either configured force values or live port values

The output combiner modes are:

| Mode | Meaning |
| ---- | ------- |
| `SEL1` .. `SEL4` | forward one selected input |
| `AND` / `OR` / `XOR` / `XNOR` | bitwise logical combination |
| `MAJ` | per-bit majority vote across inputs |
| `BLOCK8` | concatenate 8-bit slices from all four inputs |
| `BLOCK16` | concatenate 16-bit slices from inputs 1 and 2 |
| `SUM12` | arithmetic sum of inputs 1 and 2 |
| `SUM1234` | arithmetic sum of all four inputs |
| `DIFF12` | arithmetic difference of inputs 1 and 2 |

Both inputs and outputs are registered in [`ip/combiner/combiner.sv`]({{ source_file("ip/combiner/combiner.sv") }}), so the datapath is intentionally not purely combinational.

The registered design has two practical consequences:

* combination behavior is stable and easy to reason about at clock boundaries
* the combiner adds latency, so it should be treated as part of the timed datapath rather than as transparent glue logic

For the cross-IP latency summary, see [RTL latency and timing](latency.md).

### Control model

The combiner is controlled through an Avalon-MM register file.

The register map is organized around:

* one configuration register
* one inversion register per input and output
* one mask register per input and output
* one value register per input and output

The Avalon address groups used by the RTL are:

| Address group | Purpose |
| ------------- | ------- |
| `C_CFG` | mode, force bits, readback-selection bits |
| `C_INV*` | inversion masks |
| `C_MASK*` | pass masks |
| `C_VALUE*` | forced-value storage or live-port readback |

The configuration register carries both the selected mode and a set of force/readback control bits.

The most important config-bit groups are:

| Bit group | Purpose |
| --------- | ------- |
| mode bits `[3:0]` | selected combiner mode |
| `B_FORCE*` | choose forced value instead of normal datapath input/output |
| `B_RB*` | choose live port readback instead of stored forced value |

The C++ `combiner` class in [`c++/combiner.hh`]({{ source_file("c++/combiner.hh") }}) exposes the main operations:

* `mode()` - choose the combination mode
* `invert()` - configure inversion masks
* `mask()` - configure pass masks
* `value()` - preload forced values
* `force()` - preload and enable a forced input/output value
* `out()`, `in1()`, `in2()`, `in3()`, `in4()` - read back live port values
* `report()` - dump a full current configuration summary

The `force()` helper is intentionally two-step under the hood: it writes the stored value first and only then enables forcing. This avoids exposing a partially updated forced value.

There are two readback views:

* forced-value readback - inspect the configured override registers
* port-value readback - inspect the live values at the combiner ports

The helper methods `rb_force()` and `rb_port()` switch between those two views.

### Output semantics

The processing order in the output combiner is:

1. optionally replace an input with its forced value
2. apply inversion to that input
3. apply the input mask
4. compute the selected combine operation
5. optionally replace the final output with the forced output value

This means the output-level force bypasses the normal inversion and mask path for the final output.

For input ports, force happens before inversion and masking.

That ordering is important when debugging: if a value looks wrong at the output, the likely places to inspect are force settings first, then inversion masks, then pass masks, and finally the selected combine mode.

### High-level output control

[`c++/qout.hh`]({{ source_file("c++/qout.hh") }}) wraps the output combiner for use from command-line tools.

It maps command-line switches to combiner configuration, for example:

* `-out_sel1`, `-out_sel2`, `-out_sel3`, `-out_sel4`
* `-out_and`, `-out_or`, `-out_xor`, `-out_xnor`, `-out_maj`
* `-out_block8`, `-out_block16`
* `-out_sum12`, `-out_sum1234`, `-out_diff12`
* `-invert1`, `-mask1`, `-force1` and analogous options for other ports
* `-invert_out`, `-mask_out`, `-force_out`

This is the layer used by tools such as `ppqout`.

The helper also defaults to `SEL1` if no explicit output mode is requested, which keeps the single-streamer case simple.

### Trigger combiner

The trigger combiner applies the same general pattern to trigger sources.

Its inputs are used as logical trigger groups rather than full-width waveform buses:

* internal trigger signals
* external trigger inputs
* miscellaneous trigger sources

[`c++/trigger.hh`]({{ source_file("c++/trigger.hh") }}) maps command-line switches such as:

* `-trig_int`, `-trig_ext`, `-trig_misc`
* `-trig_any`, `-trig_all`
* `-invert_int`, `-invert_ext`, `-invert_misc`
* `-mask_int`, `-mask_ext`, `-mask_misc`

The trigger combiner is used to decide which trigger source or logical combination arms the streamer path.

Compared with the output combiner, the trigger combiner uses the same configuration style but a reduced practical feature set: in the RTL only the selection and basic logical-combination modes are meaningful for trigger groups.

The trigger-side width is narrower than the data-output combiner width because it carries grouped trigger/control signals rather than a full output data bus.

### When to use it

Use the combiner subsystem when:

* multiple streamer cores are active at once
* outputs need to be composed without modifying the sequences themselves
* trigger sources must be selected or masked at runtime
* temporary override values are useful for bring-up or debugging

It is also useful when you want to separate sequence generation from final output routing. In that workflow, the streamers produce independent candidate outputs while the combiner decides late in the path how those outputs are merged or selected.

### Related pages

* [`ppqout.md`](ppqout.md)
* [`pptrig.md`](pptrig.md)
* [`latency.md`](latency.md)
* [`cpp.md`](cpp.md)
