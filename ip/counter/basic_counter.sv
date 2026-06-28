// SPDX-License-Identifier: MIT
// Copyright (c) 2025,2026 Rok Zitko

// Basic single-channel statistics counter.
//
// This block samples one selected bit in the `d_clk` domain and accumulates the most
// fundamental activity metrics used throughout the counter subsystem:
//   - total number of valid samples
//   - number of low samples
//   - number of high samples
//   - low-to-high transitions
//   - high-to-low transitions
//
// `counter_if.sv` latches these working counters into readout registers before software
// reads them back through the shared Avalon-MM interface.

`default_nettype none // turn off implicit data types

module basic_counter
#(
  parameter width_addr = 3,
  parameter width_bus = 32,
  parameter width_ctr = 64
)(
  input wire clk,      // clock in system clock domain
  input wire clk_reset,
  input wire d_clk,    // data clock
  input wire d_cdc_reset,
  input wire d_reset,  // reset in d_clk clock domain
  input wire d,        // data
  input wire d_valid,  // data is sampled when d_valid is high
  input wire d_latch,  // when d_latch is asserted, the results are copied to the output registers
  input wire high_low, // upper or lower 32-bit part of the counter?
  input wire [width_addr-1:0] addr,
  output logic [width_bus-1:0] result,
  output wire overflow  // latches high if total count overflows (after 2^64 perios = 5850 years at 100MHz)
);

initial assert (2*width_bus <= width_ctr)
  else $error("width_bus too large for width_ctr");

// Working counters updated in the sampled-data clock domain.
logic [width_ctr-1:0] ctr_total, ctr_l, ctr_h, ctr_lh, ctr_hl;
// Snapshot registers in the control clock domain after a latch pulse.
logic [width_ctr-1:0] ctr_total_r, ctr_l_r, ctr_h_r, ctr_lh_r, ctr_hl_r;

logic first;  // first=1 after reset, before reading the first value
logic d_prev; // previous value

always_ff @(posedge d_clk) begin
  if (d_reset) begin
    ctr_total <= 0;
    ctr_l <= 0;
    ctr_h <= 0;
    ctr_lh <= 0;
    ctr_hl <= 0;
    first <= 1'b1;
    d_prev <= 0;
  end else if (d_valid) begin
    // Sampling is qualified by `d_valid` so the block can measure meaningful activity
    // rather than raw clock cycles in invalid/idle regions.
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

logic overflow_d;

always_ff @(posedge d_clk) begin
  if (d_reset)
    overflow_d <= 0;
  else if (d_valid && ctr_total == {width_ctr{1'b1}})
    overflow_d <= 1;
end

cdc_bit_sync overflow_sync (
  .dst_clk(clk),
  .dst_reset(clk_reset),
  .async_in(overflow_d),
  .sync_out(overflow)
);

localparam int SNAPSHOT_W = 5*width_ctr;
logic [SNAPSHOT_W-1:0] snapshot_d;
logic [SNAPSHOT_W-1:0] snapshot_clk;

assign snapshot_d = {ctr_hl, ctr_lh, ctr_h, ctr_l, ctr_total};
assign {ctr_hl_r, ctr_lh_r, ctr_h_r, ctr_l_r, ctr_total_r} = snapshot_clk;

cdc_bus_update #(
  .WIDTH(SNAPSHOT_W),
  .RESET_VALUE('0)
) snapshot_cdc (
  .src_clk(d_clk),
  .src_reset(d_cdc_reset),
  .src_data(snapshot_d),
  .src_update(d_latch),
  .src_busy(),
  .dst_clk(clk),
  .dst_reset(clk_reset),
  .dst_accept(1'b1),
  .dst_data(snapshot_clk),
  .dst_valid(),
  .dst_pending()
);

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

always_ff @(posedge clk) begin
  if (clk_reset)
    result <= '0;
  else
    result <= high_low ? sel[2*width_bus-1:width_bus] : sel[width_bus-1:0];
end

endmodule
