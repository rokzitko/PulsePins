# Wrapper reference and trigger tests

`test3` constructs bounded legal programs containing every decoder operation,
PRNG output, stores, discards and replay. It expands the expected output offline
and compares the public output stream under unrelated clocks and randomized
gating. It requires exact delivery, clean completion and the final output value.
Input durations are bounded; the test never forces internal decoder counters.

`test10`/`test11` cover trigger integration with both relative clock rates.
See `../tb_chain_trigger/README.md` for
the complete trigger matrix and mutation-test commands.
