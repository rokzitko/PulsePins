## Miscellaneous IP blocks

The `ip/misc/` directory contains reusable support blocks that are shared across the design.

These blocks are generally not end-user features by themselves, but they are important for maintenance, integration, and understanding the overall system.

Several of them are specifically about clock-domain crossing, reset hygiene, observability, and small glue-logic transformations.

### Common utility blocks

* `sync.sv` - synchronization helpers for clock-domain crossing
* `cdc_mailbox.sv` - mailbox-style CDC support
* `reset.sv` - reset handling helpers
* `sig_mux.sv` - simple signal-selection helper logic
* `delay.sv` - delay-line style helper block

More detail:

* `sync.sv` provides two-stage and three-stage single-bit synchronizers with Quartus/Altera synchronization attributes so CDC intent remains visible to tools
* `cdc_mailbox.sv` transfers a multi-bit word between clock domains using a toggle-based notification path plus a hold/request handshake from the output side
* `reset.sv` provides `reset_sync2_hold`, a reset combiner/synchronizer with asynchronous assertion, synchronous release, and programmable hold time after release
* `sig_mux.sv` is a small parameterized bit selector that safely handles non-power-of-two input counts
* `delay.sv` currently contains `delay_or4`, a simple two-stage register/OR helper for combining delayed trigger-like signals

### Observability and diagnostics

* `activity_monitor.sv` - activity indication / observation support
* `heartbeat.sv` - periodic heartbeat generation
* `crc32.sv` - CRC32 calculation logic used for integrity checks and diagnostics

More detail:

* `activity_monitor.sv` blinks an output when the observed signal has changed recently; the same file also contains `presence_detector_async_posedge`, which latches asynchronous positive edges and holds an `active` indication for a programmable time window
* `heartbeat.sv` generates a repeated double-pulse heartbeat pattern from a single input clock
* `crc32.sv` implements a streaming reflected CRC-32 over 32-bit words with one word consumed per clock and a valid pulse aligned to the registered result

### Signal-generation helpers

* `pulse_gen_timebase.sv` - generated pulse timing helper
* `rand_signal_gen.sv` - random-signal generation helper
* `tik.sv` - small timing utility block

More detail:

* `pulse_gen_timebase.sv` derives one-clock-wide pulses at `1 ms`, `10 ms`, `100 ms`, and `1 s` intervals from a single base clock
* `rand_signal_gen.sv` generates pseudo-random test activity including baseline flips, short glitches, and multi-segment bursts, with output-enable based pausing
* `tik.sv` is a compact periodic pulse generator that asserts once every configured number of cycles
* `level_to_pulse.v` contains several level-to-pulse converters, including variants with extra delay and built-in synchronizer stages
* `endianness.sv` currently contains a 96-bit byte-order swapper used where fixed-layout word packing needs explicit reordering

### Notes for developers

When modifying these blocks, pay particular attention to:

* clock-domain assumptions
* reset polarity and reset domain
* whether a helper is intended for synthesis, simulation support, or both

For the CDC-oriented blocks in particular, preserving the synchronizer structure is usually more important than micro-optimizing the RTL.

The `ip/misc/` directory also contains dedicated test benches for several of these blocks.

### Related pages

* `development.md`
* `build.md`
