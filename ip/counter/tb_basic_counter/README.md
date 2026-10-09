# Basic counter tests

`make test2` uses unrelated clocks and 8-bit counters with 4-bit readout words.
It reduces recorded public input samples into independent totals, levels and
transition counts at each latch edge. It checks snapshots while counting
continues, upper/lower words, reset during valid traffic, natural saturation,
overflow synchronization and clearing, including the exact 255-to-256 sample
boundary. Snapshots represent pre-edge working
counts, including when a new sample arrives on the latch edge.
