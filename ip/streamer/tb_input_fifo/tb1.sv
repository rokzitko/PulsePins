// Purpose: basic directed ingress FIFO test.
//
// Checks that a short sequence of input words can be enqueued and later drained in order from
// the input FIFO, establishing the basic pass-through contract.
// Simple simulation test bench
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb1_input;

logic clk;
logic reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #1 reset <= 0;
end

// Interface
logic [WIDTH_TOTAL-1:0] data;
logic wrreq;
logic rdreq;

logic [WIDTH_TOTAL-1:0] q;
logic full;
logic empty;

always @(posedge clk) begin
  $strobe("t=%8.3f reset=%b wrreq=%b rdreq=%b q=%h empty=%b used1=%d used2=%d", $realtime, reset, wrreq, rdreq, q, empty, dut.used1, dut.used2);
end

input_fifo dut(
.clk,
.reset,

.data,
.wrreq,
.full,

.rdreq,
.q,
.empty
);

initial begin
  data <= 96'b0;
  wrreq <= 0;
  rdreq <= 0;
  #4
  wrreq <= 1;
  data <= 96'h01;
  #1
  data <= 96'h02;
  #1
  data <= 96'h03;
  #1
  data <= 96'h04;
  #1
  data <= 96'h05;
  #1
  wrreq <= 0;

  #10
  rdreq <= 1;
  $display("rdreq asserted at t=%8.3f", $realtime);
  #3 // Three periods
  rdreq <= 0;

  #3
  rdreq <= 1;
  $display("rdreq asserted at t=%8.3f", $realtime);
  #2 // Two periods
  rdreq <= 0;
end

initial begin
  #8
  // use #1step to perform the test after the non-blocking assignments
  #1step assert(q == 1) else $fatal; // show ahead

  @(posedge rdreq);
  @(posedge clk); // next clock tick
  #1step assert(q == 2) else $fatal; // advanced from 1 to 2

  @(posedge rdreq);
  @(posedge clk); // next clock tick
  #1step assert(q == 5) else $fatal; // advanced from 4 to 5
end

integer fh;

initial begin
  #30 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_input_1.ucdb");
  $finish;
end

endmodule: tb1_input
