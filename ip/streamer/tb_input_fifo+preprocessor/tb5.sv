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

bit verbose = 1;

always @(posedge clk) begin
  if (verbose) $strobe("t=%8.3f reset=%b wrreq=%b rdreq=%b q=%h empty=%b full=%b used1=%d dout_valid=%b used2=%d | i=%d j=%d",
    $realtime, reset, wrreq, rdreq, q, empty, full, dut.used1, dut.dout_valid, dut.used2,
    dut.proc.i, dut.proc.j
    );
end

input_fifo #(
.p1(4),  // smaller buffer, size=16!
.p2(4)
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

integer queue[$];

logic want_write;
assign wrreq = want_write & ~full;  // actual FIFO wrreq

task automatic feed;
  input int nr;
  integer i;
  begin
    queue.push_back(nr);
    $display("[%0t] Feed: %0d", $time, nr);

    i = 1;
    want_write = 0;

    while (i <= nr) begin
      data       <= i;
      want_write <= 1;         // propose to write
      @(posedge clk);
      if (wrreq) begin
        // successful write this cycle
        if (verbose) $display("W %0d %0d", nr, i);
        i = i + 1;
      end
    end

    want_write <= 0;
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
      @(posedge clk);
      if (!empty) begin
        rdreq <= 1;       // request advance
        @(posedge clk);   // wait one more cycle for q to update
        word = q;         // now q is valid
        rdreq <= 0;

        if (verbose) $display("R %0d %0d > %0d", nr, j, word);
        assert(word == j) else $fatal;

        j = j + 1;
      end else begin
        rdreq <= 0;
      end
    end

    // clean deassertion after last read
    @(posedge clk);
    rdreq <= 0;
  end
endtask

// Consumes all data from FIFO
task automatic readall;
  reg [WIDTH_TOTAL-1:0] word;
  begin
    while (1) begin
      @(posedge clk);
      if (!empty) begin
        rdreq <= 1;       // request advance
        @(posedge clk);   // wait one more cycle for q to update
        word = q;         // now q is valid
        rdreq <= 0;

        if (verbose) $display("R %0h", word);
      end else begin
        rdreq <= 0;
      end
    end
  end
endtask

parameter int loops = 100;

task automatic producer;
integer i;
begin
  for (i = 0; i < loops; i = i + 1) begin
    feed($urandom_range(0, 100));  // generate random int
    #($urandom_range(10, 50));
  end
end
endtask

task automatic consumer;
integer i;
begin
  for (i = 0; i < loops; i = i + 1) begin
    #($urandom_range(2, 50));
    read();
  end
end
endtask

integer fh;

initial begin
  data <= 96'b0;
  want_write <= 0;
  rdreq <= 0;
  #5;

  want_write <= 1;
  data <= { STORE0, 32'd10, 32'd01 }; // store (do not pass)
  #1
  data <= { STORE1, 32'd10, 32'd02 }; // store (do not pass)
  #1
  data <= { STORE2, 32'd10, 32'd03 }; // store (do not pass)
  #1
  data <= { STORE3, 32'd10, 32'd04 }; // store (do not pass)
  #1
  data <= { STORE3, 32'd10, 32'd04 }; // store (do not pass)
  #1
  want_write <= 0;

  #10;
  want_write <= 1;
  data <= { REPLAY, 32'h0004, 32'h05 }; // replay N x M elements
  #1;
  want_write <= 0;

  #50;
  readall();

  // run producer and consumer concurrently
  fork
    producer();
    consumer();
  join
end

initial begin
  #150;
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_input_pre_5.ucdb");
  $finish;
end

endmodule: tb5_input
