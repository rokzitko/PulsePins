// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`default_nettype none // turn off implicit data types

module basic_counter
#(
  parameter width_addr = 3,
  parameter width_bus = 32,
  parameter width_ctr = 64
)(
  input wire clk,
  input wire d_clk,
  input wire reset,
  input wire d,
  input wire valid,
  input wire latch,
  input wire high_low,
  input wire [width_addr-1:0] addr,
  output reg [width_bus-1:0] result,
  output reg overflow
);

initial assert (2*width_bus <= width_ctr)
  else $error("width_bus too large for width_ctr");

// Counters: total, high, low, high-to-low transitions, low-to-high transitions
logic [width_ctr-1:0] ctr_total, ctr_l, ctr_h, ctr_lh, ctr_hl;
logic [width_ctr-1:0] ctr_total_r, ctr_l_r, ctr_h_r, ctr_lh_r, ctr_hl_r;
logic first;
logic d_prev;

always_ff @(posedge d_clk) begin
  if (reset) begin
    ctr_total <= 0;
    ctr_l <= 0;
    ctr_h <= 0;
    ctr_lh <= 0;
    ctr_hl <= 0;
    first <= 1'b1;
    d_prev <= 0;
  end else if (valid) begin
    if (ctr_total != {width_ctr{1'b1}}) // saturate counter (overflow will also be asserted)
      ctr_total <= ctr_total+1;
    if (!d) ctr_l <= ctr_l+1;
    if (d) ctr_h <= ctr_h+1;
    if (!first && d && !d_prev) ctr_lh <= ctr_lh+1;
    if (!first && !d && d_prev) ctr_hl <= ctr_hl+1;
    first <= 1'b0;
    d_prev <= d;
  end
end

always_ff @(posedge d_clk) begin
  if (reset)
    overflow <= 0;
  else if (valid && ctr_total == {width_ctr{1'b1}}) 
    overflow <= 1;
end

always_ff @(posedge d_clk) begin
  if (reset) begin
    ctr_total_r <= 0;
    ctr_l_r <= 0;
    ctr_h_r <= 0;
    ctr_hl_r <= 0;
    ctr_lh_r <= 0;
  end else if (latch) begin
    ctr_total_r <= ctr_total;
    ctr_l_r <= ctr_l;
    ctr_h_r <= ctr_h;
    ctr_hl_r <= ctr_hl;
    ctr_lh_r <= ctr_lh;
  end
end

logic [width_ctr-1:0] sel;
always_comb begin
  case (addr)
    3'd0: sel = ctr_total_r;
    3'd2: sel = ctr_l_r;
    3'd3: sel = ctr_h_r;
    3'd4: sel = ctr_lh_r;
    3'd5: sel = ctr_hl_r;
    default: sel = '0;
  endcase
end

always_ff @(posedge clk)
  result <= high_low ? sel[2*width_bus-1:width_bus] : sel[width_bus-1:0];

endmodule
