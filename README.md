# PulsePins RLE-Decoder

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.17903233.svg)](https://doi.org/10.5281/zenodo.17903233)

PulsePins is an open-hardware digital pulse sequencer designed for laboratories that need many precisely timed digital channels without the cost and opacity of full quantum-control racks. Functionally it occupies the same space as commercial pulse programmers and multi-channel digital delay generators: it compiles high-level timing programs into long, deterministic sequences on tens of TTL/LVDS outputs at clock rates up to the 100 MHz range. Unlike traditional instruments, however, PulsePins is built on a commodity SoC FPGA board with fully open RTL and software, native Linux integration, and a workflow that fits naturally into version control and scripting environments. Large-scale quantum-control platforms and high-end AWGs remain the tools of choice for analog and microwave envelope generation, but PulsePins offers a complementary, digital-only alternative: compact, affordable, and hackable, ideal both as the backbone of smaller experiments and as an expendable “digital glue” resource in larger setups.

## Mode of operation

PulsePins is a flexible run-length–encoded (RLE) pattern generator for 32-bit (or wider) parallel data buses with 10&nbsp;ns timing resolution and advanced triggering capabilities. It is designed for reliable, robust operation with extensive self-testing. Common use cases are quick to implement, while the architecture remains flexible and easily extensible.

## Main features

* High-speed RLE decoding core: zero-wait-state decoding (one update per clock period), limited only by the clock frequency. At 100&nbsp;MHz this provides 10&nbsp;ns timing resolution for pulse durations and separations.
* Two data sources: streaming from the hard processor system (ARM core) through a FIFO queue, or from predefined sequences in memory (buffer size up to 512&nbsp;MB, streamed from RAM via DMA).
* Output FIFO with throttling to guarantee loss-free streaming.
* Preprocessor implementing a second level of run-length decoding (repetitions of short sequences of RLE elements), enabling compact representation of periodic signals.
* Internal clock (PLL-generated) or external clock input.
* Rich set of data-path update operations: load, set, clear, toggle, rotate left/right, NOT, AND, OR, XOR, XNOR.
* Pseudorandom bitstream generator based on the xoroshiro128+ algorithm.
* Explicit control over output strobing (presence/absence of strobe pulses), configurable on the fly while streaming a sequence.
* Advanced multi-bit, multi-stage triggering with arbitrarily long trigger programs (bounded only by the configurable trigger-stage buffer, 256 stages by default). Each stage is defined by a mask (which bits are observed) and a pattern (expected bit values). The 8 trigger inputs can be extended to a wider trigger bus.
* Switchable trigger sources (external inputs, internal signals, on-board push-buttons/switches) with per-bit masking and inversion.
* Multiple streamer cores (four instances by default) with independent triggering for conditional streaming controlled by external signals. Outputs from the cores are combined by an advanced multiplexer that supports:
    * selection between streamers,
    * logic operations (AND, OR, XOR, XNOR, majority),
    * concatenation of data blocks from different cores,
    * arithmetic sums and differences,
    * per-bit masking and inversion.
* Pausing and retriggering to support conditional branching and looping.
* Gating: output streaming can be halted by a gate signal.
* High-level, modern, object-oriented C++ API.
* Python bindings for the C++ API (nanobind), with unit tests based on pytest.
* Buffer-underrun detection and read-back circuitry with an on-chip run-length encoder for verification and high-assurance scenarios where reliability and correctness under all operating conditions are critical.
* Comprehensive hardware self-tests via the read-back interface and a suite of test cases for systematic, intensive validation of correct device operation; most of the functionality is covered by these tests.
* 8-bit auxiliary input/output lines for general-purpose use.
* Time-stamping circuit for synchronization and timing purposes.
* General-purpose operation as a delay generator or function generator.
* Clean, well-documented Verilog implementation with test benches and high test coverage.
* KiCad schematics and layouts for interface cards that provide easy interfacing (PMOD, SMA), buffering (50&nbsp;Ω drivers), ESD protection, status LEDs, external clock input with threshold control, and pads for CMOS crystal oscillators.
* Proven stability: no lockups or errors observed during five days of continuous stress testing at 100&nbsp;MHz streamer clock without a heatsink on the FPGA.
* Configurable widths for the output data bus (32 or 64 bits) and for the run-length counter (32 or 64 bits).
* Reference and user manuals (these web pages).
* Liberal MIT license, requiring only attribution.

## Typical use cases

* Control of complex scientific apparatus under strict timing constraints (where 10&nbsp;ns resolution is sufficient).
* Driving serializer circuits for generating high-frequency signals.
* Driving digital-to-analog converters (DACs) for generating analog waveforms.
* Driving direct digital synthesis (DDS) chips for RF/microwave signal generation.
* Delay generation and synchronization.
* Characterization of logic devices and circuits.
* Protocol emulation (I²C, SPI, and other serial buses).
* Generation of periodic signals (repetitive bit patterns) or pseudorandom sequences for communications testing.
* Burn-in and stress testing.

## Documentation

[Project webpage](http://auger.ijs.si/pulsepins/site/)

Useful contributor entry points:

* `HACKING.md`
* `CONTRIBUTING.md`
* `docs/docs/build.md`
* `docs/docs/hacking.md`

## Contributors welcome

PulsePins is explicitly intended to appeal to hackers, tinkerers, and experimenters.

Contributions are welcome across documentation, examples, recipes, C++ tools, Python bindings, RTL, test benches, hardware validation notes, and experimental integration ideas. Small, practical improvements are just as valuable as large features.

Worked examples are especially valuable. Example-driven documentation is one of the project goals.

If you want to get started quickly:

* without hardware, see `HACKING.md` and `docs/docs/getting_started_no_hardware.md`
* with hardware, see `INSTALL-quick_start.md` and `docs/docs/getting_started_hardware.md`
* for contribution process and DCO details, see `CONTRIBUTING.md`

Community-contributed examples would be especially appreciated in application areas such as lasers/shutters/detectors, synchronized instrument triggering, delay generation for time-resolved measurements, and DDS/DAC control workflows.

## Recipes

Reusable command examples for the PulsePins command-line tools are stored in `recipes/`.
These files are separate from `tests/` so test executables and example command invocations are easier to find.

## License

This project is released under the MIT License. You may use, copy, modify, merge, publish, distribute, sublicense,
and/or sell the software and FPGA firmware (including HDL sources and generated bitstreams), including in commercial
and closed-source products. The only conditions are that you preserve the original copyright and license notice in all
substantial portions of the software/firmware, and that the software and firmware are provided “as is”, without any
warranty or liability on the part of the authors.

If you use PulsePins in scientific work, please cite:

> R. Žitko, *PulsePins: Open-hardware digital pulse sequencer for time-resolved experiments*, Zenodo (2025), [doi:10.5281/zenodo.17903233](https://doi.org/10.5281/zenodo.17903233).

### BibTeX

```bibtex
@misc{pulsepins_1_0,
  author       = {Žitko, Rok},
  title        = {PulsePins: Open-hardware digital pulse sequencer for time-resolved experiments},
  year         = {2025},
  publisher    = {Zenodo},
  version      = {1.0},
  doi          = {10.5281/zenodo.17903233},
  url          = {https://doi.org/10.5281/zenodo.17903233}
}
```

## Acknowledgments

This project incorporates code from the project [rsyocto](https://github.com/robseb/rsyocto) (c) Robin Sebastian, licensed under the MIT License.
See `third_party/rsyocto/LICENSE` for details.


[Rok Zitko](http://auger.ijs.si/nano), [rok.zitko@ijs.si](mailto:rok.zitko+pulsepins@ijs.si), 2025
