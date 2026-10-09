# Additional streamer regressions

* `test14`: standalone decoder against the offline model in
  `../decoder_reference.svh`. All eleven operations, shift amounts 0–32,
  zero-length runs, PRNG/relative-operation composition, randomized stalls and
  reset/reload during a run are checked. This test also runs on Icarus.
* `test15`: two fixed DUT instances compare both buffer-error policies. After
  an underrun, policy 0 permits a late terminator to drain the stream without
  reporting clean completion; policy 1 stops activation and retains pending
  output. Both instances must recover to a clean, exactly counted run after reset.

The policy regression catches widening of a bitwise negation by an unsized
constant in the `trigger_activated` expression: the error policy must be a
one-bit Boolean condition.
