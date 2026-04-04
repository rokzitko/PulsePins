// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Purpose: additional autocorrelation counter scenario.
//
// Extends lag-based self-correlation coverage beyond the primary directed case.

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`default_nettype none

module hello_world;

reg reset;
reg clk;

initial clk = 0;
always #0.5 clk = ~clk;

reg d, valid;
reg latch;
reg high_low;
reg [2:0] addr;
wire [31:0] result;

localparam int length = 5;

autocorrelation #(
 .length(length),
 .width_addr(3)
) dut (
  .clk(clk),
  .d_clk(clk),
  .reset,
  .d,
  .valid,
  .latch,
  .high_low,
  .addr,
  .result
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b d=%b valid=%b array=%b ctr=%4d acc[1]=%4d acc[2]=%4d acc[3]=%4d acc[4]=%4d acc[5]=%4d",
    $time, reset, d, valid, dut.array, dut.ctr, dut.acc[1], dut.acc[2], dut.acc[3], dut.acc[4], dut.acc[5]);
end


// generate autocorrelated random bits
logic rand_bit;
always_ff @(posedge clk) begin
  rand_bit <= ($urandom_range(0, 100) > 30 ? $urandom_range(0, 1) : rand_bit);
end

assign d = rand_bit;

logic [31:0] ctr, c1, c2;
real x;

initial begin
  reset <= 1;
  valid <= 0;
  latch <= 0;
  high_low <= 0;
  addr <= 0;
  #2;
  reset <= 0;
  #1;
  valid <= 1;
  #100000;
  valid <= 0;
  #1;
  latch <= 1;
  #1;
  latch <= 0;
  #2;

  addr <= 0;
  #1;
  ctr <= result;
  #1;

  addr <= 1;
  #1;
  c1 <= result;
  #1;
  x = $itor(c1)/($itor(ctr)-1.0);
  $display("c1=%d ctr=%d x=%f", c1, ctr, x);

  addr <= 2;
  #1;
  c2 <= result;
  #1;



  #2 $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000000 $finish;
end

endmodule: hello_world
