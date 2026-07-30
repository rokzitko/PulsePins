# First finite output

## Target profile/status

This chapter targets current `main`, host/FPGA ABI 6, and the released 32-bit DE10-Nano profile. Older release images may differ in available commands, options, startup output, or FPGA behavior; use documentation that matches the installed image when it is not a current-source build.

Status: this is a source-aligned procedure, not a claim of bench validation.

## Goal

Generate ten finite 10 ms periods on `qout[0]` with the default 50 percent duty cycle. Bits `qout[31:1]` stay low, and the explicit final bus value is `0x00000000`.

## Safety

PulsePins uses 3.3 V LVTTL signaling. Connect the board and instrument to a common ground, use a high-impedance scope or logic-analyzer input, and do not apply a direct 50 ohm termination to an unbuffered FPGA output. Disconnect any external source that could drive `qout[0]` while PulsePins output enable is active.

## What you need

* A DE10-Nano running a current ABI 6 PulsePins host build and matching FPGA image
* A board shell with `ppfg`, `ppreset`, and `ppread` available
* A high-impedance oscilloscope or logic analyzer
* One signal lead and one ground lead

## Wiring/setup table

| Item | Board connection | Instrument or setup |
| --- | --- | --- |
| Signal | `qout[0]`: GPIO 1 (`JP7`) pin 5 | High-impedance channel input |
| Reference | GPIO 1 (`JP7`) pin 12 or pin 30 | Instrument ground; establish common ground |
| Negative control | `qout[1]`: GPIO 1 (`JP7`) pin 6 | Optional second high-impedance channel; it should remain low |
| Other outputs | `qout[31:2]` | Leave disconnected for this procedure |
| Input mode | 3.3 V digital output | No direct 50 ohm termination on an unbuffered FPGA pin |

## Procedure

1. With external drivers disconnected, put the primary streamer in a zero-valued idle state:

   ```bash
   ppreset -i 0x0
   ```

2. Confirm that the startup report identifies ABI version 6. Stop if the host build and FPGA image do not match.

3. Run the same finite command used by the quick start:

   ```bash
   ppfg -burst 10 -period 10ms -trig -v1 0x1 -v0 0x0 -t 0x0
   ```

4. Observe `qout[0]` until the instrument has captured all ten periods. `ppfg` returns after queueing and activating the burst, not after hardware completion, so the shell prompt is not a completion indication. Do not replace `-burst 10` with `-cont`; this chapter intentionally uses a finite sequence.

## Expected result

The programmed waveform contains ten 10 ms periods. During each high phase the bus value is `0x00000001`, so only `qout[0]` is high. During each low phase and after completion the bus value is `0x00000000`. The requested 50 percent duty cycle divides each period into high and low phases after conversion to streamer-clock counts.

The output drivers remain enabled after the command exits even though the final value is zero. This describes the programmed waveform and does not assert a measured edge rate, voltage, or physical timing result.

## Cleanup

After observing the complete burst, stop the streamer at an explicit zero idle value:

```bash
ppreset -i 0x0
```

This reset still actively drives zero. A passive high-impedance probe can be removed, but before connecting another driver, power down or release the bidirectional pins:

```bash
ppread -oe 0 -hard-timeout 100ms
```

## Troubleshooting/negative control

Use `qout[1]` as a negative-control observation point: it should remain low for the entire command because both `-v1 0x1` and `-v0 0x0` clear bit 1.

If `qout[0]` does not show the programmed burst, verify the physical bit mapping, common ground, high-impedance input setting, startup ABI report, and exact command. If the period is unexpected, record the streamer-clock frequency printed at startup and check that no clock-selection or PLL options came from the environment.

## Reproducibility record

Record values from the actual run rather than filling in assumed measurements.

| Field | Record |
| --- | --- |
| Date and operator | |
| PulsePins commit or release image | |
| Host version and commit from startup | |
| Bitstream timestamp and ABI version | |
| Reported streamer-clock frequency | |
| Exact command | `ppfg -burst 10 -period 10ms -trig -v1 0x1 -v0 0x0 -t 0x0` |
| Connector and `qout[0]` pin used | |
| Instrument and input impedance setting | |
| Programmed final value | `0x00000000` |
| Observed result | |

## Related pages

* [Quick start](../quick_start.md)
* [Sequence file format](../sequence_format.md)
* [`ppfg`](../ppfg.md)
* [`ppreset`](../ppreset.md)
* [DE10-Nano signal reference](../de10_nano_reference.md)
* [PP_PMOD hardware reference](../pp_pmod_reference.md)
