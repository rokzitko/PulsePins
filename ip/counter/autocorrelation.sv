// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`default_nettype none // turn off implicit data types

module autocorrelation
#(
  parameter length = 7,     // size of the shift register
  parameter width_addr = 3, // width of the address bus for readout, $clog2(length+1)
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
  output reg [width_bus-1:0] result
);

logic [width_ctr-1:0] ctr;              // total length counter
logic [width_ctr-1:0] ctr_r;            // registered version of ctr
logic [width_ctr-1:0] acc   [1:length]; // accumulator for x[i] x[i+tau]
logic [width_ctr-1:0] acc_r [1:length]; // registered version of acc
logic [length:0] array;                 // shift register

always_ff @(posedge d_clk) begin
  if (reset) begin
    ctr <= 0;
    array <= '0;
  end else if (valid) begin
    ctr <= ctr+1;
    array <= { array[length-1:0], d }; // add new elements from the LSB side and shift left
  end
end

integer i;
always_ff @(posedge d_clk) begin
  if (reset) begin
    for (i = 1; i <= length; i++)
      acc[i] <= '0;
  end else if (valid) begin
    for (i = 1; i <= length; i++)
      if (ctr >= i) acc[i] <= acc[i] + (d == array[i-1] ? 1 : 0); // array[0] contains *previous* element
  end
end

always_ff @(posedge d_clk) begin
  if (reset) begin
    for (i = 1; i <= length; i++)
      acc_r[i] <= '0;
    ctr_r <= '0;
  end else if (latch) begin
    for (i = 1; i <= length; i++)
      acc_r[i] <= acc[i];
    ctr_r <= ctr;
  end
end

logic [width_ctr-1:0] v;

always_comb
  v = (addr == 0 ? ctr_r : acc_r[addr]);

always_ff @(posedge clk)
  result <= (high_low ? v[2*width_bus-1:width_bus] : v[width_bus-1:0]);

endmodule
