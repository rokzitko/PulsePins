# Trigger verification

The trigger suite has three complementary layers:

* `../tb_and_trigger/test2`: 256 mask instances check all **16,777,216**
  eight-bit input/pattern/mask combinations. Checks include the sampling edge,
  reset priority, disabled matching, re-enable, and sticky activation.
* `test2`/`test3`/`test4` here: unrelated write/read clocks, with each domain
  faster in one configuration and coincident clock edges in another;
  every input bit and polarity;
  zero/full masks; 64 randomized programs; explicit and implicit final records;
  reset after a sampled match; reset/retrigger during LOAD and WAIT; force/reset
  priority; preservation versus discard of the active record; full-queue reset;
  identical successive conditions under continuously held input/enable;
  concurrent producer/consumer operation across three queue wraps. The oracle
  predicts behavior from queued program records, not DUT state encodings.
* `../tb_st_interface/test10`/`test11`: public wrapper integration with both
  faster-control and faster-playback clock configurations,
  32 pin/polarity/enable-source cases, four force/reset-source combinations,
  gate-closed activation, enable removal after activation, retrigger reset,
  and global reset of armed/active playback. Every output sample is checked.

```sh
make -C ip/streamer/tb_and_trigger test2
SEED=42 make -C ip/streamer/tb_chain_trigger test2
make -C ip/streamer/tb_st_interface test10
python3 ip/streamer/tb_chain_trigger/test_faults.py
```

`test_faults.py` checks good baselines, then requires 16 isolated RTL mutations
to fail functional assertions/watchdogs. It covers mask/enable/latch/reset
defects, queue record loss, premature/missing finals, priority errors, and
wrapper control/input wiring faults. Mutated copies and logs remain under
`.sim/mutations/`; production sources are never edited. The portable subset is:

```sh
python3 ip/streamer/tb_chain_trigger/test_faults.py --primitive-only --sim icarus
```

The chain engine intentionally treats an empty queue as an implicit final stage.
Multi-stage programs are therefore queued before matching is enabled. A
`trigger_reset` re-arms the active record; `retrig` discards it and proceeds to
the next queued program. Tests check these different contracts explicitly.

Required coverage is reported before success. The asynchronous chain bench
samples the DUT's pre-edge state when trigger reset, retrigger, or read reset is
actually applied, and requires each control in all four states. State observation
is used only for coverage; the functional oracle remains queue-based. Further
bins require a full-queue reset, producer backpressure, checked delivery across
three queue wraps, all 64 randomized programs, and the selected clock ordering
(plus coincident edges in that configuration). The exhaustive primitive reports
its complete predicate/control counts. Wrapper tests report every one of the 32
pin/polarity/enable cases and four force/reset combinations independently, so an
aggregate count cannot hide a missing case. See each run's `run.json` for counts.
