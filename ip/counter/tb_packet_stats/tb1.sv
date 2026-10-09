// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Purpose: packet-statistics counter testbench.
//
// Verifies valid/idle accounting and packet-length statistics for a stream with packet-style
// validity.

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
  .clk_reset(reset),
  .d_clk(clk),
  .d_cdc_reset(reset),
  .reset,
  .valid,
  .latch,
  .high_low,
  .addr,
  .result,
  .overflow
);

wire cov_input_bit = 1'b0, cov_latch = latch;
`include "../sample_coverage.svh"

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
  #12;

  addr <= 0;
  #1;
  assert(result == 10) cov_read_addresses[addr] = 1; else $fatal;

  addr <= 3'b001; // ctr_valid
  #1;
  assert(result == 5) cov_read_addresses[addr] = 1; else $fatal;

  addr <= 3'b010; // ctr_idle
  #1;
  assert(result == 5) cov_read_addresses[addr] = 1; else $fatal;

  addr <= 3'b011; // ctr_pkt_begin
  #1;
  assert(result == 2) cov_read_addresses[addr] = 1; else $fatal;

  addr <= 3'b100; // ctr_pkt_end
  #1;
  assert(result == 2) cov_read_addresses[addr] = 1; else $fatal;

  addr <= 3'b101; // ctr_pkt_len_sum
  #1;
  assert(result == 5) cov_read_addresses[addr] = 1; else $fatal;

  addr <= 3'b110; // ctr_pkt_len_sum2
  #1;
  assert(result == 13) cov_read_addresses[addr] = 1; else $fatal;

  tb_require_scenario("valid_cycles", cov_samples, 5);
  tb_require_scenario("idle_cycles", cov_idle, 5);
  tb_require_scenario("packet_starts", cov_begins, 2);
  tb_require_scenario("packet_ends", cov_ends, 2);
  tb_require_scenario("latched_statistics", cov_latches);
  tb_require_scenario("statistics_checked", $countones(cov_read_addresses), 7);
  #20 $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000 $fatal(1, "timeout waiting for packet statistics checks");
end

endmodule: hello_world
