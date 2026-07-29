# PulsePins Pulse Sequencer

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.17903233.svg)](https://doi.org/10.5281/zenodo.17903233)
[![CI](https://github.com/rokzitko/PulsePins/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/rokzitko/PulsePins/actions/workflows/ci.yml)

PulsePins is an open-hardware digital pulse sequencer designed for laboratories that need many precisely timed digital channels without the cost and opacity of full quantum-control racks. Functionally it occupies the same space as commercial pulse programmers and multi-channel digital delay generators: it compiles high-level timing programs into long, deterministic sequences for tens of 3.3 V LVTTL outputs in the released DE10-Nano design, at clock rates up to 100 MHz. Unlike traditional instruments, however, PulsePins is built on a commodity SoC FPGA board with fully open RTL and software, native Linux integration, and a workflow that fits naturally into version control and scripting environments. Large-scale quantum-control platforms and high-end AWGs remain the tools of choice for analog and microwave envelope generation, but PulsePins offers a complementary, digital-only alternative: compact, affordable, and hackable, ideal both as the backbone of smaller experiments and as an expendable “digital glue” resource in larger setups.

## Start here

Choose the path that matches what you want to do first:

| Goal | Best starting document |
| ---- | ---------------- |
| Try PulsePins on real hardware | `INSTALL-quick_start.md` and `docs/docs/getting_started_hardware.md` |
| Find the right CLI/API interface for a task | `docs/docs/choose_tool.md` |
| See concrete end-to-end workflows | `docs/docs/examples.md` |
| Extend the project with a new tool, binding, or feature | `docs/docs/extension_cookbook.md` |

## Common tasks

Useful first commands:

```bash
make dev-check
make board-smoke
```

Use `make dev-check` for the consolidated local checks.
Use `make board-smoke` for a quick manual live-board regression using the current locally built FPGA image and executables.
For heavier board validation, use `run_all_tests` on the target board.

Typical tools by task:

* generate a quick signal: `ppfg`, `ppdelay`
* replay a saved sequence: `ppplay`
* capture and inspect output: `ppread`
* validate clocks or PPS timing: `ppfreq`, `ppts`
* troubleshoot trigger routing: `pptrig`
* run built-in board checks: `pptest`

See `docs/docs/choose_tool.md` for the full chooser.

## Mode of operation

PulsePins is a flexible run-length-encoded (RLE) pattern generator for parallel data buses. The released DE10-Nano build uses 32-bit data and count fields, has a 10 ns programmable time step at the default 100 MHz streamer clock, and supports multistage triggering. It is designed for reliable operation with extensive self-testing.

## Main features

* High-speed RLE decoding core: one decoded update per clock while the decoder is active and not backpressured. At 100 MHz this provides a 10 ns programmable time step for pulse durations and separations.
* Two data sources: streaming from the hard processor system (ARM core) through a FIFO queue, or from predefined sequences in memory (buffer size up to 512 MB, streamed from RAM via DMA).
* Output FIFO with throttling and underrun detection for robust hardware-paced streaming when the FIFO remains fed.
* Preprocessor implementing a second level of run-length decoding (repetitions of short sequences of RLE elements), enabling compact representation of periodic signals.
* Internal clock (PLL-generated) or external clock input.
* Rich set of data-path update operations: load, set, clear, toggle, shift left/right, NOT, AND, OR, XOR, XNOR.
* Pseudorandom bitstream generator based on the xoroshiro128+ algorithm.
* Explicit per-element control over the `qout_valid` sample qualifier and corresponding `qout_strobe` pulse, configurable on the fly while streaming a sequence.
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
* Buffer-underrun detection, plus readback circuitry with an on-chip run-length encoder for comparing generated output with expected sequences.
* Simple built-in logic-analyzer capability: the readback path can capture deterministic digital waveforms and export them as VCD files for inspection in standard waveform viewers.
* Three complementary sequence interchange formats: editable PulsePins text files, waveform-oriented VCD files, and exact self-describing binary captures for lossless replay.
* Comprehensive hardware self-tests via the readback interface and a suite of test cases for systematic, intensive validation of correct device operation; most of the functionality is covered by these tests.
* 8-bit auxiliary input/output lines for general-purpose use.
* Time-stamping circuit for synchronization and timing purposes.
* General-purpose operation as a delay generator or function generator.
* Clean, well-documented SystemVerilog/Verilog RTL with testbenches and high test coverage.
* KiCad schematics and layouts for interface cards that provide easy interfacing (Pmod, SMA), two buffered PP_PMOD SMA outputs for Q0 and Q1, ESD protection, status LEDs, an external clock input, a trigger input with threshold control, and pads for CMOS crystal oscillators.
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
* Simple logic-analyzer-style capture and VCD export of digital patterns for debugging, verification, and regression checking.
* Exact capture/replay workflows using the PulsePins binary sequence format.

## Documentation

[Project webpage](http://auger.ijs.si/pulsepins/site/)

Useful contributor documents:

* `HACKING.md`
* `CONTRIBUTING.md`
* `docs/docs/build.md`
* `docs/docs/hacking.md`

## Recipes

Reusable command examples for the PulsePins command-line tools are stored in `recipes/`.
These files are separate from `tests/` so test executables and example command invocations are easier to find.

## License

This project is released under the MIT License. You may use, copy, modify, merge, publish, distribute, sublicense,
and/or sell the software and FPGA firmware (including RTL sources and generated bitstreams), including in commercial
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
