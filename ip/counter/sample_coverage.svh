// Coverage of sampled data and snapshot requests for the directed counter benches.
// cov_input_bit/cov_latch are connected by the including bench.
int cov_samples = 0, cov_low = 0, cov_high = 0, cov_rises = 0, cov_falls = 0;
int cov_idle = 0, cov_begins = 0, cov_ends = 0, cov_latches = 0;
int cov_read_checks = 0;
logic [15:0] cov_read_addresses = 0;
bit cov_previous = 0, cov_previous_valid = 0, cov_have_sample = 0;
always @(posedge clk) begin
  if (reset) begin
    cov_previous_valid = 0; cov_have_sample = 0;
  end else begin
    if (valid) begin
      cov_samples++;
      if (cov_input_bit) cov_high++; else cov_low++;
      if (cov_have_sample && cov_input_bit && !cov_previous) cov_rises++;
      if (cov_have_sample && !cov_input_bit && cov_previous) cov_falls++;
      cov_previous = cov_input_bit; cov_have_sample = 1;
    end else cov_idle++;
    if (valid && !cov_previous_valid) cov_begins++;
    if (!valid && cov_previous_valid) cov_ends++;
    cov_previous_valid = valid;
    if (cov_latch && cov_samples > 0) cov_latches++;
  end
end
`include "../testbench/scenario_coverage.svh"
