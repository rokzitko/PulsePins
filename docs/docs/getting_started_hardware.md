## Getting started with hardware

This page is a contributor-oriented starting point for work that needs a real DE10-Nano and a working PulsePins environment.

### Default baseline

The default documented hardware baseline is:

* [`DE10-Nano`](https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=167&No=1046) only
* latest pre-built PulsePins image
* no extra wiring required for the baseline verification flow

Use the linked Terasic board documentation for board-level details such as power, Ethernet, UART, GPIO headers, switches, and connectors. For SoC/HPS family context, see the [Cyclone V SoC FPGA overview](https://www.intel.com/content/www/us/en/products/details/fpga/cyclone/v.html).

The board obtains an address via DHCP by default.

Initial access details:

* SSH user: `root`
* password: `eit`
* obtain the IP from the router DHCP lease list, or use the UART USB serial interface as a fallback

### Typical hardware-backed tasks

* validating streamer behavior end to end
* checking trigger and gating behavior
* testing timestamp capture and PPS workflows
* measuring clocks with `ppfreq`
* validating board/shield wiring and auxiliary interfaces

### Basic workflow

1. Prepare a board with the latest pre-built PulsePins image
2. Connect over SSH or serial console
3. Run the host-side sanity check from your repository checkout
4. Run the baseline test on the board
5. Verify the specific subsystem you plan to modify
6. Record the exact commands and outputs that worked

Host-side sanity check:

```bash
make dev-check
```

This is the recommended precursor before moving to live-board smoke or heavier hardware testing.

### Baseline verification

The minimal verified contributor smoke test is:

```bash
run_all_tests
```

Expected result:

* the command exits successfully
* it prints `SUCCESS`
* it takes a few minutes, around 7 minutes at the default 100 MHz streaming frequency

Treat the runtime as approximate; it is expected to evolve as more tests are added.

For a faster manual regression pass from the repository checkout, use:

```bash
make board-smoke
```

This redeploys the local `pulsepins.rbf`, `pptool`, `ppscpi`, and `ppwebgui` artifacts, reloads the FPGA, kills any running `ppscpi` / `ppwebgui` processes on the board, and runs a concise finite smoke sequence. Override the board target with `TARGETHOST=...` when needed.

### Updating the board from the repository

The recommended update flow is:

1. build the latest GitHub version locally
2. run `make copy_all`
3. SSH to the DE10-Nano
4. from the root home directory on the board, run:

```bash
FPGA-writeConfig -f pulsepins.rbf
```

5. run `run_all_tests`

Treat the FPGA reload step as required after `make copy_all`.

### Optional profile: PP_PMOD

`PP_PMOD` is an optional reference shield, not a required baseline.

It is useful for experimentation and initial hardware exploration, but contributors should think of it as a published KiCad design rather than a prebuilt accessory.

Useful capabilities include:

* PMOD expansion connectors
* one SMA trigger input with comparator threshold control
* two buffered SMA outputs
* external clock and PPS inputs
* Qwiic I2C connector
* optional onboard `AD5693` DAC
* optional onboard `MCP9808` temperature sensor
* status LEDs

See also:

* [`ppboards.md`](ppboards.md)
* [`pp_pmod.md`](pp_pmod.md)
* [`pp_pmod_reference.md`](pp_pmod_reference.md)

Documented/tested optional examples and checks include:

* LED PMODs driven with `pptest` and various scripts in `tests/`
* onboard `MCP9808` via `pptemp` or `I2C/mcp9808.py`
* external `TMP117` via `I2C/tmp117.py` on the Qwiic connector
* [PMOD DA3](https://digilent.com/shop/pmod-da3-one-16-bit-d-a-output/) for fixed-voltage DAC output
* external clock checks with `ppfreq`
* PPS checks with `ppts`

Buffered outputs have been useful with:

* oscilloscopes
* logic analyzers
* spectrum analyzers
* counters

Known-good external clock source examples include:

* lab signal generators
* OCXO/GPSDO references
* CMOS oscillator modules

PPS has been checked with a GNSS receiver PPS output.

The SMA trigger input is available for external trigger experiments, but it should not yet be treated as systematically validated.

### What to document when validating hardware

Please record:

* board revision and shield setup
* image/runtime version
* clock source used
* external wiring and jumpers
* exact commands run
* expected and observed output

That information is especially useful when turning an experiment into durable project documentation.

### Current note

Worked examples from real experimental setups would be especially valuable additions here. If you validate a setup, please consider contributing the commands, wiring notes, timing diagrams, screenshots, scope traces, or photos through GitHub Issues or a pull request.
