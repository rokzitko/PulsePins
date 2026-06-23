# Clock domains, clock selection, and CDC

## Scope

- top-level clocks in `pulsepins.sv`
- clock switching and PLL control in `c++/`
- subsystem clock ownership
- timing constraints in `pulsepins.sdc`

## CDC background terms

A [clock-domain crossing](https://en.wikipedia.org/wiki/Clock_domain_crossing) (CDC) is any signal path whose source and destination registers are clocked by clocks with no fixed safe sampling relationship. In PulsePins, CDC is expected at FIFO, reset/control, timestamp, and frequency-meter boundaries.

[Metastability](https://en.wikipedia.org/wiki/Metastability_%28electronics%29) is the short-lived ambiguous state a flip-flop can enter when an input changes too close to a sampling edge. Synchronizer chains reduce the probability of visible failure by giving the signal extra destination-clock cycles to settle; they do not make asynchronous sampling impossible. Intel/Altera's [Understanding Metastability in FPGAs](https://www.intel.com/content/dam/www/programmable/us/en/pdfs/literature/wp/wp-01082-quartus-ii-metastability.pdf) is a useful FPGA-oriented reference.

Use single-bit synchronizers for individual flags or toggles. Use a dual-clock FIFO, mailbox, or handshake when multiple bits must be observed as one value.

[Gray code](https://en.wikipedia.org/wiki/Gray_code) changes only one bit between adjacent counts. PulsePins uses Gray-coded counters in the frequency meter so a running count can be sampled across a CDC boundary without transiently combining unrelated old and new binary bits.

## Named clocks

| Name | RTL name / location | Function | Typical nominal value |
| ---- | ------------------- | -------- | --------------------- |
| `ref_clk` | `pulsepins.sv` | board reference clock from `FPGA_CLK1_50` | 50 MHz |
| `core_clk` | `pulsepins.sv`, Qsys/Platform Designer | main system/control clock | 100 MHz default |
| `int_clk` | `pulsepins.sv`, PLL output | internal candidate streaming clock | 100 MHz default |
| `streamer_clk` | `pulsepins.sv`, streamer path | active output/streaming clock | selected source |
| `ext_clk` | concept in C++; physical `EXT_CLKp` in RTL | external candidate source for `streamer_clk` | user supplied |
| `clean_clk` | `pulsepins.sv` | regenerated external clock from simple PLL cleaner | depends on external clock |
| `d_clk` | counter IP blocks | counter data/update clock | tied to `streamer_clk` at top level |
| `cnt_clk` | `freq_meter.sv` | frequency-meter reference/gate clock | tied to `ref_clk` |
| `avs_clk` | `freq_meter.sv` | frequency-meter Avalon-MM clock | system integration defined |
| `clk` in `ts_core` | `ts_core.sv` | timestamp timebase clock | integration defined |

## Top-level clock tree

| Node | Source | Use |
| ---- | ------ | --- |
| `ref_clk` | `FPGA_CLK1_50` | reference clock, reset hold logic, frequency meter reference |
| `core_clk` | core PLL | Qsys/Platform Designer fabric, control-side logic |
| `int_clk` | internal/output PLL | internal candidate streaming source |
| `clean_clk` | simple external-clock PLL fed from `EXT_CLKp` | cleaned external candidate streaming source |
| `streamer_clk` | `int_clk`, `EXT_CLKp`, or `clean_clk` | streamer output domain, readback, counters |

## Streamer clock source selection

| Build define | `streamer_clk` source | Hardware |
| ------------ | --------------------- | -------- |
| `INTERNAL_CLK` | `int_clk` | direct assignment |
| `EXTERNAL_CLK` | `EXT_CLKp` | direct assignment |
| `EXTERNAL_CLK_CLEAN` | `clean_clk` | direct assignment |
| `SELECT_CLK` | `int_clk` or `EXT_CLKp` | `altclkctrl` |
| `SELECT_CLK_CLEAN` | `int_clk` or `clean_clk` | `altclkctrl` |

The synthesized build mode is exported to software through `gp_in[9:7]`:

| `gp_in[9:7]` | Build define |
| ------------ | ------------ |
| `0` | `INTERNAL_CLK` |
| `1` | `EXTERNAL_CLK` |
| `2` | `EXTERNAL_CLK_CLEAN` |
| `3` | `SELECT_CLK` |
| `4` | `SELECT_CLK_CLEAN` |
| `7` | invalid/no clock-mode define |

Clock-select control at top level:

- `sel_clk = gp_out[1:0]`
- `clk_ena = 1`

`altclkctrl` input ordering is not self-describing. The effective selector value depends on the synthesized mux input ordering and must be checked against the built design.

## Clock roles

| Clock | Main consumers |
| ----- | -------------- |
| `ref_clk` | PLL lock/hold logic, frequency-meter reference, low-rate utility logic, crystal-derived PPS path |
| `core_clk` | Qsys/Platform Designer system, Avalon-MM control, reset release, control-side helpers |
| `int_clk` | candidate source for `streamer_clk`, frequency-meter observed channel |
| `streamer_clk` | output FIFO read side, trigger chain, strobe generation, readback, counter data clock, activity monitor |
| `d_clk` | `basic_counter`, `runs_counter`, `packet_stats`, `seq_counter`, `autocorrelation`, `crosscorrelation` |
| `cnt_clk` | frequency-meter gate timing and result accumulation |
| `clk` in `ts_core` | timestamp counter, synchronizers, edge detection |

## C++ clock control

### PLL programming

| Clock | C++ wrapper | CLI switch | Environment variable |
| ----- | ----------- | ---------- | -------------------- |
| `core_clk` | `pll_core_clk` | `-core_pll` | `PP_CORE_PLL` |
| `int_clk` | `pll_int_clk` | `-int_pll` | `PP_INT_PLL` |

Files:

- `c++/pll_clk.hh`
- `c++/fpga.hh`
- `c++/ppwebgui_service.cc`
- `c++/ppwebgui_http.cc`

The intended split is:

- `pll_clk.hh` owns PLL reconfiguration policy and reporting
- `fpga.hh` owns the top-level clock-select GPIO path and reset sequencing around source changes
- `ppwebgui_service.cc` owns the tracked web-GUI clock state and applies it through the same hardware wrappers

### Streamer clock source switching

`FPGA::set_clk()` in `c++/fpga.hh` handles source selection for `streamer_clk`.

| Operation | CLI switch | Environment variable |
| --------- | ---------- | -------------------- |
| select internal streamer source | `-int_clk` | `PP_INT_CLK` |
| select external streamer source | `-ext_clk` | `PP_EXT_CLK` |
| raw selector write | `-clk` | `PP_CLK` |

Sequence used by software when a source change is requested:

1. hold HPS-to-FPGA reset
2. write clock-select bits
3. release HPS-to-FPGA reset

PLL programming and clock-source selection are separate operations.

`FPGA::set_clk()` only performs the reset-wrapped source-switch sequence when the caller explicitly requested a source change through `ClockSelectionOptions`.

`ppwebgui` now exposes the same source-switch path through `POST /api/clocking`, but keeps the applied clock choice in tracked controller state so later reset and stream operations reuse the updated source/profile settings instead of falling back to startup-only config. If startup used no explicit source request or a raw `-clk` selector, that state stays tracked as read-only until the user explicitly applies a managed `int_clk` or `ext_clk` choice.

### Measured streamer clock

`FPGA` stores a measured `streamer_clk` frequency in Hz.

Source:

- `pp_freq_meter` in `c++/freq_meter.hh`
- `ppwebgui` clocking routes via `c++/ppwebgui_service.cc` and `c++/ppwebgui_http.cc`

Uses:

- `wait_for_N_streamer_clk_periods()`
- timing-aware software logic that depends on the active streaming clock

## Subsystem clock ownership

### Streamer and `st_interface`

| Block | Control-side clock | Output-side clock | Main crossing |
| ----- | ------------------ | ----------------- | ------------- |
| `ip/streamer/streamer.sv` | `clk` | `streamer_clk` | dual-clock output FIFO |
| `ip/streamer/st_interface.sv` | `clk` | `streamer_clk` | control registers feeding streamer/output behavior |

Current streamer facts:

- input FIFO write side: `clk`
- output FIFO read side: `streamer_clk`
- `streamer_rst`: synchronized from `reset` into `streamer_clk`
- trigger chain: `streamer_clk`
- output read request: `trigger_activated && gate_enable`

Host-side trigger routing and software-controlled trigger bits are configured from:

- `c++/trigger.hh`
- `c++/trigger_int.hh`
- `c++/trigger_ext.hh`

### Readback and counters at top level

Top-level assignments in `pulsepins.sv`:

- `rl_qin_clk = streamer_clk`
- `counter_q_input_clock = streamer_clk`

Readback and counter sampling follow the currently selected `streamer_clk` source.

### Counter subsystem

| Block | Control/readout clock | Data/update clock |
| ----- | --------------------- | ----------------- |
| `ip/counter/counter_if.sv` | `clk` | `d_clk` |
| integrated top level | `clk` | `streamer_clk` via `counter_q_input_clock` |

Current CDC facts:

- `reset_all` is synchronized into `d_clk`
- `latch_all` is synchronized into `d_clk`
- live counter updates happen in `d_clk`
- time-counter selected input levels are synchronized into `clk` before edge detection
- readout happens from the control side after latching

### Timestamp subsystem

| Block | Timebase clock | CDC type |
| ----- | -------------- | -------- |
| `ip/ts_core/ts_core.sv` | `clk` | asynchronous input capture on `sig` and `sigA` |

Current behavior:

- free-running counter in `clk`
- synchronizer chain for `sig`
- synchronizer chain for `sigA`
- rising-edge capture in `clk`

### Frequency meter

| Domain | Signal / clock |
| ------ | -------------- |
| Avalon-MM | `avs_clk` |
| gate/reference | `cnt_clk` |
| measured channel 0 | `clean_clk` |
| measured channel 1 | `int_clk` |
| measured channel 2 | `streamer_clk` |
| measured channel 3 | `core_clk` |

CDC structure in `ip/freq_meter/freq_meter.sv`:

- Avalon control changes -> `cnt_clk` by synchronized toggles
- each measured input clock increments its own counter in its own clock domain
- counters cross into `cnt_clk` in Gray-coded form
- results cross back to Avalon by per-channel update toggles

## Reset and clock relationship

| Location | Observed / generated in | Function |
| -------- | ----------------------- | -------- |
| PLL lock observation | `ref_clk` | lock qualification |
| `core_clk_pll_ready` | `ref_clk` | release condition after stable lock |
| `reset_sync2_hold` output | `core_clk` | synchronized system reset release |
| `streamer_rst` | `streamer_clk` | synchronized reset for streamer output domain |

Reset release is domain-specific.

## CDC and reset helper blocks

| File | Function |
| ---- | -------- |
| `ip/misc/sync.sv` | single-bit synchronizers |
| `ip/misc/reset.sv` | async-assert / sync-release reset synchronizer with hold time |
| `ip/misc/cdc_mailbox.sv` | mailbox-style multi-bit CDC helper |

## [Cyclone V](https://www.intel.com/content/www/us/en/products/details/fpga/cyclone/v.html) clocking facts used by this design

### Clock networks and clock-control blocks

| Resource | Device fact | PulsePins use |
| -------- | ----------- | ------------- |
| dedicated clock inputs | Cyclone V provides dedicated clock input pins | `FPGA_CLK1_50`, `EXT_CLKp` |
| GCLK | device-wide low-skew clock network | `streamer_clk` clock-mux path |
| RCLK | regional clock network | available device resource; not the main documented PulsePins clock-selection path |
| clock control block | source selection and global clock multiplexing; dynamic selection available for GCLKs | `altclkctrl` for `streamer_clk` selection |
| PLL outputs | can drive GCLK and RCLK networks | `core_clk`, `int_clk`, `clean_clk` distribution |

### PLL facts

For the PulsePins frequency-profile calculator and the project-specific integer-PLL constraints, see [`pllcalc`](pllcalc.md).

| Item | Device fact |
| ---- | ----------- |
| PLL type | Cyclone V fractional PLLs operate in integer or fractional mode |
| counters | one `M`, one `N`, nine `C` counters per PLL |
| counter ranges | `M` and `N` range from 1 to 512 |
| synthesis model | output clocks follow `M / (N x C)` scaling |
| outputs | dedicated external outputs supported |
| features | duty cycle, phase shift, reconfiguration, dynamic phase shift |

PulsePins usage:

- PLL-generated `core_clk`
- PLL-generated `int_clk`
- external-clock cleaning PLL generating `clean_clk`
- software PLL reconfiguration through `c++/pll_clk.hh`

### Manual PLL switchover

Cyclone V PLLs support manual clock switchover inside the PLL block.

PulsePins does not use PLL manual switchover for `streamer_clk` source selection.

PulsePins uses:

- PLL generation for `core_clk` and `int_clk`
- optional PLL cleaning for the external clock
- `altclkctrl` at top level for active `streamer_clk` selection

## Timing constraints in `pulsepins.sdc`

### Declared clocks and generated clocks

| Constraint type | Current item |
| --------------- | ------------ |
| primary clock | `FPGA_CLK1_50` at 20 ns |
| primary clock | `EXT_CLKp` at 100 ns |
| HPS peripheral clocks | `HPS_I2C1_SCLK` and `HPS_USB_CLKOUT` declared for hard-HPS clock status |
| derived clocks | `derive_pll_clocks` |
| clock uncertainty | `derive_clock_uncertainty` |
| generated clock | `STREAMER_FROM_INT` on `altclkctrl` output |
| generated clock | `STREAMER_FROM_CLEAN` on `altclkctrl` output |
| exclusivity | `set_clock_groups -logically_exclusive` between internal and external streamer-mux clocks |
| CDC domain groups | `set_clock_groups -asynchronous` between core/control, reference/gate, internal-streamer, and clean/external-streamer domains |

### False paths

| False-path target | Reason |
| ----------------- | ------ |
| board/user/HPS peripheral input ports | no FPGA-owned external setup/hold contract |
| board/user/HPS peripheral output ports | no FPGA-owned external setup/hold contract |
| `u_clkctrl|auto_generated|sd2|clkselect[0/1]` | clock-select control path |
| `*pll_reconfig*` | PLL reconfiguration control path |

### Scope of the current SDC

The current SDC covers:

- primary clock declaration
- automatic PLL-derived clock inference
- streamer-clock mux output modeling with self-checking post-map TimeQuest pin names
- asynchronous CDC grouping between the independently controlled clock domains used by control, reference/gate, internal streamer, and clean/external streamer logic
- explicit false-path treatment for board/user/HPS peripheral ports that do not have an FPGA-owned external timing contract
- exclusion of mux-select and PLL-reconfiguration control paths from ordinary timing analysis

The async clock groups are timing exceptions only. They tell TimeQuest not to enforce a synchronous setup/hold relationship across those clocks; they do not make the crossing safe. RTL must still use an appropriate CDC structure or a documented software-stable protocol.

For block-level latency and CDC visibility behavior, see [RTL latency and timing](latency.md).

Current examples covered by those groups include:

- dual-clock streamer FIFOs and reset/control crossings
- counter latch-then-read paths between `d_clk` snapshots and Avalon readout
- frequency-meter Gray-counter synchronizers
- combiner software configuration feeding registered streamer-domain output logic

### Specific SDC observations

| Item | Current state |
| ---- | ------------- |
| external clock assumption | `EXT_CLKp` is constrained as a 100 ns / 10 MHz clock matching the external-cleaning PLL configuration |
| raw external mux case | not modeled by the active `SELECT_CLK_CLEAN` generated-clock definitions |
| cleaned external mux case | modeled as `STREAMER_FROM_CLEAN` from the external-cleaning PLL output |
| mux exclusivity | internal and external streamer sources are declared logically exclusive |
| CDC clock grouping | core/control, reference/gate, internal-streamer, and clean/external-streamer clock domains are declared asynchronous |
| board/user/HPS peripheral ports | external port timing is excepted; internal post-buffer and pre-output-buffer logic remains timed |
| control-path timing | clock-select and PLL-reconfiguration paths are marked false |

If the top-level clock tree changes, `pulsepins.sdc` must be reviewed together with the RTL.

Changes that trigger SDC review:

- clock mux structure changes
- external-clock cleaning path changes
- PLL or mux instance renaming/movement
- different external-clock target frequency assumptions

## Clocking review checklist

- identify the owning clock domain for every modified register
- check whether the path is synchronous logic or intentional CDC
- check reset assertion and release per domain
- check `streamer_clk` behavior under both internal and external source selection
- check whether software timing uses measured `streamer_clk` or a nominal assumption
- check whether `pulsepins.sdc` still matches the top-level clock tree
- run `make timing-check` after a Quartus build and resolve reported SDC, unconstrained-path, or negative-slack failures

## Summary table

| Clock | Short description |
| ----- | ----------------- |
| `core_clk` | main control/system clock |
| `int_clk` | internal candidate streaming clock |
| `streamer_clk` | active output/streaming clock |
| `ref_clk` | 50 MHz board reference clock |
| `ext_clk` / `EXT_CLKp` | external candidate streaming source |

Most timing-sensitive PulsePins behavior follows `streamer_clk`.
