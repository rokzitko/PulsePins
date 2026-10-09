# Ingress FIFO scenario requirements

All three benches report observed handshakes and completed checks before success:

* `test1`: five accepted writes/reads, a buffered read pause and resumed reads,
  three show-ahead checks, and final drain.
* `test2`: all 65 words from three bursts delivered in order, at least one write
  blocked by `full`, accepted writes after pressure clears, and final drain.
  The consumer waits for actual full/backpressure; the producer holds each word
  until accepted. Both must complete before success, with a fatal watchdog.
* `test3`: checked randomized bursts, actual blocked/resumed writes, and matched
  counters on both drained FIFO stages. Its first directed burst reaches
  backpressure before the consumer starts, so coverage is not seed-dependent.

The counts appear under `scenario_coverage` in the runner's `run.json`.
