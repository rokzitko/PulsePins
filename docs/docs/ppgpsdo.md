## ppgpsdo

`ppgpsdo` is a reference implementation of a GNSS-disciplined oscillator.

Requirements:

* FPGA running the PulsePins design with an external clock signal (`EXT_CLK`) from a tunable oscillator such as an OCXO or TCXO
* pulse-per-second signal from a GNSS receiver applied to the PPS pin
* DAC on [PP_PMOD](ppboards.md) controlling the frequency of the tunable oscillator

Command-line switches:

* ``-kp``: coefficient for proportional part of PID
