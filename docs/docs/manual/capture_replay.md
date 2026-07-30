# Capture and replay

## Target profile/status

This chapter targets current `main`, host/FPGA ABI 6, and the released 32-bit DE10-Nano profile. Older release images may differ in available commands, options, startup output, export formats, or FPGA behavior; use documentation that matches the installed image when it is not a current-source build.

Status: this is a source-aligned procedure, not a claim of bench validation.

## Goal

Capture the `qout_valid`-qualified samples from a finite `ppfg` burst into text, VCD, and binary files, then replay the canonical text capture. The generated and replayed runs use `qout[0]` only and finish at `0x00000000`.

## Safety

The capture command uses `-oe 1`, and the generator also enables the physical output drivers. PulsePins uses 3.3 V LVTTL signaling. Leave the output bus disconnected unless it is connected to compatible high-impedance inputs, use a common ground for any instrument, and do not apply a direct 50 ohm termination to an unbuffered FPGA output. Never connect an active external driver to a PulsePins output during this procedure.

## What you need

* A DE10-Nano running a current ABI 6 PulsePins host build and matching FPGA image
* Two board terminals, such as two SSH sessions to the same board
* `ppread`, `ppfg`, `ppplay`, and `ppreset`
* Space for `capture.seq`, `capture.vcd`, and `capture.ppbin`
* Optionally, a waveform viewer and a high-impedance probe on `qout[0]`

## Wiring/setup table

| Item | Connection or process | Role |
| --- | --- | --- |
| Terminal 1 | Shell on the target board | Foreground `ppread` capture |
| Terminal 2 | Separate shell on the same board | Start finite `ppfg` only after capture is ready |
| Readback source | Internal output path selected with `-oe 1` | Capture the generated qout waveform |
| Optional signal probe | `qout[0]`: GPIO 1 (`JP7`) pin 5 to a high-impedance channel | External observation only |
| Optional negative control | `qout[1]`: GPIO 1 (`JP7`) pin 6 | Should remain low |
| Optional reference | GPIO 1 (`JP7`) pin 12 or pin 30 to instrument ground | Common ground |

## Procedure

1. Before starting either terminal workflow, reset to an explicit zero idle state:

   ```bash
   ppreset -i 0x0
   ```

2. In terminal 1, run `ppread` in the foreground. The one-second idle timeout ends capture after data stops, while the 30-second hard timeout bounds the entire attempt:

   ```bash
   ppread -veryverbose -oe 1 -timeout 1 -hard-timeout 30s \
     -save-text capture.seq -save-vcd capture.vcd \
     -save-binary capture.ppbin
   ```

3. Keep terminal 1 in the foreground and wait for its initial `Readback status:` line. Do not append `&`, do not insert a guessed `sleep`, and do not start the generator before this status appears.

4. In terminal 2, run the finite generator:

   ```bash
   ppfg -burst 10 -period 10ms -trig -v1 0x1 -v0 0x0 -t 0x0
   ```

5. Return to terminal 1. It should finish after the post-data idle timeout and report the three saved files. This generated sequence ends by dropping validity, which flushes its final active readback run. The hard timeout remains the finite fallback if no data arrives, but a hard timeout by itself does not flush an in-progress constant run.

6. Treat the text capture as the canonical artifact for this chapter and inspect it directly:

   ```bash
   cat capture.seq
   ```

7. Replay those qualified runs with forced triggering and an explicit final zero value:

   ```bash
   ppplay -force -file capture.seq -format text -t 0x0
   ```

8. The other captures can also be replayed as finite waveforms when needed:

   ```bash
   ppplay -force -file capture.vcd -format vcd -t 0x0
   ppplay -force -file capture.ppbin -format binary -t 0x0
   ```

## Expected result

The source command programs ten 10 ms periods that alternate bus values `0x00000001` and `0x00000000`, with final value `0x00000000`. The canonical `capture.seq` contains regular run-length records for values observed by readback. The replay command is finite, drives only `qout[0]` high in nonzero runs, and supplies final value `0x00000000` through `-t 0x0`.

Readback records samples only while `qout_valid` is high, not the author's original control flow. It therefore does not reconstruct the `ppfg` trigger element, replay-preprocessor structure, forced-trigger request, no-strobe states, or elapsed invalid gaps. The binary file is a field-preserving current-build snapshot of the captured `Sequence`, but that captured sequence already consists only of normalized qualified runs rather than the source program.

The VCD exporter uses `$timescale 10ns`. A VCD tick represents physical 10 ns only when the capture used a 100 MHz streamer clock. At another clock frequency, the VCD scale is a file interpretation default, not a measured physical-time calibration.

## Cleanup

Return the board to an explicit zero idle state:

```bash
ppreset -i 0x0
```

The reset actively drives zero. Before connecting another driver, power down or release the bidirectional pins with `ppread -oe 0 -hard-timeout 100ms`.

Retain the three capture files with the reproducibility record, or remove them only after their contents and hashes are no longer needed.

## Troubleshooting/negative control

Use `qout[1]` as the negative control; the source command never sets bit 1, so an observed transition there indicates a wiring, routing, or capture interpretation problem.

If all capture files are empty, repeat the exact two-terminal order and wait for the terminal 1 status line before starting terminal 2. Do not replace this coordination with a background process and arbitrary sleep. If VCD time disagrees with a separately measured interval, record the active streamer clock and apply the 10 ns interpretation only for 100 MHz. If replay waits for a trigger, verify `-force`; captured readback text does not contain the source command's force request.

## Reproducibility record

| Field | Record |
| --- | --- |
| Date and operator | |
| PulsePins commit or release image | |
| Host version and commit from startup | |
| Bitstream timestamp and ABI version | |
| Reported streamer-clock frequency | |
| Terminal 1 command and start time | |
| Terminal 2 command and start time | |
| Capture filenames, sizes, and hashes | |
| Canonical artifact | `capture.seq` |
| Replay command | `ppplay -force -file capture.seq -format text -t 0x0` |
| Programmed final value | `0x00000000` |
| Observed result | |

## Related pages

* [Sequence file format](../sequence_format.md)
* [`ppread`](../ppread.md)
* [`ppplay`](../ppplay.md)
* [Readback](../readback.md)
* [`ppfg`](../ppfg.md)
* [`ppreset`](../ppreset.md)
* [C++ sequence formats](../cpp.md#data-types-for-sequence-representation)
