# Tick boundaries

`test2` checks exact phase and inter-tick spacing for periods 1, 2, 3, 7, 8, 9
and 16, including reset away from a tick boundary. `PERIOD=1` requires a minimum
one-bit counter and produces a tick on every cycle. Nonpositive periods and
explicit counter widths too small for the period are rejected.
