# Preprocessor reference regression

`test5` runs on Questa or Icarus. A test-only command interpreter records stores
and expands finite replay requests independently, comparing every accepted
96-bit output under randomized stalls. It covers all eight storage positions,
pass/discard commands, randomized replay lengths/repetitions, and reset during
both finite and infinite replay. Infinite replay has a bounded checked prefix;
reset cancels the remaining stream, after which a fresh marker must be delivered.

Required scenario counters observe accepted commands and output handshakes.
`test5` must reach every storage position, pass/discard, length-one and
maximum-length replay, finite/infinite output, stalled replay, and reset during
both replay types. Directed boundary requests precede the randomized phase.

The older directed tests also gate success on their completed output checks.
`test2` initializes all eight positions and verifies every finite replay word,
including maximum-length tails, before checking an infinite-replay prefix. It
requires zero/one/maximum-length commands and actual infinite output. `test3`
requires length-one replay; `test4` requires oversized requests to be observed
as well as recovery. Each run records the required bins in `run.json`.
