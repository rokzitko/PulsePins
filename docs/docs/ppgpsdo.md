## ppgpsdo

ppgpsdo is a reference implementation for a GNSS-discipled oscillator.

Requisites:

* FPGA running with PulsePins design using external clock signal (EXT_CLK pin) fron a tunable oscillator (e.g. OXCO, TXCO)
* pulse-per-second signal from a GNSS received applied to PPS pin
* DAC on [PP_PMOD](ppboards.md) controls the frequency of the tunable oscillators

Command line switches:

* ``-kp``: coefficient for proportional part of PID
