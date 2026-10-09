// Purpose: ingress FIFO backpressure test.
//
// Exercises the FIFO under sustained writes/reads so the ready/full behavior and delayed drain
// timing stay correct under pressure.
// Test backpressure
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb2_input;

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
int written = 0, received = 0, stalled_writes = 0, resumed_writes = 0, checked_words = 0;
always @(posedge clk) if (!reset) begin
  if (wrreq && full) stalled_writes++;
  if (wrreq && !full) begin
    written++;
    if (stalled_writes > 0) resumed_writes++;
  end
  if (rdreq && !empty) received++;
end

always @(posedge clk) begin
  $strobe("t=%8.3f reset=%b wrreq=%b rdreq=%b q=%d empty=%b full=%b used1=%d dout_valid=%b used2=%d",
    $realtime, reset, wrreq, rdreq, q, empty, full, dut.used1, dut.dout_valid, dut.used2);
end

input_fifo #(
.p1(5),  // smaller buffer, size=32!
.p2(5)
)
dut(
.clk,
.reset,

.data,
.wrreq,
.full,

.rdreq,
.q,
.empty
);

task automatic feed;
input int nr;
integer i;
begin
  for (i = 1; i <= nr; i = i+1) begin
    @(negedge clk);
    data = WIDTH_TOTAL'(i);
    wrreq = 1;
    do @(posedge clk); while (full);
  end
  @(negedge clk);
  wrreq = 0;
end
endtask

task automatic read;
input int nr;
integer j;
begin
  @(negedge clk);
  rdreq = 1;
  for (j = 1; j <= nr; j = j+1) begin
    do @(posedge clk); while (empty);
    assert(q == j) else $fatal;
    checked_words++;
  end
  @(negedge clk);
  rdreq = 0;
end
endtask

bit producer_done = 0, reader_done = 0;
initial begin
  data <= 96'b0;
  wrreq <= 0;
  rdreq <= 0;
  #4
  feed(10);
  #40
  feed(15);
  #10
  feed(40);
  producer_done = 1;
end

initial begin
  // Hold the reader until the real full indication blocks an offered write.
  wait(full === 1'b1);
  repeat (4) @(negedge clk);
  read(10);
  #20;
  read(15);
  #5;
  read(40);
  reader_done = 1;
end

integer fh;

initial begin
  wait(producer_done && reader_done);
  repeat (8) @(negedge clk);
  tb_require_scenario("accepted_writes", written, 65);
  tb_require_scenario("checked_words", checked_words, 65);
  tb_require_scenario("accepted_reads", received, 65);
  tb_require_scenario("write_backpressure", stalled_writes);
  tb_require_scenario("write_recovery", resumed_writes);
  tb_require_scenario("drained", empty === 1'b1);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_input_2.ucdb");
  $finish;
end

initial #2000 $fatal(1, "FIFO backpressure scenario did not complete");

`include "../../testbench/scenario_coverage.svh"
endmodule: tb2_input
