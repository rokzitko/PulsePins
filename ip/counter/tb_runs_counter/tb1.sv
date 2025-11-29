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
reg [3:0] addr;
wire [31:0] result;

runs_counter dut (
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
  $display("t=%t reset=%b d=%b valid=%b ctr_run=%d ctr_run_r=%d result=%d",
    $time, reset, d, valid, dut.ctr_run, dut.ctr_run_r, result);
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
  // ctr_run=7, nr_run_l=4, nr_run_h=3, sum_run_l=5, sum_run_h=4,
  // max_run_l=2, max_run_h=2, nr_glitch_l=3, nr_glitch_h=2
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
  #10; // at least one period!
  latch <= 1;
  #1;
  latch <= 0;
  #1

  addr <= 4'b0000;
  #1;
  assert(result == 7) else $fatal;

  addr <= 4'b0010;
  #1;
  assert(result == 4) else $fatal;

  addr <= 4'b0011;
  #1;
  assert(result == 3) else $fatal;

  addr <= 4'b0100;
  #1;
  assert(result == 5) else $fatal;

  addr <= 4'b0101;
  #1;
  assert(result == 4) else $fatal;

  addr <= 4'b0110;
  #1;
  assert(result == 2) else $fatal;

  addr <= 4'b0111;
  #1;
  assert(result == 2) else $fatal;

  addr <= 4'b1000;
  #1;
  assert(result == 3) else $fatal;

  addr <= 4'b1001;
  #1;
  assert(result == 2) else $fatal;

  #20 $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000 $finish;
end

endmodule: hello_world
