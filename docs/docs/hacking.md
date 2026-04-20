## Hacking on PulsePins

PulsePins is intended to be approachable to hackers, tinkerers, and experimenters, even though the project lives in a niche space.

There are several useful ways to contribute without needing to understand the whole system at once.

### Choose a path

#### Documentation and examples

Good if you want to help people get started more easily.

Typical work:

* improve explanations
* add examples and recipes
* document real hardware workflows
* improve contributor onboarding

Worked examples are especially encouraged. Example-driven docs are one of the best ways to make PulsePins approachable.

Start here:

* `README.md`
* `HACKING.md`
* `docs/docs/build.md`
* `docs/docs/testing.md`

#### C++ tools and API

Good if you like systems programming, command-line tools, and structured data representations.

Typical work:

* improve `pptool`
* add output formatting or better errors
* improve sequence parsing and validation
* extend helper classes for hardware blocks

Start here:

* `c++/`
* `docs/docs/cpp.md`
* `docs/docs/pptool.md`

#### Python bindings

Good if you want a higher-level interface or notebook-friendly workflows.

Typical work:

* improve Python packaging and examples
* add tests
* add binding coverage for more C++ APIs

Start here:

* `python/`
* `docs/docs/python.md`

#### RTL and simulation

Good if you want to work on the hardware architecture.

Typical work:

* improve IP blocks
* add or extend test benches
* improve CDC/reset structure
* document register maps and interfaces

Simulation-only RTL work is very valuable. High reliability is a major project goal, and test benches help with both verification and documentation of interfaces and behavior.

Start here:

* `ip/`
* `docs/docs/development.md`
* `docs/docs/details.md`

### Without hardware

You can still do useful work without a board.

The most accessible areas are:

* docs
* Python bindings
* parsing and sequence logic in C++
* RTL simulation and test benches

Helpful commands:

```bash
make -C docs site
make -C python build
make -C python test-host
make -C ip test
```

### With hardware

If you have a DE10-Nano and the PulsePins environment running, you can also work on:

* runtime behavior of CLI tools
* end-to-end streaming verification
* frequency/timestamp/trigger workflows
* board bring-up and shield documentation
* PMOD, clocking, and instrument-integration writeups

See also `getting_started_hardware.md`.

### What makes a good first contribution?

Small, useful, and verifiable changes are ideal.

Examples:

* clarify a confusing doc page
* add a missing command example
* add a simulation test for a bug fix
* improve an error message
* add a recipe for a real lab workflow

Community-contributed artifacts such as wiring diagrams, timing diagrams, screenshots, scope traces, logic-analyzer captures, and photos of custom hardware setups are also very welcome.

### Community direction

PulsePins is a niche project, but niche projects can still grow strong communities if they are welcoming, well documented, and easy to experiment with.

Contributions that improve usability, examples, testing, onboarding, and real-world workflows are especially valuable.

Typical application areas where community examples would be especially appreciated include:

* pulse sequencing for lasers, shutters, and detectors
* synchronized triggering of cameras and instruments
* delay generation for time-resolved measurements
* DDS/DAC control for frequency or amplitude sweeps

GitHub Issues are a good place to share ideas, questions, setup notes, and example writeups.
