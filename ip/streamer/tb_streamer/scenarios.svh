// Observe runtime behavior in the DUT's playback clock domain. Each including
// bench requires only the bins belonging to its own scenario.
`include "../../testbench/scenario_coverage.svh"
int cov_payload = 0, cov_activations = 0, cov_completions = 0;
int cov_gated_cycles = 0, cov_retriggers = 0, cov_errors = 0, cov_quiet_cycles = 0;
bit cov_prev_active = 0, cov_prev_done = 0, cov_prev_error = 0;
always @(posedge dut.streamer_clk) begin
  if (dut.reset !== 1'b0 || dut.streamer_rst !== 1'b0) begin
    cov_prev_active = 0; cov_prev_done = 0; cov_prev_error = 0;
  end else begin
    if (dut.qout_valid) cov_payload++;
    if (dut.trigger_activated && !cov_prev_active) cov_activations++;
    if (dut.done && !cov_prev_done) cov_completions++;
    if (dut.buffer_error && !cov_prev_error) cov_errors++;
    if (dut.trigger_activated && !dut.gate_enable) cov_gated_cycles++;
    if (dut.retrig_requested) cov_retriggers++;
    if (!dut.trigger_activated && !dut.qout_valid && !dut.done) cov_quiet_cycles++;
    cov_prev_active = dut.trigger_activated;
    cov_prev_done = dut.done;
    cov_prev_error = dut.buffer_error;
  end
end
