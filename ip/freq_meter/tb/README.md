# Frequency-meter boundary regression

`test2` uses three ideal input periods, independently counted input edges, and
eight-bit hardware counters. It checks stopped/restarted inputs, different
phases, gate lengths 0/1/17/24/96, disable/re-enable, natural wraps and Gray-code
coherence at every input edge.

Two gate writes are issued between consecutive reference-clock edges; the final
value must determine subsequent measurements. Back-to-back clear requests must
publish zero rather than cancel each other. The bus clock is also slowed
below the reference clock with one-cycle gates, then inputs are stopped: fresh
zero results must reach software. These cases reject toggle cancellation and
result-notification aliasing, respectively. Both Questa and Icarus run this test.
