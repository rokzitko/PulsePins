# First text sequence

## Target profile/status

This chapter targets current `main`, host/FPGA ABI 6, and the released 32-bit DE10-Nano profile. Older release images may differ in available commands, options, startup output, or FPGA behavior; use documentation that matches the installed image when it is not a current-source build.

Status: this is a source-aligned procedure, not a claim of bench validation.

## Goal

Author a finite PulsePins text sequence using only the current `d`, `final`, and `f` records, then play it with `ppplay`. The sequence drives `qout[0]` high for 1,000,000 streamer cycles, drives the whole bus low for another 1,000,000 cycles, and finishes at `0x00000000`.

## Safety

PulsePins uses 3.3 V LVTTL signaling. Connect the board and instrument to a common ground, use a high-impedance scope or logic-analyzer input, and do not apply a direct 50 ohm termination to an unbuffered FPGA output. Disconnect any external source that could drive `qout[0]` while PulsePins output enable is active.

## What you need

* A DE10-Nano running a current ABI 6 PulsePins host build and matching FPGA image
* A board shell with `ppplay`, `ppreset`, and `ppread` available
* A text editor on the board or a way to place a plain text file there
* Optionally, a high-impedance oscilloscope or logic analyzer

## Wiring/setup table

| Item | Board connection | Instrument or setup |
| --- | --- | --- |
| Signal | `qout[0]`: GPIO 1 (`JP7`) pin 5 | High-impedance channel input |
| Reference | GPIO 1 (`JP7`) pin 12 or pin 30 | Instrument ground; establish common ground |
| Negative control | `qout[1]`: GPIO 1 (`JP7`) pin 6 | Optional second high-impedance channel |
| File | Board-local `qout0.seq` | Plain ASCII text with no header or comments |

## Procedure

1. Initialize the output to zero and note the reported ABI and streamer clock:

   ```bash
   ppreset -i 0x0
   ```

2. Create `qout0.seq` with exactly these records. Do not add a title, format header, blank prose, or comments; those are not records in the current parser grammar.

   ```text
   d 1000000 0x1
   d 1000000 0x0
   final 0x0
   f
   ```

3. Interpret the records before playback. `d COUNT VALUE` applies the value for `COUNT` streamer-clock cycles. `final VALUE` terminates the finite sequence and selects the persistent final bus value. `f` requests forced triggering; it does not select the final value.

4. At 100 MHz, time in seconds is `COUNT / 100000000`. One cycle is 10 ns, so a count of 1,000,000 programs 10 ms. If the active clock is not 100 MHz, use `COUNT / reported_clock_hz` instead; the same file then has a different physical duration.

5. Play the file. The `.seq` suffix is recognized as text, and `-format text` makes the selection explicit:

   ```bash
   ppplay -file qout0.seq -format text
   ```

## Expected result

At a 100 MHz streamer clock, the regular data portion programs `qout[0]` high for 10 ms and low for 10 ms. Bits `qout[31:1]` remain low because the only nonzero value is `0x00000001`. The terminal record leaves the complete bus at `0x00000000`.

The command is finite and the in-file `f` record starts it without waiting for an external trigger. These are programmed durations derived from the clock and counts, not reported bench measurements.

## Cleanup

Return to an explicit zero idle state:

```bash
ppreset -i 0x0
```

The reset actively drives zero. Before connecting another driver, power down or release the bidirectional pins with `ppread -oe 0 -hard-timeout 100ms`. Retain `qout0.seq` with the run record if it is still needed.

## Troubleshooting/negative control

Observe `qout[1]` as the negative control; no record sets that bit, so it should stay low. If playback waits for a trigger, verify that the final token in the file is the standalone `f` record. If parsing fails, remove any comments, prose, or header and compare every token with the four-record example.

If the observed duration does not match the 100 MHz calculation, use the streamer frequency printed by the same run rather than assuming 100 MHz. A host/FPGA ABI mismatch must be corrected rather than worked around in the sequence file.

## Reproducibility record

| Field | Record |
| --- | --- |
| Date and operator | |
| PulsePins commit or release image | |
| Host version and commit from startup | |
| Bitstream timestamp and ABI version | |
| Reported streamer-clock frequency | |
| Sequence filename and file hash | |
| Exact sequence text | Preserve the four records used |
| Playback command | `ppplay -file qout0.seq -format text` |
| Connector and instrument input mode | |
| Programmed final value | `0x00000000` |
| Observed result | |

## Related pages

* [Sequence file format](../sequence_format.md)
* [`ppplay`](../ppplay.md)
* [Sequencer model](../sequencer_model.md)
* [`ppreset`](../ppreset.md)
* [DE10-Nano signal reference](../de10_nano_reference.md)
* [PP_PMOD hardware reference](../pp_pmod_reference.md)
