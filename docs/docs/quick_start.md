# Quick start

This is the shortest path from a released SD-card image to a validated DE10-Nano and a first digital output.

**Version note:** this manual follows the current source tree and can be newer than the latest published binary image. Check the selected release notes for its included commands. If a documented command is missing, use documentation from that release or deploy a matching current-source build.

## What you need

* a DE10-Nano with its power supply
* a microSD card and card reader
* Ethernet access, or a USB mini cable for the UART console
* optionally, an oscilloscope or logic analyzer for observing the first output

No shield or external wiring is required for the baseline self-test.

## 1. Download and flash the image

Download the latest binary SD-card image from the [PulsePins releases page](https://github.com/rokzitko/PulsePins/releases).

Write the image to the microSD card with a tool such as BalenaEtcher, Raspberry Pi Imager, or Rufus. Insert the card into the DE10-Nano before applying power.

## 2. Boot and find the board

Connect Ethernet and power on the board. The released image requests an address using DHCP. Its default Ethernet MAC address is `D6:7D:AE:B3:0E:BA`.

Find the assigned address in your router's DHCP lease list. If Ethernet is unavailable, connect through the USB mini port marked `UART`; the [hardware setup](getting_started_hardware.md) page covers the fallback path and board-access details.

## 3. Log in

Connect over SSH, substituting the board's address:

```bash
ssh root@BOARD_IP
```

The released image initially uses:

* user: `root`
* password: `eit`

Change the default password before placing the board on an untrusted network:

```bash
passwd
```

You can also install your public SSH key with `ssh-copy-id root@BOARD_IP` from your development machine.

## 4. Validate the baseline image

Disconnect external circuits from the PulsePins output, valid, and strobe pins, or first verify that they can safely accept generated 3.3 V patterns. The self-tests enable the output drivers and exercise nonzero values even though they do not require external loopback wiring.

On the board, run:

```bash
run_all_tests
```

The command should exit successfully and print `SUCCESS`. At the default 100 MHz streamer clock, a full run currently takes about seven minutes. It uses the FPGA readback path for internal comparison.

If the check fails, keep the complete output and continue with [Testing procedures](testing.md).

## 5. Generate a first output

PulsePins uses 3.3 V LVTTL signals. Disconnect equipment that must not be driven, then use the [DE10-Nano signal reference](de10_nano_reference.md) or the [PP_PMOD hardware reference](pp_pmod_reference.md) to connect `qout[0]` and ground to a high-impedance scope or logic-analyzer input. Do not apply a direct 50 ohm termination to an unbuffered FPGA pin.

Generate ten 10 ms periods on `qout[0]`, returning the bus to zero afterward:

```bash
ppfg -burst 10 -period 10ms -trig -v1 0x1 -v0 0x0 -t 0x0
```

Expect ten 50% duty-cycle pulses on `qout[0]`; the other output bits remain low. The final value is zero, but the output drivers remain enabled after the command completes.

## Optional burn-in

For a longer board check, run:

```bash
run_all_tests_forever
```

The runner stops at the first failed sweep. Current-source builds write summary and per-run logs under `/var/volatile/pulsepins-test-logs`; older images may differ. See [Testing procedures](testing.md) for current log details and reporting options.

## Where to go next

* [Choose the right tool](choose_tool.md) for a task-to-interface map
* [Worked examples](examples.md) for complete lab workflows
* [Sequencer model](sequencer_model.md) for the data and trigger model
* [Hardware setup](getting_started_hardware.md) for board access and optional hardware
* [Python API](python.md), [C++ API](cpp.md), or [SCPI server](ppscpi.md) for automation
