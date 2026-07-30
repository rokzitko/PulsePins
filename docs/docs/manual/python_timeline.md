# Workstation Python timeline

## Target profile/status

This chapter targets current `main`, host/FPGA ABI 6, the released 32-bit DE10-Nano profile, and the current pure-Python `pulsepins` workstation client. Older release images or older Python packages may differ in server commands and `Timeline` methods; use software and documentation from a matching revision.

Status: this is a source-aligned procedure, not a claim of bench validation.

## Goal

Run the current workstation SCPI route against a specifically addressed board. A `Timeline` programs `qout[0]` low for 10 us, high for 5 us, then explicitly returns the complete bus to `0x00000000`. The generated PulsePins text is printed before the finite sequence is uploaded.

## Safety

PulsePins uses 3.3 V LVTTL signaling. Connect any instrument and board to a common ground, use a high-impedance input, and do not apply a direct 50 ohm termination to an unbuffered FPGA output. Disconnect any external source that could drive `qout[0]` while output enable is active.

`ppscpi` has no authentication or transport encryption. Bind it to the board's specific address, use it only on a trusted network, and do not expose TCP port 5025 to an untrusted network.

## What you need

* A DE10-Nano running a current ABI 6 PulsePins host build and matching FPGA image, with `ppread` and `ppreset` available in its shell
* A specific IP address configured on the board
* A trusted network path from a workstation to TCP port 5025 on that address
* Python 3 and the current repository's `python/pulsepins` package on the workstation
* Optionally, a high-impedance oscilloscope or logic analyzer

## Wiring/setup table

| Item | Connection or setting | Role |
| --- | --- | --- |
| Network | Workstation to the board's specific `BOARD_IP` on a trusted network | SCPI control on TCP port 5025 |
| Signal | `qout[0]`: GPIO 1 (`JP7`) pin 5 | Optional high-impedance observation channel |
| Reference | GPIO 1 (`JP7`) pin 12 or pin 30 | Common ground with the instrument |
| Negative control | `qout[1]`: GPIO 1 (`JP7`) pin 6 | Optional second channel; it should remain low |
| Other outputs | `qout[31:2]` | Leave disconnected for this procedure |
| Python route | Workstation `pulsepins` package | No board-native nanobind module is used |

## Procedure

1. Choose the address actually configured on the board. The private address below is an example; replace it with the same real value in both the board and workstation terminals:

   ```bash
   export BOARD_IP=192.168.10.25
   ```

2. On the board, start `ppscpi` in the foreground, explicitly select the 100 MHz internal streamer-clock profile, and bind it to that specific board address rather than `0.0.0.0`:

   ```bash
   ppscpi -ip "$BOARD_IP" -int_pll 100M -int_clk
   ```

3. On the workstation, install the current pure-Python client from the matching checkout and set the same explicit address:

   ```bash
   python3 -m pip install -e /path/to/PulsePins/python
   export BOARD_IP=192.168.10.25
   ```

4. Run this current high-level API workflow on the workstation. It lets `pp.timeline(unit="us")` obtain the selected 100 MHz streamer clock from the board, uses `include_final=True`, previews exactly the generated text, streams one finite timeline, resets to zero, and terminates the tutorial server:

   ```python
   import os

   from pulsepins import PulsePins


   BOARD_IP = os.environ["BOARD_IP"]
   sequence_options = {
       "force_trigger": True,
       "include_final": True,
       "final_value": 0x0,
   }

   with PulsePins(BOARD_IP) as pp:
       print(pp.idn())

       timeline = pp.timeline(unit="us", initial_value=0x0)
       timeline.channel("pulse", bit=0)
       timeline.pulse("pulse", start=10, duration=5)

       sequence_text = timeline.to_sequence(**sequence_options)
       print("Generated sequence:")
       print(sequence_text, end="")

       pp.reset()
       print(pp.run(timeline, **sequence_options))
       pp.reset()
       pp.send("TERMINATE")
   ```

5. Inspect the printed text before accepting the run. It must contain only bus values `0x0` and `0x1`, an explicit `final 0x0`, and `f` for forced triggering. Cycle counts are generated from the clock reported by this board rather than from a duplicated hard-coded conversion. Timeline start and duration values must convert to exact integer clock-cycle counts; the explicit 100 MHz profile makes 10 us and 5 us exactly representable.

## Expected result

The generated timeline is finite. It programs a zero-valued interval for 10 us, sets only `qout[0]` for 5 us, and ends with `final 0x0`; bits `qout[31:1]` stay low. `pp.run(...)` should return `SUCCESS`, after which `pp.reset()` restores a zero idle state and `TERMINATE` stops the foreground tutorial server.

The durations above are requested Timeline values converted with the board-reported streamer clock. They are not claims of measured physical pulse width or latency.

## Cleanup

The normal script path resets the hardware to zero and sends `TERMINATE`. Confirm that the foreground `ppscpi` process on the board exits. The reset still actively drives zero; before connecting another driver, power down or release the bidirectional pins from the board shell:

```bash
ppread -oe 0 -hard-timeout 100ms
```

If the workstation script is interrupted before cleanup, stop `ppscpi` from its board terminal and run:

```bash
ppreset -i 0x0
ppread -oe 0 -hard-timeout 100ms
```

The first command stops the streamer at zero and the second releases the pins before output wiring changes.

## Troubleshooting/negative control

Observe `qout[1]` as the negative control; the timeline owns only bit 0 and uses an initial and final value of zero, so bit 1 should not transition.

If the workstation cannot connect, verify that both terminals use the same explicit `BOARD_IP`, that the address is assigned to the board interface, and that TCP port 5025 is allowed only across the trusted path. If `Timeline` or `include_final` is unavailable, the Python package or board image is from an older revision; do not substitute an invented compatibility API. If generated counts are unexpected, record the board-reported streamer clock and inspect the printed sequence before running it.

## Reproducibility record

| Field | Record |
| --- | --- |
| Date and operator | |
| PulsePins commit or release image | |
| Workstation `pulsepins` package commit | |
| Host version and bitstream timestamp | |
| ABI version | `6` expected; record reported value |
| Specific `BOARD_IP` and trusted network context | |
| Reported streamer-clock frequency | `100 MHz` requested; record measured value |
| Generated sequence text and hash | |
| Timeline channel, start, duration, and final value | `qout[0]`; `10 us`; `5 us`; `0x0` |
| SCPI result | |
| Observed result or measured data | |

## Related pages

* [Sequence file format](../sequence_format.md)
* [Python bindings](../python.md)
* [`ppscpi`](../ppscpi.md)
* [Sequencer model](../sequencer_model.md)
* [Quick start](../quick_start.md)
* [DE10-Nano signal reference](../de10_nano_reference.md)
* [PP_PMOD hardware reference](../pp_pmod_reference.md)
