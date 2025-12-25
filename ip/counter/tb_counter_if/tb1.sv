// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`default_nettype none

module hello_world;

reg reset;
reg clk;

initial clk = 0;
always #0.5 clk = ~clk;

reg [31:0] d;
reg valid;

reg [3:0] avs_s0_address;
reg avs_s0_read;
reg avs_s0_write;
wire [31:0] avs_s0_readdata;
reg [31:0] avs_s0_writedata;

counter_if dut (
  .clk,
  .reset,
  .d,
  .d_valid(valid),
  .d_clk(clk),
  .avs_s0_address,
  .avs_s0_read,
  .avs_s0_write,
  .avs_s0_readdata,
  .avs_s0_writedata
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b d=%4d valid=%b",
    $time, reset, d, valid);
end

task automatic check;
  input logic [31:0] instr;
  input logic [31:0] addr;
  input logic [31:0] expected;
  begin
    avs_s0_address <= 1; // instr
    avs_s0_writedata <= instr;
    avs_s0_write <= 1;
    #1;
    avs_s0_write <= 0;
    #1;
    assert(dut.instr == instr);

    avs_s0_address <= 3; // addr
    avs_s0_writedata <= addr;
    avs_s0_write <= 1;
    #1;
    avs_s0_write <= 0;
    #1;
    assert(dut.addr == addr);

    #1;
    assert(dut.result == expected) else begin
      $display("Mismatch: instr=%d addr=%d dut.result=%d expected=%d", instr, addr, dut.result, expected);
      $fatal;
    end

    avs_s0_address <= 0; // result
    avs_s0_read <= 1;
    #1;
    assert(avs_s0_readdata == expected) else begin
      $display("Mismatch: instr=%d addr=%d avs_s0_result=%d expected=%d", instr, addr, avs_s0_readdata, expected);
      $fatal;
    end
    avs_s0_read <= 0;
    #1;

  end
endtask

initial begin
  reset <= 1;
  d <= 0;
  valid <= 0;
  #2;
  reset <= 0;
  #3;
  // 010101100
  // total=9, l=5, h=4, lh=3, hl=3
  valid <= 1;
  d <= 0;
  #1;
  d <= 2'b11;
  #1;
  d <= 0;
  #1;
  d <= 2'b11;
  #1;
  d <= 0;
  #1;
  d <= 2'b11;
  #1;
  #1;
  d <= 0;
  #1;
  #1;
  valid <= 0;
  #1;
  assert(dut.bc.ctr_total == 9) else $fatal;
  assert(dut.bc.ctr_total == 9) else $fatal;

  // Check one result
  avs_s0_address <= 3'b111;
  avs_s0_writedata <= 2; // latch_all
  avs_s0_write <= 1;
  #1;
  assert(dut.latch_all == 1) else $fatal;

  #4;
  assert(dut.bc.latch == 1);

  avs_s0_writedata <= 0; // latch_all off
  avs_s0_write <= 1;
  #1;
  assert(dut.latch_all == 0) else $fatal;
  avs_s0_write <= 0;

  assert(dut.bc.ctr_total_r == 9);
  assert(dut.bc.ctr_total_r == 9);

  avs_s0_address <= 1; // instr
  avs_s0_writedata <= 1; // instr=1
  avs_s0_write <= 1;
  #1;
  avs_s0_write <= 0;
  #1;
  assert(dut.instr == 1) else $fatal;

  assert(dut.result == 9) else $fatal;

  avs_s0_address <= 0; // result
  avs_s0_read <= 1;
  #1;
  assert(avs_s0_readdata == 9) else $fatal;
  avs_s0_read <= 0;
  #1;

  // Use task 'check'
  check(1, 3'b000, 9); // instr=1, addr=0, expected=9
  check(1, 3'b010, 5); // 5 l
  check(1, 3'b011, 4); // 4 h
  check(1, 3'b100, 3); // 3 lh
  check(1, 3'b101, 3); // 3 hl

  // Runs
  check(2, 4'b0000, 7); // 7 run
  check(2, 4'b0010, 4); // 4 runs l
  check(2, 4'b0011, 3); // 3 runs h
  check(2, 4'b0100, 5); // sum_run_l=5
  check(2, 4'b0101, 4); // sum_run_h=4
  check(2, 4'b1100, 7); // sum2_run_l=7
  check(2, 4'b1101, 6); // sum2_run_h=6
  check(2, 4'b0110, 2); // max_run_l=2
  check(2, 4'b0111, 2); // max_run_h=2
  check(2, 4'b1000, 3); // nr_glitch_l=3
  check(2, 4'b1001, 2); // nr_glitch_h=2

  // Seq
  check(3, 4'b0101, 1);
  check(3, 4'b0110, 1);
  check(3, 4'b0000, 0);
  check(3, 4'b1111, 0);
  check(3, 4'b0011, 0);
  check(3, 4'b1100, 0);

  // Packet statistics
  check(5, 0, 18); // total
  check(5, 1, 9); // valid
  check(5, 2, 9); // idle
  check(5, 3, 1); // begin
  check(5, 4, 1); // end
  check(5, 5, 9); // len_sum
  check(5, 6, 81); // len_sum2

  // Autocorrelation deep
  avs_s0_address <= 4; // sel0
  avs_s0_writedata <= 1; // sel0=1
  avs_s0_write <= 1;
  #1;
  avs_s0_write <= 0;
  #1;
  assert(dut.sel0 == 1) else $fatal;
  check(6, 0, 9);
  check(6, 1, 2);
  check(6, 2, 4);

  // Crosscorrelation deep
  avs_s0_address <= 5; // sel1
  avs_s0_writedata <= 1; // sel1=1
  avs_s0_write <= 1;
  #1;
  avs_s0_write <= 0;
  #1;
  assert(dut.sel1 == 1) else $fatal;
  avs_s0_address <= 6; // sel2
  avs_s0_writedata <= 0; // sel2=0
  avs_s0_write <= 1;
  #1;
  avs_s0_write <= 0;
  #1;
  assert(dut.sel2 == 0) else $fatal;
  check(7, 0, 9); // because d1=d2, crosscorrelation the same as autocorrelation
  check(7, 1, 2);
  check(7, 2, 4);

  #20 $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000 $finish;
end

endmodule: hello_world
