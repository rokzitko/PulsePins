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
logic [`WIDTH_TOTAL-1:0] data;
logic wrreq;
logic rdreq;

logic [`WIDTH_TOTAL-1:0] q;
logic full;
logic empty;

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
  wrreq <= 1;
  data <= 96'h01;

  for (i = 1; i < nr; i = i+1) begin
    #1
    data <= data + 1;
  end

  #1
  wrreq <= 0;
end
endtask

task automatic read;
input int nr;
integer j;
begin
  wait (empty == 0); // wait for FIFO to be ready
  rdreq <= 1;
  #1
  for (j = 1; j <= nr; j = j+1) begin
    @(posedge clk iff rdreq);
    assert(q == j) else $fatal;
  end
  rdreq <= 0;
end
endtask

initial begin
  data <= 96'b0;
  wrreq <= 0;
  #4
  feed(10);
  #40
  feed(15);
  #10
  feed(40);
end

initial begin
  #20
  read(10);
  #20;
  read(15);
  #5;
  read(40);
end

integer fh;

initial begin
  #200 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_input_2.ucdb");
  $finish;
end

endmodule: tb2_input
