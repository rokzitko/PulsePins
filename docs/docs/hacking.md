# Hacking on PulsePins

There are several useful ways to contribute without needing to understand the whole system at once.

## Choose a path

### Documentation and examples

Good if you want to help people get started more easily.

Typical work:

* improve explanations
* add examples and recipes
* document real hardware workflows
* improve contributor onboarding

Reproducible manual examples are especially encouraged.

Start here:

* [`README.md`]({{ source_file("README.md") }})
* [`HACKING.md`]({{ source_file("HACKING.md") }})
* [Build and deployment](build.md) ([source]({{ source_file("docs/docs/build.md") }}))
* [Testing procedures](testing.md) ([source]({{ source_file("docs/docs/testing.md") }}))

### C++ tools and API

Good if you like systems programming, command-line tools, and structured data representations.

Typical work:

* improve `pptool`
* add output formatting or better errors
* improve sequence parsing and validation
* extend helper classes for hardware blocks

Start here:

* [`c++/`]({{ source_file("c++/") }})
* [C++ application programming interface](cpp.md) ([source]({{ source_file("docs/docs/cpp.md") }}))
* [pptool](pptool.md) ([source]({{ source_file("docs/docs/pptool.md") }}))

### Python bindings

Good if you want a higher-level interface or notebook-friendly workflows.

Typical work:

* improve Python packaging and examples
* add tests
* add binding coverage for more C++ APIs

Start here:

* [`python/`]({{ source_file("python/") }})
* [Python bindings](python.md) ([source]({{ source_file("docs/docs/python.md") }}))

### RTL and simulation

Good if you want to work on the hardware architecture.

Typical work:

* improve IP blocks
* add or extend testbenches
* improve CDC/reset structure
* document register maps and interfaces

Simulation-only RTL work is useful. Testbenches help with both verification and documentation of interfaces and behavior.

Start here:

* [`ip/`]({{ source_file("ip/") }})
* [Development](development.md) ([source]({{ source_file("docs/docs/development.md") }}))
* [Implementation details](details.md) ([source]({{ source_file("docs/docs/details.md") }}))

## Without hardware

Useful work is possible without a board.

The most accessible areas are:

* docs
* Python bindings
* parsing and sequence logic in C++
* RTL simulation and testbenches

Helpful commands:

```bash
make -C docs site
make -C python USE_PREGENERATED=1 build test-host
make -C ip test
```

## With hardware

If you have a DE10-Nano and the PulsePins environment running, you can also work on:

* runtime behavior of CLI tools
* end-to-end streaming verification
* frequency/timestamp/trigger workflows
* board bring-up and shield documentation
* Pmod, clocking, and instrument-integration writeups

See also [Hardware setup](getting_started_hardware.md).

## What makes a good first contribution?

Small, useful, and verifiable changes are ideal.

Examples:

* clarify a confusing doc page
* add a missing command example
* add a simulation test for a bug fix
* improve an error message
* add a recipe for a real lab workflow

Community contributions such as wiring diagrams, timing diagrams, screenshots, scope traces, logic-analyzer captures, and photos of custom hardware setups are also very welcome.

## Community direction

Typical application areas where community examples would be appreciated include:

* pulse sequencing for lasers, shutters, and detectors
* synchronized triggering of cameras and instruments
* delay generation for time-resolved measurements
* DDS/DAC control for frequency or amplitude sweeps

GitHub Issues are a good place to share ideas, questions, setup notes, and example writeups.
