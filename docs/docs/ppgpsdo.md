# ppgpsdo

`ppgpsdo` is a reference implementation of a GPS-disciplined oscillator (GPSDO) using PPS from a GNSS receiver.

**Experimental status:** this command is not currently a supported user-manual workflow. It updates a DAC from paired PPS and `sigA` timestamps, but the operator must independently establish and verify that the selected `sigA` path observes the oscillator controlled by that DAC. Do not treat the documented command structure alone as evidence of a closed physical feedback loop.

It reuses the same timestamp-capture infrastructure as `ppts`, but instead of only printing events it pairs PPS and auxiliary timestamps, derives a timing error, and drives a DAC through a PID controller.

Requirements for investigating this implementation:

* target board running a matching current PulsePins host build and FPGA image
* pulse-per-second signal from a GNSS receiver applied to the PPS pin, with `-pps_in` supplied explicitly; otherwise the retained or reset-default crystal PPS source can be used
* an oscillator-derived event, normally divided to the same nominal one-event-per-second cadence as PPS, routed with electrically appropriate conditioning to a supported `sigA` source; external trigger input 0 with `-selA 2` avoids the persisted AUX-direction setting
* AD5693 DAC on I2C bus 1 at address `0x4c`, such as the DAC on [PP_PMOD](pp_pmod_reference.md), connected to the tunable oscillator's control input

The implementation pairs the nth PPS event with the nth `sigA` event, so do not feed an undivided oscillator or a source with a different event rate. The released `sigA` mux does not include `EXT_CLKp`, and the timestamp counter runs from `core_clk`, not the selected external streamer clock. Merely connecting an oscillator to `EXT_CLKp` therefore does not establish this loop. Record the `sigA` selector, event cadence, timestamp clock source, oscillator routing and conditioning, DAC transfer function, and an independent frequency measurement before evaluating controller behavior.

AUX0 with `-selA 3` is another possible route only after verifying that AUX0 is configured as an input. Neither `ppgpsdo` nor `ppts` changes AUX direction, and that state can persist from earlier low-level access. With the external source disconnected, `ppreset -i 0x0` restores the reset-default AUX input direction before connection.

Command-line switches:

Timestamp routing and run control:

* ``-pps_in``: use the external PPS input for the PPS timestamp stream
* ``-pps_xtal``: use the crystal-derived PPS source for the PPS timestamp stream
* ``-selA N``: select the source routed to the secondary ``sigA`` timestamp stream
* ``-timeout T``: timeout for waiting on new timestamp samples; ``0`` disables timeout protection
* ``-nr N``: number of timestamp samples to read per stream; ``0`` means run continuously

PID and filtering:

* ``-kp``: proportional gain, default ``0.01``
* ``-ki``: integral gain, default ``0.1``
* ``-dp``: proportional deadband, default ``0``
* ``-di``: integral deadband, default ``0``
* ``-eps``: leaky-integrator epsilon, default ``0.0``
* ``-clip N``: clip accepted timing-error deltas to ``+/-N`` before averaging, default ``1000``
* ``-reject N``: reject timing-error deltas with magnitude greater than or equal to ``N``, default ``10000``
* ``-avg N``: average ``N`` accepted deltas before updating the PID/DAC output, default ``1``; must be greater than zero

`-clip`, `-reject`, `-dp`, and `-di` operate on raw `core_clk` timestamp-counter ticks, and the PID gains act on that tick-domain error. Their physical-time meaning therefore changes with `core_clk`; record its measured frequency with every set of controller settings.

DAC output mapping:

* ``-k K``: intercept for converting PID control value to DAC voltage, default ``2.6``
* ``-l L``: slope for converting PID control value to DAC voltage, default ``-0.01``
* ``-vmin V``: minimum DAC output voltage after clamping, default ``0.0``
* ``-vmax V``: maximum DAC output voltage after clamping, default ``5.0``

Controller updates use `voltage = k + l * control`, clamped to `vmin` through `vmax`. The current verbose settings line labels the coefficients in the opposite algebraic order; the expression here follows the implemented calculation.

`ppgpsdo` always reads both the PPS and ``sigA`` timestamp streams and pairs samples from those two paths.

## Hardware side effects

On every invocation, `ppgpsdo` opens I2C bus 1 at address `0x4c`, configures the DAC for gain 2 with its internal reference enabled, and immediately writes `2.6 V`. This initial write occurs before timestamp reads and is not passed through `-vmin` or `-vmax`. Later accepted controller updates use the configured conversion and clamp. The command does not restore the previous DAC value on exit; the last programmed voltage remains in place.

Implementation notes:

* timestamp capture and pairing live in [`c++/pptool_measurement.cc`]({{ source_file("c++/pptool_measurement.cc") }})
* timestamp routing comes from [`c++/timestamp.hh`]({{ source_file("c++/timestamp.hh") }})
* DAC control is handled through the I2C-backed helper used in the same command implementation

See also [PP_PMOD hardware reference](pp_pmod_reference.md).
