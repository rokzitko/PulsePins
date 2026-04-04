// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Purpose: sequence-histogram counter directed testbench.
//
// Verifies histogram updates for short bit-pattern windows on a selected signal.

// Test: non-overlapping (rolling=0)

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

seq_counter #(
 .length(3),
 .width_addr(3),
 .rolling(0)
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
  $display("t=%t reset=%b d=%b valid=%b l=%d w=%b wr=%b ctr[0]=%d ctr_r[0]=%d ctr[7]=%d result=%d",
    $time, reset, d, valid, dut.l, dut.w, dut.wr, dut.ctr[0], dut.ctr_r[0], dut.ctr[7], result);
end

parameter LEN = 3*8;

task automatic write_serial;
  input logic [0:LEN-1] data; // bit vector to send (MSB first)
  integer i;
  begin
    @(posedge clk);
    valid <= 1'b1;
    for (i = 0; i < LEN; i++) begin
      d <= data[i];
      @(posedge clk);
    end
    valid <= 1'b0;
    d <= 1'bX;
  end
endtask

task automatic check;
  input logic [LEN-1:0] v;
  input logic [31:0] expected;
  begin
    addr <= v;
    #2;
    assert(result == expected) else begin
      $display("Mismatch: addr=%d result=%d expected=%d", v, result, expected);
      $fatal;
    end
  end
endtask

logic [0:LEN-1] data;

initial begin
  reset <= 1;
  d <= 'bX;
  valid <= 0;
  latch <= 0;
  high_low <= 0;
  addr <= 0;
  data <= 'b000_001_010_011_100_101_110_111;
  #2;
  reset <= 0;
  #1;
  write_serial(data);
  #5;
  latch <= 1;
  #1;
  latch <= 0;
  #2;

  check(3'b000, 1);
  check(3'b001, 1);
  check(3'b010, 1);
  check(3'b011, 1);
  check(3'b100, 1);
  check(3'b101, 1);
  check(3'b110, 1);
  check(3'b111, 1);
  $display("series 1 ok");

  #1;
  data <= 'b000_001_010_011_100_101_110_111;
  #1;
  write_serial(data);
  #2;
  latch <= 1;
  #1;
  latch <= 0;
  #2;

  check(3'b000, 2);
  check(3'b001, 2);
  check(3'b010, 2);
  check(3'b011, 2);
  check(3'b100, 2);
  check(3'b101, 2);
  check(3'b110, 2);
  check(3'b111, 2);
  $display("series 2 ok");

  #1;
  data <= 'b000_001_010_011_100_101_110_000;
  #1;
  write_serial(data);
  #2;
  latch <= 1;
  #1;
  latch <= 0;
  #2;

  check(3'b000, 4);
  check(3'b001, 3);
  check(3'b010, 3);
  check(3'b011, 3);
  check(3'b100, 3);
  check(3'b101, 3);
  check(3'b110, 3);
  check(3'b111, 2);
  $display("series 3 ok");

  #2 $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000 $finish;
end

endmodule: hello_world
