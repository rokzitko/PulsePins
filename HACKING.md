# Hacking on PulsePins

PulsePins is meant to be hacked on, adapted, and extended.

Contributions are welcome across RTL, C++, Python, documentation, tests, board support, examples, measurement workflows, and experimental integration ideas. You do not need to start with a large feature; small fixes, examples, and documentation improvements matter too.

## Good entry points

- Documentation: clean up wording, expand guides, add examples, improve onboarding
- C++ tools: improve `pptool`, add options, improve formatting, add tests for parsing and sequence handling
- Python bindings: improve packaging, examples, and tests in `python/`
- RTL and simulation: add or improve test benches under `ip/`
- Recipes and examples: add practical command sequences in `recipes/`
- Worked examples: capture real lab workflows, timing setups, and instrument integrations

## Contribution paths

### No hardware required

You can contribute without a board if you work on:

- documentation under `docs/` and top-level `README*` files
- sequence parsing and representation in `c++/`
- Python bindings in `python/` (host-side build/import testing is useful, but production Python builds still happen on the DE10-Nano)
- HDL test benches and simulation-only RTL work under `ip/`
- recipes, examples, and test infrastructure

Good first commands:

```bash
make -C docs site
make -C python build
make -C python test-host
make -C ip test
```

For Python specifically:

- host-side `make -C python build` is useful for syntax/import/API testing
- the supported production build of the Python modules still happens on the DE10-Nano board
- true Python cross-compilation is not currently supported

### Hardware helps, but is not always required

These areas benefit from access to a board but can still be developed partially without one:

- CLI tool UX and error messages
- sequence-generation logic
- counter/timestamp/frequency-meter docs and helper code
- deployment scripts and image staging

### Hardware required

You will usually need a DE10-Nano and the PulsePins bitstream/runtime environment for:

- end-to-end streamer behavior
- timing-sensitive measurements
- clocking, PPS, and external trigger validation
- board bring-up and interface-shield testing
- hardware-backed regression verification

The default documented hardware baseline is intentionally minimal:

- `DE10-Nano` only
- latest released pre-built PulsePins image
- no extra wiring for the baseline verification flow

For that baseline, the board:

- obtains its network address via DHCP
- can be reached over SSH as user `root` with password `eit`
- can also be accessed over the UART USB serial interface if needed

## Build/test map

| Area | Main location | Hardware needed | Main command |
| ---- | ------------- | --------------- | ------------ |
| Docs | `docs/` | no | `make -C docs site` |
| C++ host tools | `c++/` | not always | `make -C c++ build` |
| Python bindings | `python/` | not always | `make -C python build && make -C python test-host` |
| RTL simulation | `ip/` | no | `make -C ip test` |
| Full FPGA build | repo root | toolchain required | `make` |
| Board deployment | repo root, `image/` | yes | `make copy_all` |

## Baseline hardware workflow

The baseline bring-up and verification flow is:

1. boot the latest released pre-built image on a DE10-Nano
2. connect over SSH
3. run `run_all_tests`

Expected result:

- the command exits successfully
- it prints a clear pass message
- it currently takes a few minutes, roughly around 7 minutes at the default 100 MHz streaming frequency

That runtime should be treated as approximate; it may change as the test suite grows.

## Updating a board from the current repository state

The current recommended contributor update flow is:

1. build the latest GitHub version
2. run `make copy_all`
3. SSH to the DE10-Nano
4. from the root home directory on the board, run:

```bash
FPGA-writeConfig -f pulsepins.rbf
```

5. run `run_all_tests` again

For now, treat this FPGA reload step as required after `make copy_all`.

## Optional hardware profile: PP_PMOD

`PP_PMOD` is an optional reference shield. It is not part of the required baseline, and it is not a prebuilt product.

Its KiCad design is published so people can build it, modify it, or use it as a starting point for custom boards.

Useful capabilities include:

- PMOD connectors for expansion modules
- one SMA trigger input with comparator threshold control
- two buffered SMA outputs
- external clock and PPS inputs
- Qwiic I2C connector
- optional on-board `AD5693` DAC
- optional on-board `MCP9808` temperature sensor
- status LEDs

Verified/useful optional workflows around `PP_PMOD` include:

- LED PMODs for visible output patterns
- onboard `MCP9808` temperature reads
- external `TMP117` over Qwiic/I2C
- `PMOD DA3` for fixed-voltage DAC output
- external clock verification with `ppfreq`
- PPS verification with `ppts`
- buffered-output use with oscilloscopes, logic analyzers, spectrum analyzers, and counters

Known-good external clock examples include:

- lab signal generators
- OCXO/GPSDO references
- CMOS oscillator modules

PPS has been checked with a GNSS receiver PPS output.

## Suggested first contributions

- improve a doc page that feels too author-centric
- add a missing example or recipe
- improve a command-line help message
- add a small simulation test case for an IP block
- fix naming consistency or formatting in tool output
- document a real hardware workflow once you have verified it

Worked examples are especially valuable. Example-driven documentation is one of the best ways to make PulsePins approachable.

### Community-contribution areas that would help a lot

- worked examples from real physics/lab scenarios
- PMOD module and sensor writeups
- custom shield or daughterboard notes
- wiring diagrams and timing diagrams
- screenshots, scope traces, and logic-analyzer captures
- photos of custom hardware setups

Typical application areas where community examples would be especially appreciated include:

- pulse sequencing for lasers, shutters, and detectors
- synchronized triggering of cameras and instruments
- delay generation for time-resolved measurements
- DDS/DAC control for frequency or amplitude sweeps

Lua scripting is also a promising extension area, but it is still early-stage and only lightly documented.

## Reliability matters

High reliability is a major project goal.

Simulation-only RTL work is therefore very valuable and strongly encouraged, especially when it improves:

- test benches
- interface validation
- CDC and reset correctness
- executable documentation of behavior

Test benches are useful not only for verification, but also for documenting interfaces and intended functionality.

## Community interaction

For now, use GitHub Issues for:

- bug reports
- ideas and feature requests
- questions
- example writeups
- hardware setup notes

## Working style

- Prefer small, focused changes
- Keep documentation close to the code it explains
- If behavior changes, update docs and tests in the same change when practical
- If hardware behavior is uncertain, document the uncertainty instead of guessing

## Where to look next

- `CONTRIBUTING.md`
- `docs/docs/build.md`
- `docs/docs/development.md`
- `docs/docs/testing.md`
- `docs/docs/getting_started_no_hardware.md`
- `docs/docs/getting_started_hardware.md`
