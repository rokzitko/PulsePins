# Wrapper reference and trigger tests

`test3` constructs bounded legal programs containing every decoder operation,
PRNG output, stores, discards and replay. It expands the expected output offline
and compares the public output stream under unrelated clocks and randomized
gating. It requires exact delivery, clean completion and the final output value.
Input durations are bounded; the test never forces internal decoder counters.

`test10`/`test11` cover trigger integration with both relative clock rates.
See `../tb_chain_trigger/README.md` for
the complete trigger matrix and mutation-test commands.

Scenario reports are mandatory before success. The reference program requires
each decoder operation to produce checked output, plus observed stores, discards,
finite replay, gate stalls, and completed delivery. Directed wrapper tests report
their applicable payload/completion, gate, retrigger, or error cases from the
playback clock domain. Trigger integration additionally requires each individual
pin/control combination, armed and active resets, and gated activation; the
same requirements apply in both relative clock configurations.
