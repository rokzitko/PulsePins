// Observe actual sampling/flush/split events in the encoder's selected clock.
`include "../../testbench/scenario_coverage.svh"
int cov_samples = 0, cov_blocks = 0, cov_flushes = 0, cov_encoded = 0, cov_splits = 0;
int cov_overflows = 0;
logic [1:0] cov_modes = 0;
bit cov_previous_valid = 0, cov_previous_overflow = 0;
always @(posedge dut.input_clk) begin
  if (dut.reset !== 1'b0 || dut.reset_iclk !== 1'b0) begin
    cov_previous_valid = 0; cov_previous_overflow = 0;
  end else begin
    if (dut.sample_event) begin
      cov_samples++;
      cov_modes[dut.mode] = 1;
      if (!cov_previous_valid) cov_blocks++;
      if (dut.have_run && dut.data == dut.run_value && dut.run_count == dut.MAX_COUNT) cov_splits++;
    end
    if (!dut.valid && dut.have_run) cov_flushes++;
    if (dut.wrreq) cov_encoded++;
    if (dut.overflow && !cov_previous_overflow) cov_overflows++;
    cov_previous_valid = dut.valid;
    cov_previous_overflow = dut.overflow;
  end
end
