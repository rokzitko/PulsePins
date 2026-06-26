// Purpose: additional ingress FIFO backpressure scenario.
//
// Complements `tb2` with a second directed flow-control pattern so FIFO backpressure handling is
// checked under more than one timing relationship.
// Test backpressure
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb3_input;

logic clk;
logic reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset = 1;
  #1 reset = 0;
end

// Interface
logic [WIDTH_TOTAL-1:0] data;
logic wrreq;
logic rdreq;

logic [WIDTH_TOTAL-1:0] q;
logic full;
logic empty;
logic [WIDTH_STAT-1:0] ctr1_in;
logic [WIDTH_STAT-1:0] ctr1_out;
logic [WIDTH_STAT-1:0] ctr2_in;
logic [WIDTH_STAT-1:0] ctr2_out;
logic overflow_in;
logic overflow_out;

bit verbose = 0;

always @(posedge clk) begin
  if (verbose) $strobe("t=%8.3f reset=%b wrreq=%b rdreq=%b q=%d empty=%b full=%b used1=%d dout_valid=%b used2=%d",
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
.empty,

.ctr1_in,
.ctr1_out,
.ctr2_in,
.ctr2_out,
.overflow_in,
.overflow_out
);

integer queue[$];

logic want_write;
logic saw_backpressure;
assign wrreq = want_write;  // valid signal, held until accepted by !full

task automatic feed;
  input int nr;
  integer i;
  bit accepted;
  begin
    queue.push_back(nr);
    $display("[%0t] Feed: %0d", $time, nr);

    i = 1;
    want_write = 0;

    while (i <= nr) begin
      @(negedge clk);
      data = WIDTH_TOTAL'(i);
      want_write = 1'b1;       // propose to write
      accepted = !full;
      @(posedge clk);
      if (accepted) begin
        // successful write this cycle
        if (verbose) $display("W %0d %0d", nr, i);
        i = i + 1;
      end else begin
        saw_backpressure = 1'b1;
      end
    end

    @(negedge clk);
    want_write = 1'b0;
  end
endtask

task automatic read;
  int nr;
  integer j;
  reg [WIDTH_TOTAL-1:0] word;
  begin
    if (queue.size() > 0) begin
      nr = queue.pop_front();
      $display("[%0t] Read: %0d", $time, nr);
    end else begin
      $display("[%0t] Read: queue empty", $time);
      return;
    end

    j = 1;
    while (j <= nr) begin
      @(negedge clk);
      if (!empty) begin
        word = q;         // show-ahead FIFO: current word is already visible

        if (verbose) $display("R %0d %0d > %0d", nr, j, word);
        assert(word == WIDTH_TOTAL'(j)) else $fatal;

        rdreq = 1'b1;     // consume the visible word on the next edge
        @(posedge clk);
        @(negedge clk);
        rdreq = 1'b0;

        j = j + 1;
      end else begin
        rdreq = 1'b0;
      end
    end

    // clean deassertion after last read
    @(negedge clk);
    rdreq = 1'b0;
  end
endtask

parameter int loops = 100;

task automatic producer;
integer i;
begin
  for (i = 0; i < loops; i = i + 1) begin
    feed(i == 0 ? 40 : $urandom_range(0, 100));  // first burst must hit backpressure
//    #10; // spacing between feeds
    #($urandom_range(10, 50));
  end
end
endtask

task automatic consumer;
integer i;
begin
  for (i = 0; i < loops; i = i + 1) begin
//    #10; // spacing between reads
    #($urandom_range(2, 50));
    read();
  end
end
endtask

integer fh;

initial begin
  data <= 96'b0;
  want_write <= 0;
  saw_backpressure = 1'b0;
  rdreq <= 0;
  #5;

  // run producer and consumer concurrently
  fork
    producer();
    consumer();
  join
  assert(saw_backpressure) else $fatal(1, "backpressure was not exercised");
  assert(!overflow_in) else $fatal(1, "legal input backpressure set overflow_in");
  assert(!overflow_out) else $fatal(1, "legal output backpressure set overflow_out");
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
`ifndef VERILATOR
  $set_coverage_db_name("run_input_3.ucdb");
`endif
  $finish;
end

endmodule: tb3_input
