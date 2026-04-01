// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`default_nettype none // turn off implicit data types

// Run-length statistics for one selected binary channel.
//
// This block groups consecutive equal samples into runs and accumulates per-level
// summary statistics such as:
//   - total number of completed runs
//   - number of low/high runs
//   - sum of run lengths per level
//   - maximum run length per level
//   - number of short runs classified as glitches
//
// The input is sampled in `d_clk`, and software reads a latched snapshot through
// `counter_if.sv` after pulsing the shared latch control.

module runs_counter
#(
  parameter width_addr = 4,
  parameter width_bus = 32,
  parameter width_ctr = 64,
  parameter glitch_length = 1
)(
  input wire clk,
  input wire d_clk,
  input wire reset,
  input wire d,
  input wire valid,
  input wire latch,
  input wire high_low,
  input wire [width_addr-1:0] addr,
  output reg [width_bus-1:0] result
);

logic [width_ctr-1:0] ctr_run;                  // Number of completed runs of either level
logic [width_ctr-1:0] nr_run_l, nr_run_h;       // Number of runs of low/high level
logic [width_ctr-1:0] sum_run_l, sum_run_h;     // Sum of lengths of runs of low/high level
`ifdef DO_SUM2
  logic [width_ctr-1:0] sum2_run_l, sum2_run_h;   // Sum squared of lengths of runs of low/high level (liable to overflow)
`endif
logic [width_ctr-1:0] max_run_l, max_run_h;     // Longest low/high run
logic [width_ctr-1:0] nr_glitch_l, nr_glitch_h; // Number of low/high glitches
logic [width_ctr-1:0] ctr_run_r, nr_run_l_r, nr_run_h_r, sum_run_l_r, sum_run_h_r,
                      sum2_run_l_r, sum2_run_h_r, max_run_l_r, max_run_h_r, nr_glitch_l_r, nr_glitch_h_r;

logic d_reg, valid_reg;
always_ff @(posedge d_clk) begin
  if (reset) begin
    d_reg <= 0;
    valid_reg <= 0;
  end else begin
    d_reg <= d;
    valid_reg <= valid;
  end
end

logic first;                   // Have we seen the first valid sample of the current burst?
logic d_prev;                  // Previous sampled value
logic valid_prev;              // Previous state of valid signal
logic [width_ctr-1:0] run_len; // Current run length in samples

always_ff @(posedge d_clk) begin
  if (reset) begin
    ctr_run <= 0;
    run_len <= 0;
    nr_run_l <= 0;
    nr_run_h <= 0;
    sum_run_l <= 0;
    sum_run_h <= 0;
`ifdef DO_SUM2
    sum2_run_l <= 0;
    sum2_run_h <= 0;
`endif
    max_run_l <= 0;
    max_run_h <= 0;
    nr_glitch_l <= 0;
    nr_glitch_h <= 0;
    first <= 1'b1;
    d_prev <= 0;
    valid_prev <= 0;
  end else begin
    if (valid_reg && first) begin
      // Start the first run of a new valid burst.
      run_len <= 1;
      first <= 0;
    end else
    if (valid_reg && !first && d_reg == d_prev) begin
      run_len <= run_len+1;
    end else
    if ((valid_reg && !first && d_reg != d_prev) || (!valid_reg && valid_prev)) begin
      // A run ends either when the sampled level changes or when the valid region ends.
      if (d_prev == 1'b0) begin
        nr_run_l <= nr_run_l + 1;
        sum_run_l <= sum_run_l + run_len;
`ifdef DO_SUM2
        sum2_run_l <= sum2_run_l + run_len*run_len;
`endif
        max_run_l <= (run_len > max_run_l ? run_len : max_run_l);
        if (run_len <= glitch_length)
          nr_glitch_l <= nr_glitch_l + 1;
      end else begin
        nr_run_h <= nr_run_h + 1;
        sum_run_h <= sum_run_h + run_len;
`ifdef DO_SUM2
        sum2_run_h <= sum2_run_h + run_len*run_len;
`endif
        max_run_h <= (run_len > max_run_h ? run_len : max_run_h);
        if (run_len <= glitch_length)
          nr_glitch_h <= nr_glitch_h + 1;
      end
      ctr_run <= ctr_run+1;
      if (valid_reg)
        run_len <= 1;
      else begin
        run_len <= 0;
        first <= 1'b1;
      end
    end
    d_prev     <= d_reg;
    valid_prev <= valid_reg;
  end
end

always_ff @(posedge d_clk) begin
  if (reset) begin
    ctr_run_r <= 0;
    nr_run_l_r <= 0;
    nr_run_h_r <= 0;
    sum_run_l_r <= 0;
    sum_run_h_r <= 0;
`ifdef DO_SUM2
    sum2_run_l_r <= 0;
    sum2_run_h_r <= 0;
`endif
    max_run_l_r <= 0;
    max_run_h_r <= 0;
    nr_glitch_l_r <= 0;
    nr_glitch_h_r <= 0;
  end else if (latch) begin
    // Freeze a coherent snapshot for software readout in the control clock domain.
    ctr_run_r <= ctr_run;
    nr_run_l_r <= nr_run_l;
    nr_run_h_r <= nr_run_h;
    sum_run_l_r   <= sum_run_l;
    sum_run_h_r   <= sum_run_h;
`ifdef DO_SUM2
    sum2_run_l_r  <= sum2_run_l;
    sum2_run_h_r  <= sum2_run_h;
`endif
    max_run_l_r   <= max_run_l;
    max_run_h_r   <= max_run_h;
    nr_glitch_l_r <= nr_glitch_l;
    nr_glitch_h_r <= nr_glitch_h;
  end
end

logic [width_ctr-1:0] v;
always_comb begin
  case (addr)
    'd0:  v = ctr_run_r;
    'd2:  v = nr_run_l_r;
    'd3:  v = nr_run_h_r;
    'd4:  v = sum_run_l_r;
    'd5:  v = sum_run_h_r;
    'd6:  v = max_run_l_r;
    'd7:  v = max_run_h_r;
    'd8:  v = nr_glitch_l_r;
    'd9:  v = nr_glitch_h_r;
`ifdef DO_SUM2
    'd12: v = sum2_run_l_r;
    'd13: v = sum2_run_h_r;
`endif
    default: v = '0;
  endcase
end

always_ff @(posedge clk)
  result <= high_low ? v[2*width_bus-1:width_bus] : v[width_bus-1:0];

endmodule
