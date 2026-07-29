# PulsePins manual

PulsePins is an open-hardware FPGA pulse sequencer for laboratory timing and digital control. The released [DE10-Nano](https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=167&No=1046) design provides 32 digital outputs, 3.3 V LVTTL signaling, and a 10 ns programmable time step at the default 100 MHz streamer clock.

Project repository: [https://github.com/rokzitko/PulsePins](https://github.com/rokzitko/PulsePins).

## Start by goal

| Goal | Start with |
| ---- | ---------- |
| Set up a released image and validate a board | [Quick start](quick_start.md) |
| Resolve network, UART, or optional-hardware setup | [Hardware setup](getting_started_hardware.md) |
| Generate, capture, or measure a signal | [Choose the right tool](choose_tool.md) |
| Follow a complete laboratory workflow | [Worked examples](examples.md) |
| Automate an experiment | [Python API](python.md), [C++ API](cpp.md), or [SCPI server](ppscpi.md) |
| Understand the data, trigger, and timing model | [Sequencer model](sequencer_model.md) |
| Find board pins and electrical behavior | [DE10-Nano signal reference](de10_nano_reference.md) |
| Build, test, or extend the project | [Contributor overview](hacking.md) and [Build and deployment](build.md) |

## Manual sections

| Section | What it contains |
| ------- | ---------------- |
| Get started | first-board setup, tool selection, and worked examples |
| Command line | generation, playback, capture, measurement, and diagnostic commands |
| Interfaces | C++, Python, SCPI, and browser control |
| Hardware | DE10-Nano signals and optional shield designs |
| Architecture | sequence semantics, streamer internals, clocking, routing, and measurement IP |
| Develop and test | builds, validation, extension guidance, and contribution workflows |

The [sequencer model](sequencer_model.md) explains how compact run-length-encoded elements become triggered output updates. Detailed subsystem pages then document the corresponding RTL, registers, software interfaces, timing behavior, and diagnostics.

![PulsePins logo](img/pulsepins.jpg){: .heartbeat style="height:100px;width:100px"}
{ .blink-img }
