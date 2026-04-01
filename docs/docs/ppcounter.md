## ppcounter

`ppcounter` exercises the event-counter logic and reports captured counter values.

It is implemented in `c++/pptool.cc` and uses the C++ counter interface from `c++/counter.hh`.

For the hardware architecture behind the measurements, see `counter.md`.

`ppcounter` is primarily a measurement and self-test tool for the integrated counter subsystem rather than a generic waveform tool.

The software-visible hardware wrapper is `c++/counter.hh`, and the main RTL integration block is `ip/counter/counter_if.sv`.

Common modes:

* `-test1`: run the first built-in counter test sequence
* `-test2`: run the second built-in counter test sequence
* `-check`: validate the result of `-test1`

Behavior notes:

* `-test1` uses a short deterministic pattern with known expected statistics
* `-test2` uses a longer pseudo-random sequence whose length is controlled by the usual count arguments
* the tool resets counters before streaming, then latches all results before reporting them

The report includes multiple measurement families, typically:

* basic statistics
* run-length statistics
* packet statistics
* short-sequence histogramming
* autocorrelation, and optionally crosscorrelation if enabled in the build

As with the other `pptool`-based utilities, standard verbosity and clock/streamer options are shared.

See also:

* `counter.md`
* `ip/counter/README.md`
