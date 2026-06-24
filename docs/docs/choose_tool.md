## Choose the right tool

PulsePins exposes the same hardware through several different surfaces:

* small command-line tools (`ppfg`, `ppdelay`, `ppplay`, `ppread`, ...)
* the C++ API
* Python bindings
* the SCPI server (`ppscpi`)
* the browser UI (`ppwebgui`)

This page is a task-oriented guide for choosing the right entry point.

### Start here

If you want to...

* generate a quick digital signal without writing a sequence file:
    use [`ppfg`](ppfg.md) or [`ppdelay`](ppdelay.md)
* run or replay a saved sequence file:
    use [`ppplay`](ppplay.md)
* capture what PulsePins produced and save it for inspection or replay:
    use [`ppread`](ppread.md)
* verify board health or run built-in streaming tests:
    use [`pptest`](pptest.md)
* inspect external clock or PPS timing:
    use [`ppfreq`](ppfreq.md) or [`ppts`](ppts.md)
* troubleshoot trigger routing and trigger sources:
    use [`pptrig`](pptrig.md)
* inspect or change combiner routing manually:
    use [`ppqout`](ppqout.md)
* drive a workflow from scripts or notebooks:
    use the [Python API](python.md)
* build custom sequence-generation logic or hardware wrappers:
    use the [C++ API](cpp.md)
* control the device remotely from automation software:
    use [`ppscpi`](ppscpi.md)
* interact with the device from a browser:
    use [`ppwebgui`](ppwebgui.md)

### By task

#### I want to generate a simple digital output

Use:

* [`ppfg`](ppfg.md) for periodic patterns, PWM-style outputs, bursts, and gated square-wave generation
* [`ppdelay`](ppdelay.md) for one-shot delayed pulses after a trigger
* [`pphelloworld`](pphelloworld.md) for the most minimal output-toggle smoke test

These are the best first tools when you want a signal quickly and do not need a saved sequence artifact.

#### I want to play back a sequence file

Use:

* [`ppplay`](ppplay.md)

Choose the file format by goal:

* text (`.seq`, `.txt`) when you want human-editable sequences
* binary (`.ppbin`) when you want exact replay of the full internal representation
* VCD when the source waveform came from a waveform-oriented tool

#### I want to capture and inspect what happened

Use:

* [`ppread`](ppread.md)

Typical outputs:

* VCD for waveform viewers
* text for manual inspection/editing
* binary for exact replay/regression workflows

If you want a simple digital-logic-analyzer workflow, `ppread` is usually the right first stop.

#### I want to validate clocks, PPS, or timing sources

Use:

* [`ppfreq`](ppfreq.md) for external/internal/streamer clock measurements
* [`ppts`](ppts.md) for PPS and timestamp stream inspection

These tools are useful during board bring-up, synchronization work, and external-reference troubleshooting.

#### I want to debug triggers and routing

Use:

* [`pptrig`](pptrig.md) to inspect trigger source selection, masks, and inversion
* [`ppqout`](ppqout.md) to inspect or force combiner/output routing behavior

These are lower-level troubleshooting tools rather than general sequence-generation commands.

#### I want to verify the device itself

Use:

* [`pptest`](pptest.md) for built-in streaming, trigger, preprocessor, and readback verification
* [`ppcounter`](ppcounter.md) for the integrated counter subsystem

If you just brought up a board or changed low-level behavior, start here before building bigger workflows.

#### I want to read board peripherals

Use:

* [`pptemp`](pptemp.md) for the onboard temperature sensor path
* [`ppaux`](ppaux.md) for AUX input sampling

For PP_PMOD-specific hardware context, also see [`pp_pmod.md`](pp_pmod.md) and [`pp_pmod_reference.md`](pp_pmod_reference.md).

### Choose the right interface layer

#### CLI tools

Best when:

* you want immediate interaction from a shell
* you want copy-pasteable lab commands
* you want the fastest path to trying a built-in feature

Start with the CLI if the job already maps to an existing tool page.

#### Python API

Best when:

* you want scripting, notebooks, or quick automation
* you want to generate or transform sequences programmatically without building a C++ binary
* you want to integrate PulsePins into a larger measurement script

See: [Python API](python.md).

#### C++ API

Best when:

* you are extending the project itself
* you need new tool behavior, new wrappers, or tighter control over host-side execution paths
* performance and direct integration with the existing C++ runtime matter

See: [C++ API](cpp.md).

#### SCPI server

Best when:

* you want remote instrument-style control from other software
* your lab already has SCPI-oriented orchestration

Use: [`ppscpi`](ppscpi.md).

#### Web GUI

Best when:

* you want a quick browser-based control surface
* you want to inspect live status and adjust trigger/combiner settings interactively
* you want to stream PulsePins text sequences without writing a custom client

Use: [`ppwebgui`](ppwebgui.md).

### Typical user journeys

#### First hardware bring-up

Suggested order:

1. [`pphelloworld`](pphelloworld.md)
2. [`pptest`](pptest.md)
3. [`ppfreq`](ppfreq.md) or [`ppts`](ppts.md) if timing inputs matter
4. [`ppfg`](ppfg.md) or [`ppdelay`](ppdelay.md) for your first real output

#### Build a repeatable waveform workflow

Suggested order:

1. prototype with [`ppfg`](ppfg.md) or [`pptest`](pptest.md)
2. capture with [`ppread`](ppread.md)
3. replay with [`ppplay`](ppplay.md)
4. move to Python or C++ if you need generated sequences

#### Bring up a peripheral device from qout pins

Suggested order:

1. validate base outputs with [`pptest`](pptest.md)
2. validate routing with [`ppqout`](ppqout.md) if needed
3. prototype the transaction with helper tools under [`tools/`]({{ source_file("tools/") }})
4. replay the generated sequence with [`ppplay`](ppplay.md) or `pptest -f`

### When to move beyond the CLI

Move from CLI tools to Python or C++ when:

* you are repeating the same command patterns in scripts
* you need programmatic sequence generation
* you want richer error handling than shell pipelines provide
* you are adding a new capability instead of just using an existing one

### Related pages

* [Worked examples](examples.md)
* [C++ API](cpp.md)
* [Python API](python.md)
* [Build and deployment](build.md)
* [Hacking on PulsePins](hacking.md)
