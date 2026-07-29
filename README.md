# PulsePins Pulse Sequencer

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.17903233.svg)](https://doi.org/10.5281/zenodo.17903233)
[![CI](https://github.com/rokzitko/PulsePins/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/rokzitko/PulsePins/actions/workflows/ci.yml)

PulsePins is an open-hardware FPGA pulse sequencer for laboratory timing and digital control. The released DE10-Nano design provides 32 digital outputs and a 10 ns programmable time step at a 100 MHz streamer clock. Use it for triggers, gates, digital buses, and DAC or DDS control; it does not generate analog or RF waveforms directly.

## Start here

Choose the path that matches what you want to do first:

| Goal | Start here |
| ---- | ---------- |
| Bring up a DE10-Nano | [Quick start](INSTALL-quick_start.md) and [hardware setup](docs/docs/getting_started_hardware.md) |
| Choose a command or API | [Tool chooser](docs/docs/choose_tool.md) |
| Follow a complete lab workflow | [Worked examples](docs/docs/examples.md) |
| Understand the sequencer | [Manual home](docs/docs/index.md) |
| Modify or extend PulsePins | [Hacking guide](HACKING.md) and [extension cookbook](docs/docs/extension_cookbook.md) |

## At a glance

| Item | Released DE10-Nano design |
| ---- | ------------------------- |
| Digital output | 32-bit bus, 3.3 V LVTTL |
| Timing | 10 ns programmable time step at a 100 MHz streamer clock |
| Sequence delivery | HPS-driven FIFO or memory-backed DMA transfers up to 512 MB |
| Control | Command-line tools, C++, Python, SCPI-style TCP, and a browser interface |
| Checks | FPGA readback encoder, CRC and FIFO diagnostics, and board self-tests |
| Design files | SystemVerilog/Verilog RTL, software, and KiCad sources under the MIT License |

## Documentation

[User and reference manual](http://auger.ijs.si/pulsepins/site/) ([source](docs/docs/index.md))

Useful contributor documents:

* [Hacking guide](HACKING.md)
* [Contribution guide](CONTRIBUTING.md)
* [Build and deployment](docs/docs/build.md)
* [Development guide](docs/docs/hacking.md)

## Recipes

Reusable command examples for the PulsePins command-line tools are stored in [`recipes/`](recipes/).
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
