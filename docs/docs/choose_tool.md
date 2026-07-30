# Choose the right tool

PulsePins exposes the same hardware through task-specific commands, C++ and Python APIs, a SCPI-style network service, and a browser interface. Start with the narrowest interface that already matches the job.

This page describes the current source-tree command set. Older released images may provide fewer commands; check the selected release notes when a tool is unavailable.

## Choose by task

| Goal | Start with | Why |
| ---- | ---------- | --- |
| Set up and validate a released board | [Quick start](quick_start.md) | linear image, access, self-test, and first-output procedure |
| Generate a periodic signal, PWM, or finite burst | [`ppfg`](ppfg.md) | direct frequency, period, duty-cycle, and burst controls |
| Emit a pulse after a trigger and delay | [`ppdelay`](ppdelay.md) | one-shot delay-generator workflow |
| Run the smallest live output smoke test | [`pphelloworld`](pphelloworld.md) | immediate repeating output without a sequence file |
| Play a saved sequence | [`ppplay`](ppplay.md) | canonical text plus derived binary and VCD input |
| Capture, inspect, or replay qualified output samples | [`ppread`](ppread.md) and [Readback](readback.md) | `qout_valid`-qualified runs as text, VCD, or a current-build binary snapshot |
| Run built-in hardware checks | [`pptest`](pptest.md) and [Testing procedures](testing.md) | streamer, trigger, preprocessor, and readback validation |
| Measure clocks | [`ppfreq`](ppfreq.md) | external, internal, streamer, and core clock measurements |
| Inspect PPS or timestamp events | [`ppts`](ppts.md) | timestamp stream and interval reporting |
| Exercise event counters | [`ppcounter`](ppcounter.md) | built-in deterministic or pseudorandom counter test sequences |
| Monitor AUX pin levels | [`ppaux`](ppaux.md) | formatted sampling of the read-only CLI path |
| Read the PP_PMOD temperature sensor | [`pptemp`](pptemp.md) | MCP9808 board-peripheral access |
| Debug trigger sources and masks | [`pptrig`](pptrig.md) | trigger-combiner configuration and live state |
| Inspect or override output routing | [`ppqout`](ppqout.md) | output-combiner modes, masks, and force values |
| Reset an interrupted or infinite stream | [`ppreset`](ppreset.md) | return the primary streamer to a known idle state |
| Calculate a PLL profile without hardware | [`pllcalc`](pllcalc.md) | standalone clock-profile calculation |
| Automate from a script or notebook | [Python API](python.md) | generated sequences and experiment integration |
| Add a command or direct hardware wrapper | [C++ API](cpp.md) | native access to the existing runtime and hardware classes |
| Control a board over the network | [`ppscpi`](ppscpi.md) | line-oriented remote instrument control |
| Operate interactively from a browser | [`ppwebgui`](ppwebgui.md) | live state plus trigger, routing, and sequence controls |

## Choose an interface layer

| Interface | Best fit | Main tradeoff |
| --------- | -------- | ------------- |
| Command-line tools | immediate shell use and existing single-purpose operations | limited composition beyond shell scripts |
| Python API | notebooks, sweeps, generated sequences, and larger measurement scripts | board-native bindings and workstation SCPI clients have different deployment models |
| C++ API | new tools, direct wrappers, and performance-sensitive board-side work | requires native build and lower-level project knowledge |
| SCPI server | remote orchestration and instrument-style clients | unauthenticated control; restrict the bind address or use a trusted network |
| Web interface | interactive setup, status, and manual operation | intended for trusted networks rather than unattended automation |

Use the command line first when a dedicated tool already provides the operation. Move to Python or C++ when sequence generation, repetition, error handling, or integration logic starts dominating the shell commands.

## Sequence file formats

| Format | Use it when |
| ------ | ----------- |
| PulsePins text | the sequence should remain readable and editable |
| VCD | a flattened waveform should be inspected in a waveform-oriented tool and the documented projection limits are acceptable |
| PulsePins binary (`.ppbin`) | a normalized current-build sequence snapshot is useful and matching field widths are available |

PulsePins text is the canonical user-facing format. See [PulsePins text sequence format](sequence_format.md), [`ppplay`](ppplay.md), and [`ppread`](ppread.md) for fidelity and interface-specific behavior.

## Common paths

First board:

1. Follow [Quick start](quick_start.md).
2. Confirm `run_all_tests` reports `SUCCESS`.
3. Follow [First finite output](manual/first_output.md).
4. Select the next command or interface from the task table above.

Repeatable capture and replay:

1. Start [`ppread`](ppread.md) before the signal source and keep the capture running.
2. Generate with [`ppfg`](ppfg.md), [`ppdelay`](ppdelay.md), or a custom sequence while capture is active.
3. Inspect VCD or text output.
4. Replay with [`ppplay`](ppplay.md).

Readback records live qualified samples; it cannot recover a finite waveform that completed before `ppread` started, no-strobe states, or the elapsed time in invalid gaps. See [Capture and replay](manual/capture_replay.md) for the two-terminal workflow.

Peripheral transaction:

1. Validate the base output path with [`pptest`](pptest.md).
2. Check routing with [`ppqout`](ppqout.md) if needed.
3. Generate the transaction with the C++, Python, or helper-tool layer.
4. Play and verify the resulting sequence.

## Experimental reference

[`ppgpsdo`](ppgpsdo.md) remains an experimental GPSDO reference implementation rather than a recommended user-manual workflow. Establish and verify the complete oscillator-to-timestamp feedback path before treating it as a control loop for laboratory use.

For complete command-and-observation chapters, continue with the [User manual](examples.md).
