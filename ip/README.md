Verilog description of the FPGA circuitry.

## Main subsystems

- `streamer` - primary pulse-sequence engine; accepts encoded sequence elements, applies trigger/gating policy, and produces the final `qout` stream
- `rl_encoder_if` - run-length encoder used for readback/testing and for logic-analyzer-style capture workflows
- `counter` - event counter and statistics subsystem for streamer verification and external-signal measurement
- `ts_core` - timestamp capture path for trigger logging and synchronization workflows
- `freq_meter` - frequency-measurement logic
- `st_mux` - simple Avalon-ST multiplexer implemented in `st_mux_if.sv`
- `combiner` - advanced multiplexer/postprocessor for multiple streamers (pipeline version)
- `combiner_comb` - purely combinational version of the combiner
- `combiner_trig` - multiplexer for trigger signals
- `misc` - small reusable support blocks used across the design

## Maintainer reading order

For the main output-generation path, start with:

1. `streamer/README.md`
2. `streamer/st_interface.sv`
3. `streamer/streamer.sv`
4. `streamer/config.vh`
5. `rl_encoder_if/rl_encoder_if.sv`
6. `counter/counter_if.sv`

That path covers the core programming model, verification path, and measurement side channels used most often during development.

## Verification and related docs

- test benches use ModelSim or Icarus
- main command: `make -C ip test` (integrated HDL regression target; some local TB directories still require direct invocation)
- testbench tree overview: `TESTBENCHES.md`
- web docs entry points:
  - `docs/docs/streamer.md`
  - `docs/docs/readback.md`
  - `docs/docs/counter.md`
  - `docs/docs/st_mux.md`
  - `docs/docs/clock_domain.md`
  - `docs/docs/timestamp.md`
  - `docs/docs/freq_meter.md`
