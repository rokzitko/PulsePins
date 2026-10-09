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

Before success, benches require named counts from the playback domain or their
functional checkers. Legacy tests select the relevant activation/completion,
gate-hold, retrigger, underrun, and checked-payload bins. `test14` separately
requires checked output for every operation, accepted zero-length runs,
stalls/recovery during a run, and reset while a run is still active; idle reset
does not satisfy that requirement. `test15` requires checked underrun-policy
outcomes and post-reset recovery for both policies. Reports are retained under
`scenario_coverage` in each run's `run.json`.
