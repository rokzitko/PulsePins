# PulsePins text sequence format

This page is the normative description of the PulsePins text sequence parser and writer in the current implementation. PulsePins text is the canonical user-facing authoring and interchange format for the normalized subset of `Sequence` elements that it can represent. It is not a raw dump of every possible internal `{control, count, value}` triplet.

The defining implementation is [`c++/sequence.hh`]({{ source_file("c++/sequence.hh") }}) together with the element mapping in [`c++/elements.hh`]({{ source_file("c++/elements.hh") }}). Numeric conversion uses [`c++/config.h`]({{ source_file("c++/config.h") }}), [`tidbit/misc.hh`]({{ source_file("tidbit/misc.hh") }}), and [`tidbit/parseVerilog.hh`]({{ source_file("tidbit/parseVerilog.hh") }}).

## Lexical structure

PulsePins text has no magic string, version line, or format header. The first non-whitespace token, if any, is a record token.

The parser extracts whitespace-separated tokens from an `std::istream`:

* All whitespace is equivalent. Spaces, tabs, carriage returns, and newlines only separate tokens.
* Line boundaries are insignificant. One record can span lines, and several records can share one line.
* There is no comment syntax. Tokens such as `#`, `//`, and their following text are parsed as ordinary tokens and cause an error when they occur where no such token or number is valid.
* Record tokens are lowercase and case-sensitive. For example, `d` is valid and `D` is not. Numeric base markers and hexadecimal digits have their own case-insensitive rules described below.
* There is no quoting or escaping syntax. The apostrophe in a Verilog/SystemVerilog numeric literal is part of that numeric token.
* Unknown tokens, incomplete records, malformed numbers, and numbers outside their destination range are errors. Extra tokens are not ignored; they are parsed as the start of another record.

An empty stream is valid and produces an empty `Sequence` with forced triggering disabled. A stream is also valid without an explicit `final` record. Parsing and execution preparation are separate; [playback final handling](#parsing-versus-playback) is described below.

## Record grammar

The metavariables used below are:

| Name | Meaning |
| ---- | ------- |
| `C` | regular or pseudo-random cycle count |
| `V` | output value or regular-operation operand |
| `I` | preprocessor storage slot |
| `OP` | one of the regular record tokens |
| `R` | replay repetition count |
| `L` | replay length |
| `P` | trigger pattern |
| `M` | trigger mask |

Every metavariable is one numeric token. The accepted numeric spellings do not change the record grammar.

### Regular records

A regular record has the form `OP C V`. For `C > 0`, the operation is applied once to the previous effective output `q`, and the result is emitted for `C` streamer-clock cycles.

| Token | Effective result | Output qualifier |
| ----- | ---------------- | ---------------- |
| `d C V` | `V` | normal `qout_valid` and `qout_strobe` |
| `dn C V` | `V` | updates `qout` but suppresses `qout_valid` and `qout_strobe` |
| `s C V` | `q | V` | normal |
| `c C V` | `q & ~V` | normal |
| `x C V` | `q ^ V` | normal |
| `n C V` | `~q` | normal; `V` is required and stored but is not used by this operation |
| `a C V` | `q & V` | normal |
| `o C V` | `q | V` | normal |
| `xr C V` | `q ^ V` | normal |
| `xn C V` | `~(q ^ V)` | normal |
| `sl C V` | `q << V` | normal logical left shift |
| `sr C V` | `q >> V` | normal logical right shift |

The operations use the current build's unsigned `value_t` width. `s` and `o` have the same effective bitwise operation but remain distinct encoded modes, as do `x` and `xr`.

### Store and replay

`store I OP C V` stores one regular element in fast-memory slot `I` instead of emitting it at that point. `OP` accepts exactly the regular tokens listed above, including `dn`. A `store` record cannot contain another `store` or a non-regular record.

The current build has eight slots, numbered `0` through `7`. The parser rejects an out-of-range slot. It does not verify that slots later referenced by replay have already been initialized.

`r R L` replays slots `0` through `L - 1`, in that order, `R` times. The current maximum `L` is `8`; a larger value is rejected. The special cases are:

* `R = 0` and `1 <= L <= 8` means infinite replay. Records after it are not reached while that replay continues.
* `L = 0` is accepted and the replay command is consumed as a no-op, including when `R = 0`.

### Trigger and control-flow records

| Record | Meaning |
| ------ | ------- |
| `tn P M` | add a non-final trigger-condition stage |
| `t P M` | add a final trigger-condition stage |
| `rt` | request retriggering and wait for the next trigger subsequence |
| `pr C` | emit one pseudo-random output value per cycle for `C` cycles |
| `final V` | terminate decoding and leave `V` as the persistent final output |
| `f` | return force-trigger metadata alongside the parsed `Sequence` |

For `t` and `tn`, each set bit in `M` selects a trigger input to compare with the corresponding bit in `P`. The current trigger pattern and mask width is eight bits. The `t` token marks the final stage of a trigger subsequence; it is unrelated to the terminal output record `final V` and to the command-line option `-t V`.

`rt` and `pr` have normalized implicit payloads in the internal representation: `rt` carries the default retrigger payload and `pr` carries value zero. Those payloads are not fields in the text grammar.

An explicit `final V` is a BITLOAD terminator and must be the last sequence element. Any later sequence element makes the input invalid. Because `f` is metadata rather than an element, `f` may appear before, between, or after complete element records, including after `final V`. Repeated `f` records are accepted and collapse to the single Boolean result `force_trigger = true`.

## Numeric literals

All numeric fields are unsigned. In the current main build, regular counts, replay repetitions, values, replay lengths, and the initially parsed store slot are 32-bit values. Trigger patterns and masks are 8-bit values. Additional semantic checks then restrict store slots to `0..7` and replay lengths to `0..8`.

The ordinary accepted forms are:

| Form | Example | Value |
| ---- | ------- | ----- |
| decimal | `42` | 42 |
| hexadecimal | `0x2a` or `0X2A` | 42 |
| binary | `0b101010` or `0B101010` | 42 |
| leading-zero octal | `052` | 42 |

Leading zero selects octal for a non-Verilog literal. Consequently, `010` is decimal 8 and `08` is invalid. A `0o` prefix is not accepted in the ordinary form.

Underscores are removed before conversion, so forms such as `1_000`, `0xff_ff`, and `0b1010_0001` are accepted. Underscore placement is not validated as a digit-separator rule; every underscore in the token is simply discarded before the remaining token is parsed.

Verilog/SystemVerilog-style integer forms are also accepted when the token contains an apostrophe:

* Sized forms: `8'hff`, `16'd42`, `12'o777`, `8'b1010_0101`
* Unsized based forms: `'hff`, `'d42`, `'o77`, `'b1010`
* Optional signed marker: `16'sd10`, `8'shff`
* Unbased unsized forms: `'0`, `'1`

Base letters and the `s` marker are case-insensitive. These forms are integer-literal parsing only, not a SystemVerilog expression evaluator. The current caveats are:

* Negative values are rejected. The Verilog/SystemVerilog path also rejects an explicit `+`; the ordinary non-apostrophe parser accepts a leading `+`.
* An explicit width truncates higher bits before the destination-type range check. For example, `4'hff` is accepted as `0xf`.
* The signed marker is accepted syntactically but the result is treated as an unsigned bit pattern. For example, `8'shff` is 255; there is no sign extension into the destination type.
* Unknown and high-impedance digits `x`, `z`, and `?` are recognized by the literal parser. Any such bits that remain after explicit-width truncation are rejected by numeric conversion; unknown bits truncated above the explicit width no longer participate in that check.
* In this parser, unbased unsized `'1` converts to integer 1 rather than expanding to all ones at the destination width.
* Floating-point values, units, arithmetic expressions, and a sign attached to a SystemVerilog literal are not accepted.

After all literal-width handling, the result must fit the field's C++ type. For the current build this means at most `0xffffffff` for 32-bit count/value fields and `0xff` for trigger pattern/mask fields.

## Zero-count distinctions

Zero does not have one universal meaning across record kinds:

| Record or field | Meaning of zero |
| --------------- | --------------- |
| regular `C = 0` | the element is consumed without emitting a cycle, and its update operation is not applied |
| `store I OP 0 V` | the zero-count regular element is stored; replaying it emits no cycles and applies no update |
| `pr 0` | emits no pseudo-random cycles |
| `r 0 L`, `L > 0` | repeats forever |
| `r R 0` | no replay, regardless of `R` |
| `t` or `tn` | trigger elements internally carry count zero but remain meaningful because they use the trigger-program path rather than the regular run-length decoder |

The terminal `final` and retrigger `rt` records do not expose a count in text.

## Exact example

The following is the complete content of a supported sequence. It has no header and no comments.

```text
store 0 d 4 0x0
store 1 d 4 0x1
r 3 0x2
pr 8
final 0x0
f
```

It stores a two-element waveform, replays it three times, emits eight pseudo-random cycles, selects zero as the explicit final output, and requests forced triggering.

## Canonical writer output

`write_sequence_to_stream(...)` and `write_sequence_to_file(...)` emit one element record per line, in sequence order, followed by `f` on its own final line when the separate force-trigger Boolean is true. Every emitted record ends with a newline. No header or comments are emitted.

The writer uses these spellings:

| Element | Output spelling |
| ------- | --------------- |
| regular | `OP C 0xV` |
| stored regular | `store I OP C 0xV` |
| replay | `r R 0xL` |
| trigger stage | `t 0xP 0xM` or `tn 0xP 0xM` |
| retrigger | `rt` |
| pseudo-random | `pr C` |
| BITLOAD final | `final 0xV` |
| force metadata | `f` after all element records |

`C`, `I`, and `R` are unpadded decimal. Values, replay lengths, trigger patterns, and trigger masks use unpadded lowercase hexadecimal with a `0x` prefix. Thus parsing and writing normalize numeric bases, numeric letter case, leading zeros, underscores, whitespace, record line layout, and the position or multiplicity of `f`.

The writer performs lexical normalization, not waveform optimization. It does not merge adjacent runs, apply regular operators, remove zero-count records, reorder sequence elements, or insert a terminator.

### Representable subset and writer limits

Every valid element produced by the text parser is writable, but the in-memory and binary models can contain elements that text cannot represent losslessly.

The writer rejects these cases:

* A no-strobe regular element whose mode is not BITLOAD. `dn` is the only no-strobe text spelling.
* A final element whose mode is not BITLOAD. This includes the internal BITOR-zero no-modify terminator used by default playback preparation.
* A regular element whose mode has no recognized text token.

Text has no raw-triplet escape. For otherwise writable elements, the writer renders only fields represented by the grammar:

* Regular records preserve the recognized operation, count, value, optional BITLOAD no-strobe state, and optional store slot. Other raw control decorations are not emitted.
* Trigger records expose only final/non-final status, pattern, and mask. They have no text count field, and value bits outside the packed pattern/mask fields are not emitted.
* Replay records expose repetitions and length but not unrelated raw control decorations.
* `rt` exposes no raw count or value payload.
* `pr` exposes its count but no raw value payload.
* `final` exposes its value but no raw count or unrelated control decorations.

Consequently, writer output is canonical for normalized parser-representable elements, not for arbitrary raw internal elements.

## Parsing versus playback

The parser does not require or synthesize a terminator. Empty input and non-empty input without `final V` both parse successfully.

The shared playback workflow in [`c++/ppworkflow.hh`]({{ source_file("c++/ppworkflow.hh") }}) prepares a working copy before transmission:

| Selected final policy | Prepared terminal element |
| --------------------- | ------------------------- |
| explicit terminal `final V` in text | use that BITLOAD final |
| command-line `-t V` | append a BITLOAD final with `V` |
| `-random_final` or `PP_RANDOM_FINAL` | append a BITLOAD final with a generated value |
| none of the above | append an internal BITOR-zero final, which leaves the decoder's current output unchanged |

An explicit text final conflicts with the external `-t` and random-final policies. The standalone `f` record controls force-versus-arm trigger activation only; it never selects a final output. Playback preparation leaves the caller's parsed or cached sequence unchanged.

The internal no-modify final is why a parsed file does not need to contain `final V` for normal playback. It is also outside the text writer's representable final subset because its mode is BITOR rather than BITLOAD.

## Execution context outside the format

PulsePins text describes sequence elements, not a complete hardware session. These settings are supplied by the executing tool or API:

| Context | Effect |
| ------- | ------ |
| streamer clock | regular and pseudo-random counts are clock cycles, not absolute time; the selected clock determines their duration |
| initial `qout` | supplies `q` before the first effective regular operation and therefore affects every relative operation until a load establishes a new state |
| trigger routing | selects, masks, and inverts the physical or internal sources seen by `t` and `tn` |
| force/arm policy | interprets parsed `f`, command options, or interface overrides when activating the trigger |
| gate routing | can pause output advancement independently of sequence text |
| final-output policy | can select `-t`, random final, browser-managed final output, or the default no-modify final during playback preparation |

Output enable, output-combiner configuration, clock source, timeout policy, and readback checking are also execution settings rather than text grammar. See the [Sequencer model](sequencer_model.md), [Sequence playback](ppplay.md), and [Streamer timing diagrams](streamer_timing.md) for those runtime relationships.

## Interface constraints

The same parser grammar is used when sequence text is carried through other interfaces. Those interfaces add transport or execution-policy constraints around the text:

| Interface | Constraint |
| --------- | ---------- |
| [ppscpi](ppscpi.md) `SEQ` | the entire line is limited to 64 KiB; the workstation Python client flattens multiline input to whitespace for that line, and semicolons cannot occur in the payload because the SCPI transport splits them into separate commands before `SEQ` parsing |
| [ppwebgui](ppwebgui.md) `/api/stream` | `sequence_text` is limited to 32 KiB and must be non-empty; browser execution rejects an explicit `final V` because it supplies the tracked idle `qout` as its external final-output policy |

These restrictions do not define alternate record spellings. The SCPI implementation is in [`c++/ppscpi.cc`]({{ source_file("c++/ppscpi.cc") }}) and [`c++/scpi_server.hh`]({{ source_file("c++/scpi_server.hh") }}); the web limits and final policy are in [`c++/ppwebgui_http.cc`]({{ source_file("c++/ppwebgui_http.cc") }}) and [`c++/ppwebgui_service.cc`]({{ source_file("c++/ppwebgui_service.cc") }}).

## Representation fidelity

| Representation | Fidelity and role |
| -------------- | ----------------- |
| PulsePins text | canonical user-facing authoring/interchange for the normalized subset described on this page; preserves represented element semantics and the force-trigger Boolean, but not arbitrary raw fields |
| `.ppbin` | current-build binary snapshot of the sequence element triplets and force-trigger flag; its header is versioned, but loading requires matching control, count, value, and trigger widths, so it is not the portable canonical authoring/interchange format |
| VCD | flattened regular-record projection; export evaluates records classified as regular from an initial value of zero, then removes zero-count runs, merges adjacent equal states, and rejects non-regular elements; import produces BITLOAD runs rather than the original operators, strobes, control flow, final policy, or force metadata |
| readback | normalized runs from `qout_valid`-qualified samples, represented as BITLOAD states and run counts; it does not recover authored operators, control flow, no-strobe states, or elapsed time while validity is low |
| Timeline CSV | editable named-channel pulse-table authoring data (`channel,bit,start,duration,color`) that compiles to PulsePins text |
| Timeline draft JSON | editable Timeline authoring state for channels, units, colors, and pulses that compiles to PulsePins text |
| Timeline SVG | visual preview only; it is not sequence input or an execution interchange format |

VCD export is not an execution-semantic serializer. It applies a stored regular record as an immediate output operation and applies a zero-count operation before dropping its run, whereas the hardware preprocessor can store a record without emitting it and the decoder ignores zero-count output operations. It also evaluates relative operations from initial value zero rather than the runtime initial `qout`. Normalize input to emitted, positive-count BITLOAD records, or evaluate relative operations against the actual initial state first; otherwise stored records, zero-count records, or a nonzero runtime initial value can produce a VCD that differs from hardware playback.

The binary reader/writer and VCD projection are implemented in [`c++/sequence.hh`]({{ source_file("c++/sequence.hh") }}). Readback behavior is described in [Readback](readback.md), and Timeline CSV, JSON, SVG, and text generation are described in the [Python API](python.md) and [Web interface](ppwebgui.md).

## Related documentation

* [Sequencer model](sequencer_model.md)
* [Sequence playback](ppplay.md)
* [C++ application programming interface](cpp.md)
* [Python API](python.md)
* [SCPI server](ppscpi.md)
* [Web interface](ppwebgui.md)
* [Readback](readback.md)
