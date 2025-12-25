// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`default_nettype none

module hello_world;

reg reset;
reg clk;

initial clk = 0;
always #0.5 clk = ~clk;

reg valid;
reg latch;
reg high_low;
reg [2:0] addr;
wire [31:0] result;
wire overflow;

packet_stats dut (
  .clk(clk),
  .d_clk(clk),
  .reset,
  .valid,
  .latch,
  .high_low,
  .addr,
  .result,
  .overflow
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b valid=%b ctr_total=%d ctr_total_r=%d result=%d",
    $time, reset, valid, dut.ctr_total, dut.ctr_total_r, result);
end

initial begin
  reset <= 1;
  valid <= 0;
  latch <= 0;
  high_low <= 0;
  addr <= 0;
  #2;
  reset <= 0;
  // 0011001110
  // ctr_tot=10
  valid <= 0;
  #2;
  valid <= 1;
  #2;
  valid <= 0;
  #2;
  valid <= 1;
  #3
  valid <= 0;
  #1;
  latch <= 1;
  #1;
  latch <= 0;
  #1;

  addr <= 0;
  #1;
  assert(result == 10) else $fatal;

  addr <= 3'b001; // ctr_valid
  #1;
  assert(result == 5) else $fatal;

  addr <= 3'b010; // ctr_idle
  #1;
  assert(result == 5) else $fatal;

  addr <= 3'b011; // ctr_pkt_begin
  #1;
  assert(result == 2) else $fatal;

  addr <= 3'b100; // ctr_pkt_end
  #1;
  assert(result == 2) else $fatal;

  addr <= 3'b101; // ctr_pkt_len_sum
  #1;
  assert(result == 5) else $fatal;

  addr <= 3'b110; // ctr_pkt_len_sum2
  #1;
  assert(result == 13) else $fatal;

  #20 $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000 $finish;
end

endmodule: hello_world
