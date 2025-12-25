// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

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
wire overflow;

basic_counter dut (
  .clk(clk),
  .d_clk(clk),
  .reset,
  .d,
  .valid,
  .latch,
  .high_low,
  .addr,
  .result,
  .overflow
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b d=%b valid=%b ctr_total=%d ctr_total_r=%d result=%d",
    $time, reset, d, valid, dut.ctr_total, dut.ctr_total_r, result);
end

initial begin
  reset <= 1;
  d <= 0;
  valid <= 0;
  latch <= 0;
  high_low <= 0;
  addr <= 0;
  #2;
  reset <= 0;
  #1;
  // 010101100
  // total=9, l=5, h=4, lh=3, hl=3
  valid <= 1;
  d <= 0;
  #1;
  d <= 1;
  #1;
  d <= 0;
  #1;
  d <= 1;
  #1;
  d <= 0;
  #1;
  d <= 1;
  #1;
  #1;
  d <= 0;
  #1;
  #1;
  valid <= 0;
  latch <= 1;
  #1;
  latch <= 0;
  #1;

  addr <= 0;
  #1;
  assert(result == 9) else $fatal;

  addr <= 3'b010;
  #1;
  assert(result == 5) else $fatal;

  addr <= 3'b011;
  #1;
  assert(result == 4) else $fatal;

  addr <= 3'b100;
  #1;
  assert(result == 3) else $fatal;

  addr <= 3'b101;
  #1;
  assert(result == 3) else $fatal;

  #20 $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000 $finish;
end

endmodule: hello_world
