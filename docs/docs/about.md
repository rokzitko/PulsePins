# PulsePins Pulse Sequencer

## Project summary

PulsePins is a [run-length-encoded (RLE)](index.md#run-length-encoding) pattern generator for
parallel data buses. The released DE10-Nano build uses 32-bit data and count fields, has a 10 ns programmable time step at the default 100 MHz streamer clock, and supports multistage triggering. It is designed for reliable operation with self-testing.

## Main features

* High-speed RLE decoding core: one decoded update per clock while the decoder is active and not backpressured. At 100 MHz this provides a 10 ns programmable time step for pulse durations and separations.
* Two data sources: streaming from the hard processor system (ARM core) through a FIFO queue, or from predefined sequences in memory (buffer size up to 512 MB, streamed from RAM via DMA).
* Output FIFO with throttling and underrun detection for robust hardware-paced streaming when the FIFO remains fed.
* Preprocessor implementing a second level of run-length decoding (repetitions of short sequences of RLE elements), enabling compact representation of periodic signals.
* Internal clock (PLL-generated) or external clock input.
* Rich set of data-path update operations: load, set, clear, toggle, shift left/right, NOT, AND, OR, XOR, XNOR.
* Pseudorandom bitstream generator based on the xoroshiro128+ algorithm.
* Explicit per-element control over the `qout_valid` sample qualifier and corresponding `qout_strobe` pulse, configurable on the fly while streaming a sequence.
* Multi-bit, multi-stage triggering with long trigger programs (bounded only by the configurable trigger-stage buffer, 256 stages by default). Each stage is defined by a mask (which bits are observed) and a pattern (expected bit values). The 8 trigger inputs can be extended to a wider trigger bus.
* Switchable trigger sources (external inputs, internal signals, on-board push-buttons/switches) with per-bit masking and inversion.
* Multiple streamer cores (four instances by default) with independent triggering for conditional streaming controlled by external signals. Outputs from the cores are combined by an advanced multiplexer that supports:

     - selection between streamers,
     - logic operations (AND, OR, XOR, XNOR, majority),
     - concatenation of data blocks from different cores,
     - arithmetic sums and differences,
     - per-bit masking and inversion.

* Pausing and retriggering to support conditional branching and looping.
* Gating: output streaming can be halted by a gate signal.
* High-level object-oriented C++ API.
* Python bindings for the C++ API (nanobind), with unit tests based on pytest.
* Buffer-underrun detection, plus readback circuitry with an on-chip run-length encoder for comparing generated output with expected sequences.
* Hardware self-tests via the readback interface and a suite of test cases for validation of correct device operation; most of the functionality is covered by these tests.
* 8-bit auxiliary input/output lines for general-purpose use.
* Time-stamping circuit for synchronization and timing purposes. With PPS from a GNSS receiver and an external frequency-tunable oscillator, PulsePins can implement a GPS-disciplined oscillator (GPSDO).
* General-purpose operation as a delay generator or function generator.
* Well-documented SystemVerilog/Verilog RTL with testbenches.
* KiCad schematics and layouts for interface cards (Pmod, SMA) that provide buffered outputs, ESD protection, status LEDs, a trigger SMA input with threshold control, external clock and PPS inputs, and optional CMOS oscillator modules.
* A 20-day continuous soak test at a 100 MHz streamer clock, without an FPGA heatsink, completed with no observed lockups or errors.
* Configurable design widths for the output data bus and run-length counter; the released DE10-Nano build uses 32-bit data and count registers.
* Reference and user manuals (these web pages).
* Liberal MIT license, requiring only attribution.

## Typical use cases

* Control of complex scientific apparatus under strict timing constraints (where a 10 ns programmable time step at 100 MHz is sufficient).
* Driving serializer circuits for generating high-frequency signals.
* Driving digital-to-analog converters (DACs) for generating analog waveforms.
* Driving direct digital synthesis (DDS) chips for RF/microwave signal generation.
* Delay generation and synchronization.
* Characterization of logic devices and circuits.
* Protocol emulation (I²C, SPI, and other serial buses).
* Generation of periodic signals (repetitive bit patterns) or pseudorandom sequences for communications testing.
* Burn-in and stress testing.

## Acknowledgments

This project incorporates code from the project [rsyocto](https://github.com/robseb/rsyocto) (c) Robin Sebastian, licensed under the MIT License.
See `third_party/rsyocto/LICENSE` for details.

[Rok Zitko](http://auger.ijs.si/nano), [rok.zitko@ijs.si](mailto:rok.zitko+pulsepins@ijs.si), 2025
