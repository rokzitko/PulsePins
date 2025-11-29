Verilog description of FPGA circuitry.

* streamer - pulse sequences code
* rl_encoder_if - RL encoder (for testing the streamre or for using the board as a logic analyser)
* counter - event counter (for testing the streamer or for measuring statistical properties of external signals)
* ts_core - time-stamping logic (for recording trigger event or for synchronizing clocks)
* st_mux - simple Avalon ST bus multiplexer
* combiner - multiplexer and postprocessor for multiple streamers (pipeline version)
* combiner_comb - purely combinational version of the above (no wait states); provided for compleness
* combiner_trig - multiplexer for trigger signals

Test benches use ModelSim or Icarus.
