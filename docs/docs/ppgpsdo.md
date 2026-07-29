# ppgpsdo

`ppgpsdo` is a reference implementation of a GPS-disciplined oscillator (GPSDO) using PPS from a GNSS receiver.

It reuses the same timestamp-capture infrastructure as `ppts`, but instead of only printing events it pairs PPS and auxiliary timestamps, derives a timing error, and drives a DAC through a PID controller.

Requirements:

* target board running the PulsePins FPGA design with an external clock signal (`EXT_CLK`) from a tunable oscillator such as an OCXO or TCXO
* pulse-per-second signal from a GNSS receiver applied to the PPS pin
* DAC on [PP_PMOD](pp_pmod_reference.md) controlling the frequency of the tunable oscillator

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

DAC output mapping:

* ``-k K``: slope for converting PID control value to DAC voltage, default ``2.6``
* ``-l L``: offset for converting PID control value to DAC voltage, default ``-0.01``
* ``-vmin V``: minimum DAC output voltage after clamping, default ``0.0``
* ``-vmax V``: maximum DAC output voltage after clamping, default ``5.0``

`ppgpsdo` always reads both the PPS and ``sigA`` timestamp streams and pairs samples from those two paths.

Implementation notes:

* timestamp capture and pairing live in [`c++/pptool_measurement.cc`]({{ source_file("c++/pptool_measurement.cc") }})
* timestamp routing comes from [`c++/timestamp.hh`]({{ source_file("c++/timestamp.hh") }})
* DAC control is handled through the I2C-backed helper used in the same command implementation

See also [PP_PMOD hardware reference](pp_pmod_reference.md).
