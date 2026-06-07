# Readback

Readback is the name of the run-length encoder subsystem connected directly to the PulsePins streamer output.

Its job is to observe streamed symbols, compress them back into `{count, value}` runs, and expose those runs to software for verification, debugging, and external-signal capture. PulsePins uses this path heavily for self-test: software can compare the observed output stream against the reference sequence that was originally sent to the streamer.

Besides verification, the readback path lets PulsePins act as a simple digital logic analyzer: it can observe digital activity, compress it on-chip, and export the captured deterministic waveform as a VCD file for standard waveform viewers.

The acquired sequence uses the same top-level element format as the streamer (`control`, `counter`, `value`), but in practice the control field is always zero and software interprets the readback as plain `BITLOAD` output states.

The bus widths in ``ip/rl_encoder_if/rl_config.vh`` need to match those in ``ip/streamer/config.vh``.

For a maintainer-oriented RTL map, see `ip/rl_encoder_if/README.md`.

## Architecture overview

At a high level the flow is:

1. sampled output values arrive on `qin`
2. `rl_encoder.sv` groups consecutive equal samples into runs
3. a dual-clock FIFO buffers those runs for software-side reads
4. `rl_encoder_if.sv` exposes the encoded runs on Avalon-ST and adds control/status registers
5. `c++/readback.hh` either dumps the captured stream or compares it against a reference `Sequence`

This makes the readback path the main verification seam between the generated FPGA output and the host-side model.

## `rl_encoder` core

Inputs:

  * `qin`: observed data word, usually connected to streamer `qout`
  * `qin_valid`: sampled-data validity qualifier
  * `qin_clk`: sampled-data clock
  * `qin_strobe`: strobe signal retained for the dormant `WEIRD_CLOCK` alternate mode

Important behavior:

* equal consecutive samples are merged into one run-length element
* normal builds sample `qin` on `qin_clk` while `qin_valid` is asserted
* the older strobe-clocked mode is hidden unless the RTL is explicitly built with `WEIRD_CLOCK`
* when validity drops, the pending run is flushed so the final observed state is not lost
* overflow latches high if software is not draining the encoded FIFO fast enough

## rl_encoder_if interface

`rl_encoder_if.sv` provides:

* Avalon-ST output carrying encoded readback elements
* Avalon-MM control/status for reset, active-mode status, FIFO-empty state, overflow, observed pulse count, and CRC32

The pulse counter and CRC are computed in the sampled-input domain, which lets software compare transport-level integrity against the transmitted reference stream.

## Software interface

The software interface is provided through class `readback` in `c++/readback.hh`.
The key member functions are:

  * `reset`: reset the encoder and discard stale FIFO contents
  * `clear_fifo`: read and discard all queued elements
  * `filled`: report whether encoded elements are available
  * `check_fill_status`: print FIFO status/debug information
  * `read`: read one encoded element
  * `read_all`: dump the captured stream until timeout or external termination
  * `check`: compare the captured stream against a reference `Sequence`

Captured deterministic waveforms can also be turned into VCD files through the `Sequence` export path in `c++/sequence.hh`.

At the CLI level, `ppread` can now save captures as PulsePins text sequence files, as VCD waveforms, or as the exact binary sequence format.

The `check` function returns true if no errors are detected. A timeout argument can be provided; if no new elements are received during the specified interval, an exception is raised. Higher-level playback tools now default to a conservative safe policy when no explicit timeout is provided: 2s waiting for the first readback element and 2s for later idle gaps. An exception is also raised if the reference sequence is exhausted and a new element is received from the encoder. A report is produced when the check completes, including the number and ratio of errors plus the difference in encoded size and effective output length.

## Readback of external signals

If the output enable signal ``oe`` is low, the readback core is reading signals from the external
pins, i.e. the 32 `qout` pins actually act as inputs, and the `streamer_qout_valid` pin starts
acting as the valid signal input port. The [ppread](ppread.md) tool can be used to read the
generated data stream. One application of this tool is to troubleshoot another PulsePins board
by connecting the qout and qout_valid ports together and clocking both devices from the same clock
source. In this mode, PulsePins effectively becomes a simple external digital logic analyzer for
the qout bus and valid signal.
