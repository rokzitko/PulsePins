# ppgpsdo

`ppgpsdo` is a reference implementation of a GNSS-disciplined oscillator.

It reuses the same timestamp-capture infrastructure as `ppts`, but instead of only printing events it pairs PPS and auxiliary timestamps, derives a timing error, and drives a DAC through a PID controller.

Requirements:

* FPGA running the PulsePins design with an external clock signal (`EXT_CLK`) from a tunable oscillator such as an OCXO or TCXO
* pulse-per-second signal from a GNSS receiver applied to the PPS pin
* DAC on [PP_PMOD](ppboards.md) controlling the frequency of the tunable oscillator

Command-line switches:

* ``-kp``: coefficient for proportional part of PID

Implementation notes:

* timestamp capture and pairing live in `c++/pptool_measurement.cc`
* timestamp routing comes from `c++/timestamp.hh`
* DAC control is handled through the I2C-backed helper used in the same command implementation

Because the underlying timestamp core is designed for sparse events, `ppgpsdo` assumes PPS-like timing signals rather than dense arbitrary transitions.
