# PulsePins pulse sequencer

This manual covers the released [DE10-Nano](https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=167&No=1046) design: 32 digital outputs, 3.3 V LVTTL signaling, and a 10 ns programmable time step at the default 100 MHz streamer clock. The board runs the PulsePins control software on its HPS and can be operated locally, over Ethernet, or from Python and C++.

Project repository: [https://github.com/rokzitko/PulsePins](https://github.com/rokzitko/PulsePins).

## First run

1. Follow the [`INSTALL-quick_start.md`]({{ source_file("INSTALL-quick_start.md") }}) instructions to prepare and access the board.
2. Run the [baseline board checks](testing.md) and confirm that `run_all_tests` reports `SUCCESS`.
3. Follow a [worked output example](examples.md) and verify the signal with a scope or logic analyzer.

For source work that does not require a board, see [Development without hardware](getting_started_no_hardware.md).

## Choose by task

| Goal | Start with |
| ---- | ---------- |
| Bring up or test a board | [Hardware setup](getting_started_hardware.md) and [testing procedures](testing.md) |
| Generate a periodic or delayed output | [`ppfg`](ppfg.md), [`ppdelay`](ppdelay.md), and [worked examples](examples.md) |
| Play a saved sequence | [`ppplay`](ppplay.md) |
| Capture, inspect, or replay output | [`ppread`](ppread.md) and [readback](readback.md) |
| Measure clocks or PPS timing | [`ppfreq`](ppfreq.md) and [`ppts`](ppts.md) |
| Control a board from Python or a notebook | [Python interface](python.md) and [`ppscpi`](ppscpi.md) |
| Troubleshoot triggers or output routing | [`pptrig`](pptrig.md) and [`ppqout`](ppqout.md) |
| Add a command, binding, or hardware feature | [Extension cookbook](extension_cookbook.md) and [build guide](build.md) |

For a fuller comparison of the available interfaces, see [Choose the right tool](choose_tool.md).

## Run-length encoding

[Run-length encoding](https://en.wikipedia.org/wiki/Run-length_encoding) (RLE) stores a stable output value as a value and a repetition count instead of recording one sample per clock. In PulsePins, a regular element with count `N` holds its output value for `N` streamer-clock cycles. The same element stream also carries trigger conditions, final values, and preprocessor instructions such as store and replay.

The FPGA decodes these elements into output updates and can send them to GPIO pins or other FPGA data paths. Trigger and gate logic control when output advancement is allowed, while the readback path records the emitted stream for comparison or capture.

![PulsePins diagram](img/PulsePins.001.png){: style="height:300px"}

HPS = hard processor system (ARM cores), ST = [Avalon-ST](https://www.intel.com/content/www/us/en/docs/programmable/683091/22-3/avalon-streaming-interfaces.html) streaming interface, MM = [Avalon-MM](https://www.intel.com/content/www/us/en/docs/programmable/683091/22-3/avalon-memory-mapped-interfaces.html) memory-mapped interface, PIO = programmable input/output.

## General concept

Sequence _elements_ representing updates of the output data signals or control information (e.g. trigger settings) are
fed from the HPS through an Avalon-ST bus via an input FIFO buffer to a RLE decoding core. The core
transmits the decoded signals to an output FIFO buffer, from which it is read out using a reading clock and provided
on the output pins. Reading out starts upon triggering. The input buffer has a preprocessor that can perform
high-level manipulations on the sequence (e.g. storing short segments in memory and replaying stored segments, i.e.,
second-level run-length decoding).

## Elements

The structure of each _element_ is as follows:

* ``control_t y``: control parameter
* ``count_t c``: counter payload
* ``value_t v``: value payload (output data, trigger pattern, etc.)

This list defines the standard order (as transmitted via Avalon-ST) and the standard variable names (``y``, ``c``, ``v``) of
the three constituents. In the released DE10-Nano build, the types ``control_t``, ``count_t`` and ``value_t`` are
32-bit unsigned integers, i.e., ``uint32_t``; both the RTL and the software library are written in such a way that expansion
(to e.g. 64-bit values) or narrowing (to e.g. 16-bit values) is possible with a matching build configuration. The control register contains
information about the exact meaning of the information contained in the two payload items: data updates ("regular
elements", also known as "symbols"), trigger patterns and masks ("trigger elements", "trigger conditions", or simply
"triggers"), sequence termination ("final elements", also known as "terminators"), preprocessor instructions (store,
replay).

### Composition of the control parameter

Defined in [`ip/streamer/config.vh`]({{ source_file("ip/streamer/config.vh") }}) (bit fields, 0 is LSB):

| Name               | Bit        | Description |
| -------           | ----------- | ----------- |
| BIT_TRIGGER       | 0           | regular element (0) or trigger element (1)                    |
| BIT_TRIGGER_FINAL | 1           | intermediate trigger element (0) or final trigger element (1) |
| BIT_TERMINATE     | 2           | data sequence terminator                                      |
| BIT_NO_STROBE     | 3           | `qout_valid` sample qualifier and strobe pulse (0) or neither (1) |
| BIT_MODE*         | 4-7         | mode bits (load, set, clear, flip, invert, shift, etc.)       |
| BIT_NOPASS        | 8           | preprocessor bit (0 = pass unmodified, 1 = preprocess)        |
| BIT_STORE         | 9           | store in preprocessor memory                                  |
| BIT_POSITIONS*    | 10-13       | storage position                                              |
| BIT_REPLAY        | 15          | replay a sequence stored in the preprocessor                  |
| BIT_RETRIG        | 16          | retrigger request                                             |
| BIT_PRNG          | 17          | emit random numbers                                           |


### Element types

Regular elements represent data updates. The data can assert the sample qualifier (BIT_NO_STROBE low) or not (BIT_NO_STROBE high). In both cases
the data will be clocked out on the output bus using the streamer clock, but in the first case `streamer_qout_valid` qualifies the sample and
`streamer_qout_strobe` emits the corresponding pulse (see below about the exact timing of the strobe pulse with respect to the streamer
clock). For flexibility, regular elements can either specify the new value on the output bus, or encode a change (bit
set, bit clear, bit flip, etc.). This is controlled by the "mode bits" in the control parameter.

All elements describing trigger sequences have the control bit 0 (BIT_TRIGGER) high. The final trigger element additionally has its
control bit 1 (BIT_TRIGGER_FINAL) high (preceding non-final elements have their bit 1 low); this special marking of the final trigger
element is often not needed, because the trigger will also fire when all trigger conditions in the condition queue are
exhausted. In simple cases, there will be a single trigger element in the sequence. The final trigger element
might be needed in cases where streaming is paused through a _retrigger request_. In this case, the trigger queue
needs to contain trigger condition subsequences separated by "final trigger" elements, because retriggering advances
from the completed trigger subsequence to the next queued subsequence.

The trigger can also be forced by an internal signal (generated by the HPS, i.e., using a software library call) or an
external signal (digital input pin on the FPGA); in this case trigger elements in the sequence are not necessary.

The last (final) element in the sequence terminates the decoding, hence it is also known as the terminator. It has the
control bit 2 (BIT_TERMINATE) high. The value payload of the final element is decoded in the standard way (depending on the setting of
mode bits) and presented on the output as the persistent final state. The decoding of the terminator element also
marks the successful completion of a streaming run for the buffer-underrun detection circuit.

### Replay preprocessor

PulsePins has a "preprocessor" in the input pipeline.  This implements a second level of
run-length decoding (i.e., repetitions of the same sequence of run-length-encoded events).

The preprocessor can store up to 8 elements (the size can be expanded).  A "replay" consists of
repeatedly emitting these elements back into the queue.  If the number of repetition is set to 0, the
elements are replayed indefinitely; this is a simple way for generating periodic signals (see the
[ppfg](ppfg.md) function generator tool).

## Multistreamer

Four instances of the RLE decoder are available. They run independently with separate triggering.
The results are combined to produce the final output signals. The combiner-multiplexer allows
bit-resolved inversion and masking at all inputs and outputs. There are multiple modes of operation:

* SEL1: select streamer 1
* SEL2: select streamer 2
* SEL3: select streamer 3
* SEL4: select streamer 4
* AND, OR, XOR, XNOR: bitwise logical operation for each bit taking inputs from all four streamers
* MAJ: majority, i.e., three out of four operation
* BLOCK8: takes 8 least-significant bits from each streamer to generate the four bytes of the output
* BLOCK16: takes 16 least-significant bits from streamer 1 and 2 to generate the output
* SUM12, SUM1234: algebraic sum of data from streamer 1 and 2, or 1, 2, 3, and 4
* DIFF12: algebraic difference of data from streamer 1 and 2

This design allows conditional streaming of different sequences based on the trigger conditions.

## DMA streaming

Streaming can proceed via direct memory access (DMA) up to 512 MB in size without any intervention of the HPS, freeing
the processor for other tasks. Except for the difference in the data channel and speed, streaming from DMA
and through FIFO buffers is equivalent. For sequences with quickly changing signals at
high streamer_clk frequencies, it may happen that a FIFO buffer underflow occurs. Such errors are detected and indicated
by the buffer_error LED lighting up. These are the situations where the DMA method should be used.

## Output enable

By default, the physical output drivers are in the high-Z state, so the pins act as inputs (see
[readback](readback.md) about using the device as a simple logic analyzer with run-length encoding).
To drive the pins, the physical output enable (``oe``) must be set high.

## Clocks and clock domains

PulsePins uses several important clocks. At the top level, the most important ones are the 50 MHz reference clock
(`ref_clk`), the main internal system clock (`core_clk`), the internal candidate streaming clock (`int_clk`), and the
actually selected streaming/output clock (`streamer_clk`).

By default, `core_clk` and `int_clk` are both configured for 100 MHz operation. When `streamer_clk` is 100 MHz, the
design uses a 10 ns programmable time step for digital level updates. There is no fixed upper limit on pulse duration; it
is limited only by the counter width and the selected clock.

The active `streamer_clk` can be switched between the internal clock path and an externally connected clock. The
external clock is a 3.3 V LVTTL signal applied to the `EXT_CLKp` input pin.

The most important boundary between the main control side and the output side is the dual-clock output FIFO in
[`ip/streamer/output_fifo.sv`]({{ source_file("ip/streamer/output_fifo.sv") }}).

For a fuller description of clock relationships, software clock switching, and timing constraints, see
`clock_domain.md`.

## Signal routing

### Pinout on the DE10-Nano

The output data is presented on the GPIO 0 and GPIO 1 headers of the Terasic DE10-Nano board.

![PulsePins pinout](img/PulsePins.002.png){: style="height:400px"}

Color code in the schematic:

| Color | Meaning |
| ----- | ------- |
| <font color="orange">orange</font>  | clocking |
| <font color="green">green</font>    | status |
| <font color="darkblue">blue</font>  | triggering |
| <font color="#FFD580">yellow</font> | aux I/O |
| <font color="cyan">cyan</font>      | external clock inputs |
| <font color="red">red</font>        | output data ports |

In the reference implementation for the DE10-Nano development board, the signals are present on the following GPIO
pins (defined in [`pulsepins.sv`]({{ source_file("pulsepins.sv") }})). GPIO0[25:22] reflects the currently selected
`EXTRA_SETB` debug mux; the alternate `EXTRA_SETA` build exposes streamer trigger visibility on those pins instead:

| Connector | Index | Debug port | Name        | Description |
| --------- | ----- | ----       | ----------- | -------- |
| GPIO0     | 0     | D0         | <font color="orange">streamer_qout_strobe</font>     | Data strobe pulse |
|           | 1     | D1         | <font color="orange">oe</font>                  | Physical output enable |
|           | 2     | D2         | <font color="orange">streamer_clk</font>        | Streamer clock |
|           | 3     | D3         | <font color="orange">streamer_qout_valid</font> | Sample qualifier for data output (qout) |
|           | 4     |            | <font color="orange">activity</font>            | Activity detected (high when data is being streamed out) |
|           | 5     |            | <font color="orange">heartbeat</font>           | Pulses when FPGA bitstream is loaded |
|           | 6     | D8         | <font color="green">trigger_armed</font>        | PulsePins is waiting for the trigger event to occur |
|           | 7     | D9         | <font color="green">trigger_activated</font>    | Triggered and data is being streamed out |
|           | 8     | D10        | <font color="green">done</font>                 | Streaming out has completed without any underflow errors |
|           | 9     | D11        | <font color="green">buffer_error</font>         | Buffer underflow error detected |
|           | 10    |            | <font color="darkblue">ext_trigger_enable</font>    | Trigger enable (make PulsePins sensitive to trigger signals) |
|           | 11    |            | <font color="darkblue">ext_trigger_force</font>     | External trigger force (unconditional) |
|           | 12    |            | <font color="darkblue">ext_trigger_reset</font>     | Reset the trigger circuit |
|           | 13    |            | <font color="darkblue">gate_in</font>               | Output-advancement gate input |
|           | 21:14 |            | <font color="darkblue">ext_trigger_in[7:0]</font>   | Trigger inputs |
|           | 22    | D12        | <font color="lightskyblue">rnd1</font>                            | Synthetic random debug signal (`EXTRA_SETB`) |
|           | 23    | D13        | <font color="lightskyblue">rnd2</font>                            | Synthetic random debug signal (`EXTRA_SETB`) |
|           | 24    | D14        | <font color="lightskyblue">0</font>                               | Constant low debug output (`EXTRA_SETB`) |
|           | 25    | D15        | <font color="lightskyblue">0</font>                               | Constant low debug output (`EXTRA_SETB`) |
|           | 26    |            | <font color="DimGrey">I2C SDA</font>            | I2C interface data |
|           | 27    |            | <font color="DimGrey">I2C SCL</font>            | I2C interface clock |
|           | 35:28 |            | <font color="#FFD580">AUX</font>            | Auxiliary I/O |
| GPIO1     | 0     |            | <font color="Cyan">EXT_CLKp</font>    | External clock input |
|           | 1     |            | <font color="Cyan">PPS_IN</font>      | Pulse-per-second input (for synchronization and triggering) |
|           | 2     |            | <font color="Cyan">PPCLK1</font>      | Reserved external crystal clock 1 |
|           | 3     |            | <font color="Cyan">PPCLK2</font>      | Reserved external crystal clock 2 |
|           | 35:4  | D[7:4] for qout[3:0]     | <font color="red">streamer_qout</font> | Data output, qout[31:0] |

Note that the table contains the "index" within the GPIO arrays, not the pin numbers on headers. The signals marked by
(out) and with the description "as seen by the streamer core" are output signals for monitoring. See the section on
the _trigger combiner_ module about mixing external, internal and on-board switch/button triggering signals.

### Status LEDS

PulsePins provides streaming status signals for status LEDs. The following signals are provided
(suggested colors for LEDs on [ppboards](ppboards.md) are also indicated):

* pin 0 - trigger_armed (blue)
* pin 1 - trigger_activated (yellow)
* pin 2 - done (green)
* pin 3 - buffer_error (red)

The meaning of these signals is explained in the table detailing GPIO port connections. The same
signals are also wired to the on-board green LEDs of the DE10-Nano board.

The remaining on-board LEDs on DE10-Nano board are connected as follows by default:

* pin 4: streamer_trigger_in[0]
* pin 5: streamer_trigger_in[1]
* pin 6: activity
* pin 7: heartbeat

Activity LED lights up if at least one low-to-high transition is detected within 200 ms on the
streamer_qout_strobe signal. Heartbeat pulses each second if the FPGA bitstream is loaded and
the clock is running (at the default rate of 100 MHz).

## Trigger system

PulsePins provides 8 trigger inputs. Trigger conditions are defined by a pattern and a mask. The
mask defines which trigger inputs are tested, while the pattern defines the target values.

The trigger conditions can be chained. The trigger-program buffer has 256 positions for trigger conditions.
The trigger is activated when all trigger conditions in the chain have been consecutively fulfilled and a trigger condition element
marked as final has been encountered or the trigger buffer becomes empty.

Trigger is sensitive only when the _enable_ signal is high. The enable signal can be generated internally (via a
software call), externally (signal applied to a pin) or using switches (switch 0 set to ON); see
the section on [trigger combiner](#trigger-combiner) on details about the different trigger sources.

Trigger being _armed_ means that the data is available in the output FIFO and that the PulsePins core is waiting for
the trigger events to occur.

Trigger can be forced by an internal or external trigger_force signal, or using the physical switch number 2 on DE10-Nano board.

Trigger system can be _reset_ using the trigger_reset signal. The trigger reset deasserts the trigger activated signal.
It does not clear the trigger buffer or discard the currently armed trigger condition! Holding the trigger reset signal
asserted will prevent any triggering, even in case of a trigger-force signal. The trigger reset can thus be used as a
safety mechanism (e.g. as an interlock).

Trigger reset signal is also useful for situations where streaming needs to be stopped until the current trigger
condition occurs again. For sequence-controlled advancement to a different trigger subsequence, use a retrigger element.

### Trigger combiner

Trigger combiner is a hardware circuit that accepts trigger inputs and control signals from
multiple sources. The source ports are named

* _internal_ (INT): software defined using a PIO interface,
* _external_ (EXT): connected through GPIO pins to connectors on a [ppboard](ppboards.md),
* _miscellaneous_ (MISC): pushbuttons and switches; detailed in the following.

The combiner circuits allows bit inversion on inputs and outputs, bit masking on inputs and
outputs, overrides on inputs and outputs, multiplexing or logic operations to combine the signals,
readback of all signals. All settings are under software control.

The multiplexer modes are the following:

* SEL1: select port 1 (INT)
* SEL2: select port 2 (EXT)
* SEL3: select port 3 (MISC)
* SEL4: select port 4 (not used)
* AND, OR, XOR, XNOR: bitwise logical operation for each bit taking inputs from all four ports

### Manual triggering using the physical buttons

Trigger inputs 0 and 1 are wired to KEY0 and KEY1 pushbuttons of the DE10-Nano board. In the
[trigger combiner](#trigger-combiner) circuit these two trigger signals are wired to the MISC trigger port.

### Manual trigger control

Trigger control signals are connected to the following switches of the DE10-Nano board:

* switch 0: trigger enable
* switch 1: trigger force
* switch 2: trigger reset

(Note: Switches are in OFF position when they are closer to the Arduino connector.)
In the [trigger combiner](#trigger-combiner) circuit these trigger controls are wired to the MISC
trigger port.

### Triggering on pulse-per-second signal (PPS)

PPS_IN signal is connected to pin 2 of the MISC port.


## Timing

Streaming is synchronous with the read clock which must be running continuously.

### Clocking (sample qualifier and strobe pulse)

Example of a successful decoding run of a short sequence (counter from 0 to 7). After the trigger is activated,
`streamer_qout_valid` is asserted at the next rising edge of `streamer_clk` to qualify the output sample. The data can be read out in two ways:

* at the rising edges of `streamer_clk`, when `streamer_qout_valid` qualifies the sample
* at the rising edges of the `streamer_qout_strobe` pulses

The `streamer_qout_strobe` pulse is asserted in the middle of the period (i.e., when `streamer_clk` is deasserted,
thus out of phase with the clock).

The first approach (using the sample qualifier) is potentially more reliable at high frequencies,
because the signal is guaranteed to be settled at the rising edges of `streamer_clk`; there is no
guarantee for this to be the case at the rising edges of `streamer_qout_strobe` (but in practice the
signals are stabilized by then at all frequencies of practical interest).

The second approach (using the strobe pulse) is potentially more reliable in slow digital logic
systems which may have issues with high slew rates, i.e., those that require long hold times after
the rising edge of the clock signal in order for flip-flips to function reliably.

[ ![Timing](img/seq1.png){: style="width:600px;height:600px"} ](img/seq1.png)


## Trigger monitoring

Trigger signals are connected to an input PIO (pio_trig_monitor) for monitoring purposes. Lower half of
the bits correspond to the external signals:

| Pin   | Name |
| ---   | ---- |
| 7:0   | ext_trig_in |
| 8     | ext_trig_enable |
| 9     | ext_trig_force |
| 10    | ext_trig_reset |

Upper half of the bits correspond to the signals as seen by the streamer cores (i.e., after being
processed in the trigger combiner):

| Pin   | Name |
| ---   | ---- |
| 23:16 | streamer_trig_in |
| 24    | streamer_trig_enable |
| 25    | streamer_trig_force |
| 26    | streamer_trig_reset |

Use [pptrig](pptrig.md) for debugging the triggering subsystem.

## Auxiliary I/O (AUX)

The released design exposes eight bidirectional AUX pins. Direction is selected independently for each bit through `pio_cfg`: `0` selects input, `1` selects output, and all bits default to input after reset. `pio_aux` holds output values and reports the sampled pin levels.

[`ppaux`](ppaux.md) samples the bus; it does not configure pin direction or drive output values.

## Extensions

PulsePins has been successfully extended to 64 output channels and to a 64-bit size of the count variable. Currently,
only the 32-bit version (32-bit for both data and count registers) is distributed as a prebuilt binary.

PulsePins is portable to other Altera/Intel FPGA solutions and it has been tested, for example, on Arria 10
FPGAs for driving 10 Gb/s transceivers, specifically on [Terasic HAN
Pilot](https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&No=1133), corresponding to a 100 ps unit interval.

![PulsePins logo](img/pulsepins.jpg){: .heartbeat style="height:100px;width:100px"}
{ .blink-img }
