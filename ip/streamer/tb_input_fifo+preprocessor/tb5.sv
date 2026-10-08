// Purpose: backpressure test for the combined FIFO + preprocessor path.
//
// Verifies that replay/storage behavior remains correct even when downstream readiness applies
// pressure to the integrated ingress path.
// Checks store/replay followed by randomized traffic against an expected-output queue;
// success requires upstream backpressure, complete delivery, and both FIFOs drained.
// Test backpressure in preprocessor
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb5_input;

`include "defines.sv"

logic clk;
logic reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

// Interface
logic [WIDTH_TOTAL-1:0] data;
logic wrreq;
logic rdreq;

logic [WIDTH_TOTAL-1:0] q;
logic full;
logic empty;

bit verbose = 1;

always @(posedge clk) begin
  if (verbose) $strobe("t=%8.3f reset=%b wrreq=%b rdreq=%b q=%h empty=%b full=%b used1=%d dout_valid=%b used2=%d | i=%d j=%d",
    $realtime, reset, wrreq, rdreq, q, empty, full, dut.used1, dut.dout_valid, dut.used2,
    dut.proc.i, dut.proc.j
    );
end

input_fifo #(
.p1(5),  // 32 words, leaving capacity beyond the 16-word almost-full margin
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

logic [WIDTH_TOTAL-1:0] expected_queue[$];
int expected_count = 0;
int received_count = 0;
int blocked_cycles = 0;
bit producer_done = 0;
bit reader_done = 0;
bit random_phase_done = 0;

logic want_write;
assign wrreq = want_write & ~full;  // actual FIFO wrreq

task automatic send(input logic [WIDTH_TOTAL-1:0] word);
  begin
    @(negedge clk);
    data = word;
    want_write = 1;
    do @(posedge clk); while (!wrreq);
    @(negedge clk);
    want_write = 0;
  end
endtask

task automatic expect_word(input logic [WIDTH_TOTAL-1:0] word);
  expected_queue.push_back(word);
  expected_count++;
endtask

// Compare show-ahead data on the accepting edge, before the FIFO advances.
logic [WIDTH_TOTAL-1:0] expected_word;
always @(posedge clk) begin
  if (!reset) begin
    if (want_write && full) blocked_cycles++;
    if (rdreq && !empty) begin
      assert(expected_queue.size() != 0) else $fatal(1, "unexpected output %h", q);
      expected_word = expected_queue.pop_front();
      assert(q === expected_word) else $fatal(1, "output mismatch at word %0d: got %h expected %h", received_count, q, expected_word);
      received_count++;
    end
  end
end

parameter int loops = 100;

task automatic producer;
integer i, j, nr;
logic [WIDTH_TOTAL-1:0] word;
begin
  send({ STORE0, 32'd10, 32'd1 });
  send({ STORE1, 32'd10, 32'd2 });
  send({ STORE2, 32'd10, 32'd3 });
  send({ STORE3, 32'd10, 32'd4 });
  for (i = 0; i < 8; i++) begin
    expect_word({ STORE0, 32'd10, 32'd1 });
    expect_word({ STORE1, 32'd10, 32'd2 });
    expect_word({ STORE2, 32'd10, 32'd3 });
    expect_word({ STORE3, 32'd10, 32'd4 });
  end
  send({ REPLAY, 32'd8, 32'd4 });

  // Fill ingress while replay is stalled to guarantee upstream backpressure.
  for (i = 0; i < 40; i++) begin
    word = { PASS, 32'd1, 32'(i) };
    expect_word(word);
    send(word);
  end
  wait(received_count == expected_count);

  for (i = 0; i < loops; i = i + 1) begin
    nr = $urandom_range(0, 100);
    for (j = 0; j < nr; j++) begin
      word = { PASS, 32'd1, $urandom() };
      expect_word(word);
      send(word);
    end
    repeat ($urandom_range(10, 50)) @(negedge clk);
  end
  random_phase_done = 1;
  producer_done = 1;
end
endtask

task automatic consumer;
begin
  // The watchdog bounds this wait as well as the randomized reader below.
  wait(blocked_cycles >= 4);
  while (!producer_done || received_count != expected_count) begin
    @(negedge clk);
    rdreq = !empty && ($urandom_range(0, 3) != 0);
  end
  @(negedge clk);
  rdreq = 0;
  reader_done = 1;
end
endtask

integer fh;

initial begin
  data = '0;
  want_write = 0;
  rdreq = 0;
  #5;

  fork
    producer();
    consumer();
  join
end

initial begin
  wait(producer_done && reader_done);
  repeat (8) @(negedge clk);
  assert(random_phase_done && blocked_cycles >= 4) else $fatal(1, "required scenarios not completed");
  assert(expected_queue.size() == 0 && received_count == expected_count) else $fatal(1, "incomplete delivery");
  assert(empty && dut.rdempty && !dut.proc.active) else $fatal(1, "FIFO/preprocessor did not drain");
  assert(!dut.overflow_in && !dut.overflow_out) else $fatal(1, "FIFO overflow");
  $display("SUCCESS: checked %0d words, %0d blocked cycles", received_count, blocked_cycles);
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_input_pre_5.ucdb");
  $finish;
end

initial #100000 $fatal(1, "timeout: received=%0d expected=%0d producer=%b reader=%b",
  received_count, expected_count, producer_done, reader_done);

endmodule: tb5_input
