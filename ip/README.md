Verilog description of the FPGA circuitry.

* streamer - pulse-sequence engine
* rl_encoder_if - RL encoder (for testing the streamer or for using the board as a logic analyzer)
* counter - event counter (for testing the streamer or for measuring statistical properties of external signals)
* ts_core - time-stamping logic (for recording trigger events or for synchronizing clocks)
* freq_meter - frequency-measurement logic
* st_mux - simple Avalon-ST bus multiplexer (implemented in `st_mux_if.sv`)
* combiner - multiplexer and postprocessor for multiple streamers (pipeline version)
* combiner_comb - purely combinational version of the above (no wait states); provided for completeness
* combiner_trig - multiplexer for trigger signals
* misc - small reusable support blocks used across the design

Test benches use ModelSim or Icarus.
