## Examples and starter ideas

This page collects practical example ideas for PulsePins.

The highest-value examples for the project are not toy demos, but worked examples that show how PulsePins can fit into real experiments and measurement setups.

### Community example areas

The following application areas would especially benefit from community-contributed examples:

* pulse sequencing for lasers, shutters, and detectors
* synchronized triggering of cameras and instruments
* delay generation for time-resolved measurements
* DDS/DAC control for frequency or amplitude sweeps

### Practical starter examples

* generate a periodic square-wave pattern
* use PulsePins as a delay generator
* replay a waveform from a VCD file with `ppvcd`
* measure an external clock with `ppfreq`
* inspect PPS timing with `ppts`
* compose two streamer outputs with the combiner
* drive LED PMODs using `pptest`
* generate SPI transactions directly from C++ and stream them without an intermediate payload tool
* read the onboard `MCP9808` using `pptemp` or `I2C/mcp9808.py`
* read an external Qwiic `TMP117` module using `I2C/tmp117.py`

For board-specific hardware examples, see [PP_PMOD validated examples](pp_pmod_examples.md).

### Why examples matter

Examples are one of the easiest ways for new users and contributors to understand what the project can do.

Well-documented examples are also a good place to capture:

* required hardware setup
* exact commands
* expected outputs
* common mistakes

Examples can also capture:

* instrument choices
* wiring diagrams
* timing diagrams
* screenshots, scope traces, and logic-analyzer captures
* photos of the physical setup

### Status

This page is currently a roadmap for future example-driven documentation. Contributions of tested example workflows are very welcome.
