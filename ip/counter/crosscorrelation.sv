// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`default_nettype none // turn off implicit data types

// Lag-based crosscorrelation counter for two selected channels.
//
// This block stores a short history of `d1` and compares the current `d2` sample against
// that history. For each lag `tau`, it counts how often `d2[i]` matches `d1[i-tau]`.
// Address 0 exposes the total number of valid samples, while higher addresses expose the
// per-lag match accumulators.
//
// Accumulation happens in `d_clk` and software reads a latched snapshot through
// `counter_if.sv`.

module crosscorrelation
#(
  parameter length = 8,                    // size of the shift register
  parameter width_addr = $clog2(length+1), // width of the address bus for readout
  parameter width_bus = 32,
  parameter width_ctr = 64
)(
  input wire clk,
  input wire clk_reset,
  input wire d_clk,
  input wire d_cdc_reset,
  input wire reset,
  input wire d1,
  input wire d2,
  input wire valid,
  input wire latch,
  input wire high_low,
  input wire [width_addr-1:0] addr,
  output logic [width_bus-1:0] result
);

logic [width_ctr-1:0] ctr;              // total number of valid samples
logic [width_ctr-1:0] ctr_r;            // registered version of ctr
logic [width_ctr-1:0] acc   [1:length]; // accumulator for x[i] y[i+tau]
logic [width_ctr-1:0] acc_r [1:length]; // registered version of acc
logic [length:0] array;                 // shift register

always_ff @(posedge d_clk) begin
  if (reset) begin
    ctr <= 0;
    array <= '0;
  end else if (valid) begin
    ctr <= ctr+1;
    array <= { array[length-1:0], d1 }; // shift in the newest `d1` sample from the LSB side
  end
end

integer i;
always_ff @(posedge d_clk) begin
  if (reset) begin
    for (i = 1; i <= length; i++)
      acc[i] <= '0;
  end else if (valid) begin
    // For lag `i`, compare the current `d2` sample with the `d1` sample from `i` valid cycles ago.
    for (i = 1; i <= length; i++)
      if (ctr >= i) acc[i] <= acc[i] + (d2 == array[i-1] ? 1 : 0); // array[0] contains *previous* element
  end
end

localparam int SNAPSHOT_W = (length+1)*width_ctr;
logic [SNAPSHOT_W-1:0] snapshot_d;
logic [SNAPSHOT_W-1:0] snapshot_clk;

integer pack_i;
always_comb begin
  snapshot_d[0 +: width_ctr] = ctr;
  ctr_r = snapshot_clk[0 +: width_ctr];
  for (pack_i = 1; pack_i <= length; pack_i++) begin
    snapshot_d[pack_i*width_ctr +: width_ctr] = acc[pack_i];
    acc_r[pack_i] = snapshot_clk[pack_i*width_ctr +: width_ctr];
  end
end

cdc_bus_update #(
  .WIDTH(SNAPSHOT_W),
  .RESET_VALUE('0)
) snapshot_cdc (
  .src_clk(d_clk),
  .src_reset(d_cdc_reset),
  .src_data(snapshot_d),
  .src_update(latch),
  .src_busy(),
  .dst_clk(clk),
  .dst_reset(clk_reset),
  .dst_accept(1'b1),
  .dst_data(snapshot_clk),
  .dst_valid(),
  .dst_pending()
);

initial assert (2*width_bus <= width_ctr)
  else $error("width_bus too large relative to width_ctr");

logic [width_ctr-1:0] v;
always_comb
  v = (addr == 0) ? ctr_r : acc_r[addr];

always_ff @(posedge clk) begin
  if (clk_reset)
    result <= '0;
  else
    result <= high_low ? v[2*width_bus-1:width_bus] : v[width_bus-1:0];
end

endmodule
