# C++ application programming interface

The C++ code in [`c++/`]({{ source_file("c++/") }}) serves two closely related roles:

* it is the host-side control plane for the PulsePins hardware
* it is also the implementation substrate for the `pptool` command family

The architecture is intentionally layered so command-line tools, tests, and bindings share the same sequence and hardware-control model.

### Host-side architecture

The main entry point is [`c++/pptool.cc`]({{ source_file("c++/pptool.cc") }}).

At startup it:

1. builds a shared `HostRuntime` from [`host_runtime.hh`]({{ source_file("c++/host_runtime.hh") }})
2. that runtime parses common options, enables the shared runtime policy, constructs the single `FPGA` object, and performs the startup frequency-meter report
3. dispatches to a command handler based on the executable name

That dispatch model is why one compiled binary can appear as multiple tools such as `pptool`, `ppfg`, `ppcounter`, `ppdelay`, and `ppplay`.

The most important host-side layers are:

* [`fpga.hh`]({{ source_file("c++/fpga.hh") }}) - top-level ownership of memory maps, PLL helpers, trigger monitors, and GPIO-backed control paths
* [`startup.hh`]({{ source_file("c++/startup.hh") }}) - common process bootstrap and default FPGA startup policy
* [`host_runtime.hh`]({{ source_file("c++/host_runtime.hh") }}) - shared bootstrap/runtime object used by the main host-side executables
* [`options.hh`]({{ source_file("c++/options.hh") }}), [`pll_rules.hh`]({{ source_file("c++/pll_rules.hh") }}), [`pll_calc.hh`]({{ source_file("c++/pll_calc.hh") }}) - typed option resolution, symbolic PLL presets, and calculated PLL profiles shared by multiple tools
* [`pptool_streaming.cc`]({{ source_file("c++/pptool_streaming.cc") }}), [`pptool_measurement.cc`]({{ source_file("c++/pptool_measurement.cc") }}) - user-facing command implementations
* [`ppworkflow.hh`]({{ source_file("c++/ppworkflow.hh") }}) - shared streaming workflow used by commands that send sequences, arm/force triggers, and optionally validate readback
* [`elements.hh`]({{ source_file("c++/elements.hh") }}), [`sequence.hh`]({{ source_file("c++/sequence.hh") }}) - host-side representation of pulse programs and trigger programs
* subsystem wrappers such as [`streamer.hh`]({{ source_file("c++/streamer.hh") }}), [`readback.hh`]({{ source_file("c++/readback.hh") }}), [`counter.hh`]({{ source_file("c++/counter.hh") }}), [`timestamp.hh`]({{ source_file("c++/timestamp.hh") }}), [`freq_meter.hh`]({{ source_file("c++/freq_meter.hh") }}), and the trigger/combiner helpers in [`trigger.hh`]({{ source_file("c++/trigger.hh") }}), [`trigger_int.hh`]({{ source_file("c++/trigger_int.hh") }}), and [`trigger_ext.hh`]({{ source_file("c++/trigger_ext.hh") }})

For a maintainer-oriented walkthrough of the directory, see [`c++/README.md`]({{ source_file("c++/README.md") }}).

### Common execution flow

For streamer-oriented tools, the typical host-side path is:

1. build or parse a `Sequence`
2. construct a transport such as a FIFO-backed streamer or DMA-backed streamer
3. transmit the sequence to the FPGA
4. enable or force the trigger
5. optionally validate the readback stream
6. wait for completion and inspect final status, counters, FIFO statistics, and [CRC32 integrity checks](readback.md#crc32-integrity-checks)

That common pattern is centralized in `send_and_trig(...)` in [`ppworkflow.hh`]({{ source_file("c++/ppworkflow.hh") }}) so that behavior stays consistent across multiple tools.

The startup path follows the same philosophy: `HostRuntime` centralizes executable bootstrap, CLI and environment inputs are normalized by [`options.hh`]({{ source_file("c++/options.hh") }}), then [`startup.hh`]({{ source_file("c++/startup.hh") }}) applies the resulting clock-selection and PLL policy through the `FPGA` wrapper.

Trigger configuration follows a similar split:

* [`options.hh`]({{ source_file("c++/options.hh") }}) resolves trigger-related CLI switches into a `TriggerOptions` policy object
* [`trigger.hh`]({{ source_file("c++/trigger.hh") }}) applies that policy to the trigger combiner
* [`trigger_int.hh`]({{ source_file("c++/trigger_int.hh") }}) and [`trigger_ext.hh`]({{ source_file("c++/trigger_ext.hh") }}) expose the direct software-driven and observed trigger PIO paths

### Data types for sequence representation

The host-side sequence model mirrors the encoded data consumed by the streamer core. The core types are defined in [`elements.hh`]({{ source_file("c++/elements.hh") }}) and [`sequence.hh`]({{ source_file("c++/sequence.hh") }}).

Besides the text sequence format used by several tools, `Sequence` also supports [Value Change Dump (VCD)](https://en.wikipedia.org/wiki/Value_change_dump) import and deterministic waveform export back to VCD. The export path targets sequences that can be reduced to a regular effective output waveform; control-flow and random elements are intentionally rejected. The C++ VCD export default is `$timescale 10ns`, matching the VCD import default of a 10 ns PulsePins output period.

For exact, lossless interchange, `Sequence` also supports a self-describing binary format that preserves the full internal sequence representation, including control-flow elements and the force-trigger flag.

In practice this also makes PulsePins useful as a simple logic-analyzer backend for deterministic readback waveforms.

Serialization capability matrix:

| Format | C++ | Python | CLI |
| ------ | --- | ------ | --- |
| PulsePins text sequence format | import + export | import + export | export via `ppread -save-text`; import in selected workflows |
| Value Change Dump (VCD) | import + export | import + export | import via `ppplay` or `ppvcd`; export via `ppread -save-vcd` |
| PulsePins binary sequence format | import + export | import + export | import via `ppplay`, export via `ppread -save-binary` |

Class hierarchy for counter and value helper objects:

* ``Counter`` (defined in [`elements.hh`]({{ source_file("c++/elements.hh") }})): lightweight wrappers that carry the repetition count and the strobe policy for regular elements

    * ``Strobe``: regular elements that assert the valid/strobe output
    * ``NoStrobe``: regular elements that update qout without asserting the valid/strobe output

Interface of ``Counter`` objects:

* ``count()``: returns the count value
* ``count_str()``: the count value as a string in hexadecimal and decimal form for inspection and troubleshooting
* ``control_bits()``: returns the strobe-related bits to be or'd into the control word
* ``desc()``: descriptive string for the strobe policy

Update types (``d`` is the input value, ``q_prev`` is the previous output value, ``q`` is the new streamed value):

* ``LOAD``: ``q = d``
* ``SET``: ``q = q_prev | d``
* ``CLEAR``: ``q = q_prev & ~d``
* ``FLIP``: ``q = q_prev ^ d``
* ``NOT``: ``q = ~q_prev``
* ``AND``: ``q = q_prev & d``
* ``OR``: ``q = q_prev | d``
* ``XOR``: ``q = q_prev ^ d``
* ``XNOR``: ``q = ~(q_prev ^ d)``
* ``SLL``: ``q = q_prev << d``
* ``SRL``: ``q = q_prev >> d``

The corresponding ``Value`` helper hierarchy is exposed in [`elements.hh`]({{ source_file("c++/elements.hh") }}):

* ``Value``
* ``BitLoad``
* ``BitSet``
* ``BitClear``
* ``BitFlip``
* ``BitNot``
* ``BitAnd``
* ``BitOr``
* ``BitXor``
* ``BitXnor``
* ``BitSll``
* ``BitSrl``
* ``TriggerCondition``

These helper types are lightweight tags and inspectors for authored elements. The ``el`` object
itself stores flattened raw ``control``, ``count``, and ``value`` fields.

Interface of ``Value`` objects:

* ``value()``: returns the stored value
* ``value_str()``: value in hexadecimal and decimal form for inspection and troubleshooting
* ``result(value_t v_prev)``: returns the updated value for a given previous output state
* ``mode_bits()``: returns the update-mode bits to be or'd into the control word
* ``desc()``: descriptive string for the update mode

``TriggerCondition`` remains a specialized ``Value`` subclass with a constructor that takes ``pattern``, ``mask``, and ``final``. Its string formatting decomposes the packed trigger value back into pattern and mask fields.

Class ``el`` (defined in [`elements.hh`]({{ source_file("c++/elements.hh") }})) represents one encoded sequence element. Supported construction paths are:

* ``el()``: sequence terminator with the default final output value
* ``el(value_t)``: sequence terminator with an explicit final output value
* ``el(count_t, value_t)``: regular element, ``BitLoad`` update
* ``el(Counter, value_t)``: regular element with explicit strobe policy, ``BitLoad`` update
* ``el(Counter, Value)``: regular element with explicit strobe and update mode
* ``el(trigger_t, trigger_t, bool)``: trigger condition element
* ``el(Replay, count_t, value_t)``: replay a stored subsequence; the length must not exceed the fast-memory depth (``POSITIONS``)
* ``el(Retrig, value_t)``: stop streaming and wait for a retrigger event
* ``el(PseudoRandom, count_t)``: emit pseudo-random values for the selected count

Important implementation detail: ``el`` derives the effective element kind from the encoded control word. It keeps small authored metadata only where the raw wire format is ambiguous for regular elements (for example, to preserve descriptive strings around plain ``Value`` vs authored ``BitLoad`` construction).

Useful ``el`` methods:

* ``control()``, ``count()``, ``value()``: raw integer representation sent to the streamer/RLE decoder
* ``kind()``: returns the derived ``el_type``
* ``mode()``: returns the regular-mode bits from the control word
* ``no_strobe()``, ``is_stored()``, ``store_slot()``: inspect regular-element control metadata
* ``trigger_pattern()``, ``trigger_mask()``, ``trigger_is_final()``: inspect trigger elements
* ``updated_value(value_t)``: compute the effective streamed output for a previous output state
* ``desc()``: detailed debug description
* ``decode()``: includes extra control decorations such as the ``store`` slot
* ``regular_token()`` and ``sequence_record()``: render the element into the PulsePins text sequence grammar

Preferred transformation helpers on ``el`` are immutable:

* ``stored_in(int)``
* ``with_control(control_t)``
* ``with_count(count_t)``
* ``with_counter(Counter)``
* ``with_regular_value(Value)``
* ``as_bitload_after(value_t)``

Additional mutating helpers are available for direct in-place updates: ``set_control()``, ``set_count()``, ``set_value()``, and ``store()``. Prefer the immutable helpers above when constructing transformed elements.

[`elements.hh`]({{ source_file("c++/elements.hh") }}) also centralizes reconstruction helpers that are used by binary and text I/O:

* ``el::classify_control(...)``
* ``el::from_raw_triplet(...)``
* ``el::from_regular_token(...)``
* ``el::is_regular_token(...)``

``el`` objects can be compared and sent to an ``std::ostream``.

Class ``Sequence`` (defined in [`sequence.hh`]({{ source_file("c++/sequence.hh") }})) represents a sequence of elements. Internally, this is an overloaded `std::deque` of ``el`` objects.
Public member functions are:

* ``size()``: total number of elements
* ``data_size()``: number of regular elements
* ``length()``: total duration of the sequence in units of clock periods (length)
* ``dump()``: print out the entire sequence
* ``convert_to_BitLoad()``: return a sequence where all regular (data) elements are converted to BITLOAD updates
* ``merge()``: return a sequence where neighbouring data elements with the same value are merged together

Two sequences can be compared using function ``compare()`` and using ``operator==``.

[`sequence.hh`]({{ source_file("c++/sequence.hh") }}) also provides ``parse_sequence_from_stream(std::istream&)`` for the text-based sequence format used by `pptest` test 42 and by the SCPI `SEQ` command. That parser accepts the same regular update modes implemented by the `Value` subclasses, non-final triggers, preprocessor operations (`store`, `r`, `rt`, `pr`), terminal explicit final terminators, and the `f` force-trigger flag. The accepted token grammar is documented inline next to the parser and mirrored in [pptest - self-tests](pptest.md) ([source]({{ source_file("docs/docs/pptest.md") }})).

The same header also provides ``write_sequence_to_stream(...)`` and ``write_sequence_to_file(...)`` for emitting that text format from an in-memory `Sequence`. These helpers are intended for round-tripping sequences through files or for generating sequence files programmatically instead of hand-writing token streams.

### SPI sequence generation

[`SPI.hh`]({{ source_file("c++/SPI.hh") }}) provides a host-side [SPI](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface) sequence generator that emits PulsePins `Sequence` objects.

PulsePins generates SPI transactions by turning clock, data, and chip-select transitions into ordinary output-sequence elements.

Generic SPI support lives in the `spi` namespace:

* ``spi::Config``: decoder clock, requested SPI clock, SPI mode, bit order, pin mapping, chip-select polarity, and select/deselect timing
* ``spi::SequenceBuilder``: stateful encoder that emits RLE `Sequence` elements for SPI idle, select, clock, MOSI, and auxiliary-pin changes

The builder quantizes the requested SPI clock to the nearest half-period measured in decoder-clock ticks. Use ``half_period_ticks()`` and ``achieved_spi_clock_hz()`` to inspect the resulting timing.

Typical usage:

```cpp
spi::Config cfg;
cfg.decoder_clock_hz = 100e6;
cfg.spi_clock_hz = 12e6;

spi::SequenceBuilder spi_builder(cfg);
spi_builder.write_transaction({0x9a, 0xbc});

const Sequence &seq = spi_builder.sequence();
write_sequence_to_file(seq, "spi_sequence.txt");
```

Device-specific helpers stay separate from the generic SPI layer. [`PMODDA3.hh`]({{ source_file("c++/PMODDA3.hh") }}) provides [PMOD DA3](https://digilent.com/shop/pmod-da3-one-16-bit-d-a-output/) support in the `pmod_da3` namespace:

* ``pmod_da3::default_spi_config()``: PMOD pin mapping and timing defaults matching the existing PMOD DA3 example
* ``pmod_da3::code_from_voltage()``: convert output voltage to the 16-bit DAC code
* ``pmod_da3::transaction_for_code()`` and ``pmod_da3::transaction_for_voltage()``: build the SPI transaction as a `spi::SequenceBuilder`

Example:

```cpp
auto dac = pmod_da3::transaction_for_voltage(1.25);
std::cout << dac.achieved_spi_clock_hz() << std::endl;
write_sequence_to_stream(dac.sequence(), std::cout, true);
```

### Streamer control interface

The C++ streamer wrappers are thin, typed views of the hardware control/status registers rather than a separate software simulation of the datapath.

This is important for maintainability: the intent is that the code stays close to the actual FPGA programming model.

Class ``streamer_control`` (defined in [`streamer_control.hh`]({{ source_file("c++/streamer_control.hh") }}) and re-exported by [`streamer.hh`]({{ source_file("c++/streamer.hh") }})) is the high-level interface for controlling the RLE-decoder core. Member functions:

* ``status()``: read the status register (buffer error, done, triggered, armed); these are outputs from the
core
* ``get_control()``: returns the software-side control shadow (stop, internal trigger force, internal trigger enable,
  reset, internal trigger reset, qout select, stop-on-buffer-error)
* ``get_qout()``: synchronized snapshot of the current value at the output (on the "device pins", if there is no postprocessing)
* ``get_qout_streamer()``: synchronized snapshot of the streamer output before the optional override mux
* ``qout_select()``: select whether the streamer output or override value is presented; the active streamer-clock
  shadow updates only while the streamer is idle or reset
* ``qout_set()``: write the override value and select it; active output visibility follows the same idle/reset
  commit rule and readback snapshot latency
* ``monitor_ext_trig()``: debugging tool for external trigger signals
* ``set_initial_value()``: set the value that is present at the streamer data output before triggering
* ``buffer_error()``: buffer error detected
* ``done()``: sequence decoding completed successfully
* ``status_report()``: dump the status in textual representation
* ``reset_streamer()``: reset the streamer and erase data in all FIFO buffers
* ``reset()``: reset the streamer core (set control register to zero and calls ``reset_streamer``)
* ``trigger_enable()``: enable the trigger circuit; the trigger signals are ignored until this member function is
called (or using an external trigger enable signal)
* ``trigger_force()``: assert the internal trigger force signal
* ``trigger_reset()``: assert the internal trigger reset signal
* ``gating()``: control gating (control of the streaming using an external output-enable signal)

Class ``streamer_fifo`` (defined in [`streamer_fifo.hh`]({{ source_file("c++/streamer_fifo.hh") }}) and re-exported by [`streamer.hh`]({{ source_file("c++/streamer.hh") }})) is the high-level interface for spooling the sequence to the RLE-decoder core. Member functions:

* ``out()``: send one element
* ``send_sequence()``: send entire sequence
* ``check_fill_status()``: check FIFO status

Class ``streamer_dma`` (defined in [`streamer_dma.hh`]({{ source_file("c++/streamer_dma.hh") }}) and re-exported by [`streamer.hh`]({{ source_file("c++/streamer.hh") }})) is the high-level interface for transmitting the sequence to the
RLE-decoder code using [direct memory access (DMA)](https://en.wikipedia.org/wiki/Direct_memory_access). Member functions:

* ``write_element()``: write an element at given memory location
* ``prepare()``: write an entire sequence in the memory buffer
* ``verify()``: verify the correctness of the sequence stored in memory
* ``transfer()``: perform the transfer
* ``send_sequence()``: high-level function for transmitting a sequence to the streamer via DMA

In practical terms, ``streamer_fifo`` is the simpler CPU-driven path for short sequences, while ``streamer_dma`` prepares a memory-backed transfer and lets the DMA engine move the sequence into the hardware path.

### System-level interfaces

`FPGA` (defined in [`fpga.hh`]({{ source_file("c++/fpga.hh") }})) is the top-level ARM-side container for memory-mapped FPGA access.

It owns the main memory maps, clock-selection helpers, trigger monitor helpers, PLL control helpers, and the output-enable GPIO path.
The constructor enforces single-owner semantics for this top-level hardware view. Shared startup actions such as clock selection, PLL setup, and the bring-up LED blink are applied by [`startup.hh`]({{ source_file("c++/startup.hh") }}), not by the constructor itself.

Clocking-related responsibilities are split deliberately:

* `FPGA::set_clk()` performs top-level streamer-clock source switching with the required reset hold/release sequence
* `pll_core_clk` and `pll_int_clk` in [`pll_clk.hh`]({{ source_file("c++/pll_clk.hh") }}) program the two reconfigurable PLLs
* `FPGA::set_streamer_clk()` stores the measured active streaming frequency for later timing-aware host operations
* `FPGA::streamer_done_config(active_mask, armed_live_mask)` configures the top-level aggregate `streamer_done` masks used by the four-stream output/status path

Streamer classes (defined in [`basic_multi_dma.hh`]({{ source_file("c++/basic_multi_dma.hh") }})):

* ``basic_streamer``: one streamer instance with its control interface and FIFO transport; the minimal building block for higher-level streamer helpers
* ``streamer``: single-core helper using the FIFO transport and the Avalon-ST mux default path; it also applies the usual bring-up sequence (initial value, output enable, reset)
* ``dma_streamer``: single-core helper that reuses the same streamer bring-up but switches the transport to DMA via the ST mux
* ``multistreamer``: container for four independent ``basic_streamer`` instances with coordinated bring-up of the four streamer cores

The single-stream helpers configure aggregate done as `active_mask=0b0001` and `armed_live_mask=0b0001`. `multistreamer` defaults both masks to `0b1111`; commands may override them with `-streamer_active_mask` and `-streamer_armed_live_mask`. For mutually exclusive trigger programs, set `-streamer_armed_live_mask 0` so streamers that remain armed but never activate do not block aggregate completion.

The host-side transport split is:

* [`streamer_control.hh`]({{ source_file("c++/streamer_control.hh") }}) - register-level lifecycle control, status, gating, CRC readout, and FIFO statistics
* [`streamer_fifo.hh`]({{ source_file("c++/streamer_fifo.hh") }}) - direct CPU-driven FIFO transport for short/simple sequence delivery
* [`streamer_dma.hh`]({{ source_file("c++/streamer_dma.hh") }}) - SDRAM + [Intel/Altera Modular Scatter-Gather DMA](https://www.intel.com/content/www/us/en/docs/programmable/683130/21-4/modular-scatter-gather-dma-core.html) transport for longer or repeated transfers

`combiner` (defined in [`combiner.hh`]({{ source_file("c++/combiner.hh") }})) is the interface for advanced multiplexers.

`combiner_qout`, `qout` (defined in [`qout.hh`]({{ source_file("c++/qout.hh") }}))

`readback` (defined in [`readback.hh`]({{ source_file("c++/readback.hh") }}))

`st_mux` (defined in [`st_mux.hh`]({{ source_file("c++/st_mux.hh") }}))

`trigger` (defined in [`trigger.hh`]({{ source_file("c++/trigger.hh") }}))

counter (defined in [`counter.hh`]({{ source_file("c++/counter.hh") }})) exposes the integrated statistics/measurement subsystem.

timestamp (defined in [`timestamp.hh`]({{ source_file("c++/timestamp.hh") }})) exposes the dual-path timestamp FIFOs and source-selection controls.

freq_meter, pp_freq_meter (defined in [`freq_meter.hh`]({{ source_file("c++/freq_meter.hh") }})) expose the on-board frequency meter and its higher-level PulsePins wrapper.

### Customization points

Common extension paths are:

* add a new `pp...` command in [`pptool.cc`]({{ source_file("c++/pptool.cc") }}) and declare it in [`pptool_commands.hh`]({{ source_file("c++/pptool_commands.hh") }})
* extend the sequence model in [`elements.hh`]({{ source_file("c++/elements.hh") }}) / [`sequence.hh`]({{ source_file("c++/sequence.hh") }})
* add a new typed wrapper for a hardware block and expose it through a tool or higher-level API
* adjust shared streaming policy in [`ppworkflow.hh`]({{ source_file("c++/ppworkflow.hh") }})
* update startup defaults in [`startup.hh`]({{ source_file("c++/startup.hh") }})
* change clock/PLL option semantics in [`options.hh`]({{ source_file("c++/options.hh") }}) and [`pll_rules.hh`]({{ source_file("c++/pll_rules.hh") }})

The preferred approach is to preserve the top-level command names and high-level wrapper types while evolving the implementation behind them.

For subsystem-level details, see also:

* [Counter subsystem](counter.md)
* [Combiner subsystem](combiner.md)
* [Timestamp capture](timestamp.md)
* [Frequency meter](freq_meter.md)
* [Avalon-ST multiplexer](st_mux.md)
* [Build and deployment](build.md)
