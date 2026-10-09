// Count selections only while the bench supplies a non-idle data vector.
`include "../../testbench/scenario_coverage.svh"
logic [15:0] cov_modes = 0;
always @(negedge clk) if (!reset && (|in1 || |in2 || |in3 || |in4)) cov_modes[cov_mode] = 1;
