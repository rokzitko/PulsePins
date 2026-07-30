# Hardware setup

Use this page for board access, signal-level context, and optional hardware after following the [Quick start](quick_start.md). Build and deployment workflows for repository changes are documented separately under [Build and deployment](build.md).

## Released baseline

The default supported setup is intentionally small:

* one [DE10-Nano](https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=167&No=1046)
* the latest released PulsePins SD-card image
* no shield or external loopback wiring for baseline validation

Use the Terasic board documentation for power, physical header pins, UART driver setup, switches, and board-level electrical limits. The [DE10-Nano signal reference](de10_nano_reference.md) documents the PulsePins assignments within the FPGA design.

## Network access

The released image requests an Ethernet address using DHCP. Its default MAC address is `D6:7D:AE:B3:0E:BA`; use that value to identify the board in the router's DHCP lease list.

Connect with:

```bash
ssh root@BOARD_IP
```

The initial password is `eit`. Change it with `passwd` before using the board on an untrusted network. For passwordless access, run `ssh-copy-id root@BOARD_IP` from the development machine.

## UART fallback

The USB mini connector marked `UART` provides a serial console when Ethernet configuration or address discovery is not working. Connect it to a host, open the serial device using the settings documented by Terasic for the DE10-Nano, and log in with the same released-image credentials.

Use the serial console to inspect or change network configuration before retrying SSH.

## Output and input signals

PulsePins uses 3.3 V LVTTL signaling. Check voltage compatibility and establish a common ground before connecting laboratory equipment or another digital device.

Useful references:

* [DE10-Nano signal reference](de10_nano_reference.md) for base-board assignments, output enable, LEDs, and manual trigger controls
* [PP_PMOD hardware reference](pp_pmod_reference.md) for connector-level details on the optional reference shield
* [Sequencer model](sequencer_model.md#output-enable-and-readback) for high-impedance input/readback behavior
* [Readback](readback.md#readback-of-external-signals) for external signal capture

## Baseline validation

Before running the board tests, disconnect external circuits from driven PulsePins signals or verify that they can safely accept generated 3.3 V patterns. The tests enable outputs and exercise nonzero values.

Run `run_all_tests` before diagnosing optional hardware or developing a larger experiment. A clean run prints `SUCCESS`; see [Quick start](quick_start.md#4-validate-the-baseline-image) for the first-board procedure and [Testing procedures](testing.md) for test levels, logs, and troubleshooting.

For a first externally observed signal, follow [First finite output](manual/first_output.md), then continue with the [User manual](examples.md).

## Optional PP_PMOD profile

`PP_PMOD` is a published KiCad reference shield, not a required baseline or prebuilt accessory. It breaks out the output, trigger, AUX, clock, PPS, I2C, and status signals and can be used directly or adapted into a custom interface board.

Digital trigger routing can be checked with `pptrig`, but the thresholded SMA comparator path should not be treated as systematically validated for every assembly and signal source. Record its threshold, termination, source, and observed waveform when testing it.

Start with:

* [ppboards shield overview](ppboards.md)
* [PP_PMOD overview](pp_pmod.md)
* [PP_PMOD hardware reference](pp_pmod_reference.md)

Record the exact shield revision, populated options, jumper positions, cabling, and external equipment whenever reporting a hardware result.

## Working from a repository build

For hardware-backed development rather than released-image setup:

* use [Build and deployment](build.md#recommended-workflows) for `make`, `make copy_all`, and the required FPGA reload
* use [Testing procedures](testing.md) to choose between `make dev-check`, `make board-smoke`, and `run_all_tests`
* use the [Contributor overview](hacking.md) for subsystem-specific starting points

When validating a change, record the board revision, image and software versions, clock source, wiring, exact commands, expected result, and observed result.
