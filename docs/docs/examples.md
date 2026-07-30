# User manual

This manual is organized around finite, reproducible laboratory tasks. It describes current `main`, host/FPGA ABI 6, and the released 32-bit DE10-Nano design. Older binary images may not provide every command or option used here; use documentation and software from the same revision.

The chapters are source-reviewed procedures, not claims of bench validation. Record the actual hardware, clock, software revision, commands, and observations whenever you run one.

## Validation status

| Status | Meaning |
| ------ | ------- |
| source-reviewed | commands and stated behavior were checked against current source and reference documentation |
| board-tested | the complete procedure was run on a matching DE10-Nano build |
| bench-validated | wiring and expected physical behavior were checked with the stated instruments |
| experimental | the workflow is incomplete, setup-dependent, or lacks enough evidence for a supported lab procedure |

Only `source-reviewed` status is asserted by these pages unless a chapter includes a dated validation record.

## Core chapters

| Chapter | Outcome | Additional hardware | Status |
| ------- | ------- | ------------------- | ------ |
| [First finite output](manual/first_output.md) | generate ten periods on only `qout[0]` and return the bus to zero | high-impedance scope or logic analyzer | source-reviewed |
| [First text sequence](manual/text_sequence.md) | author and play the canonical PulsePins text format | optional high-impedance probe | source-reviewed |
| [Triggered one-shot delay](manual/triggered_delay.md) | use KEY0 to release one delayed pulse on `qout[0]` | high-impedance scope or logic analyzer | source-reviewed |
| [Capture and replay](manual/capture_replay.md) | record a live finite waveform and replay the canonical text capture | two board terminals; optional waveform viewer | source-reviewed |
| [Workstation Python timeline](manual/python_timeline.md) | generate, inspect, and stream a finite Timeline through restricted SCPI | workstation and trusted network | source-reviewed |

Read the [PulsePins text sequence format](sequence_format.md) before hand-authoring control-flow, trigger, replay, or relative-update records.

## Before a chapter

1. Complete [Quick start](quick_start.md) or deploy a matching current-source build.
2. Confirm that the host software and FPGA image report the expected ABI.
3. Disconnect incompatible external drivers and loads.
4. Use 3.3 V-compatible, high-impedance inputs and a common ground.
5. Record the active streamer clock instead of assuming 100 MHz.
6. Keep the chapter's explicit output masks and final value.

The manual uses finite operations by default. Command-reference pages may also describe continuous or low-level modes; those modes require their own interrupt, reset, output-state, and safe-rewiring procedure.

## Reference-only workflows

The following capabilities remain documented, but they are not yet presented as supported manual chapters:

| Area | Current reference | Why it is not a core chapter |
| ---- | ----------------- | ---------------------------- |
| external clock measurement | [`ppfreq`](ppfreq.md) and [Frequency meter](freq_meter.md) | source voltage, nominal frequency, termination, selected channel, and negative control must be specified for a reproducible setup |
| external PPS timestamping | [`ppts`](ppts.md) and [Timestamp capture](timestamp.md) | a chapter must use explicit `-pps_in`, finite sample counts, and a disconnected-input negative control |
| PP_PMOD temperature | [`pptemp`](pptemp.md) and [PP_PMOD hardware reference](pp_pmod_reference.md) | shield revision, populated sensor, bus visibility, finite sample count, and expected address should be recorded |
| SPI/DDS generation | [C++ SPI sequence generation](cpp.md#spi-sequence-generation) | generator path, output-bit wiring, decoder clock, peripheral reference clock, and measured result require setup-specific validation |
| GPSDO controller | [`ppgpsdo`](ppgpsdo.md) | experimental reference implementation; the complete oscillator feedback path must be independently established and verified |

These entries should become chapters only after their setup, finite commands, expected result, cleanup, and positive and negative controls have been recorded.

## Choose another interface

Use [Choose the right tool](choose_tool.md) when the task does not match a chapter. Command pages provide option-level reference, while [C++](cpp.md), [Python](python.md), [SCPI](ppscpi.md), and the [web interface](ppwebgui.md) describe programmatic control layers.
