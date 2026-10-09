# Combiner reference regression

`test3` is shared by `combiner`, `combiner_comb`, and `combiner_trig`; each
directory's `run3` selects the corresponding DUT. It checks all 16 mode codes
(including reserved-mode fallback), cycle-varying inputs, input/output forcing,
inversion/masking, every register, and live-port readback.

A population-count reference implements logical modes independently of the
RTL expressions. Arithmetic includes zero, all ones, sign boundaries, carry
and underflow. Registered variants must have the documented two-register
latency; the combinational variant is checked without that delay. Configuration
uses an unrelated clock, and rapid writes must converge before datapath checks
resume after a bounded settling interval.
