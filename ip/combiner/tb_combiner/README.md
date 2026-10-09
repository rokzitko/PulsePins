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

Required scenario reports count checked outputs separately for all 16 modes
(at least 128 each), plus output forcing and each of the four input-force paths.
An input-force bin is credited only while its input is selected and the output
matches the reference. Directed selections ensure every force affects a checked
result; programming a force bit in an unrelated mode is insufficient.
The older directed/random benches also require their selected modes to be
observed with nonidle inputs and their complete check sequences to finish.
