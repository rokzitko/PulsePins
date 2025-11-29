// Storage of elements, etc.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_input_pre_4;

`include "common_init.sv"

initial begin
  wrreq <= 0;
  data <= 96'b0;
  #4
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h01ff }; // pass
  #1
  wrreq <= 1;
  data <= { NOPASS, 32'h00ff, 32'h02ff }; // do not pass
  #1
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h03ff }; // pass
  #1
  wrreq <= 1;
  data <= { STORE0, 32'h1234, 32'h04ff }; // store (do not pass)
  #1
  wrreq <= 1;
  data <= { STORE1, 32'h1234, 32'h05ee }; // store (do not pass)
  #1
  wrreq <= 1;
  data <= { STORE2, 32'h1234, 32'h06dd }; // store (do not pass)
  #1
  wrreq <= 1;
  data <= { STORE3, 32'h1234, 32'h07cc }; // store (do not pass)
  #1
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h08ff }; // pass
  #1
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h09ff }; // pass
  #1
  wrreq <= 1;
  data <= { REPLAY, 32'h0004, 32'h01 }; // replay 1 element 4 times (do not pass)
  #1;
  wrreq <= 0;
  data <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  #10
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h0aff }; // pass
  #1
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h0bff }; // pass
  #1
  wrreq <= 0;
  data <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
end

// checker for expected output
integer i;
initial begin
  rdreq <= 0;
  #40 rdreq <= 1; // delayed trigger
  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff000001ff) else $fatal;
  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff000003ff) else $fatal;
  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff000008ff) else $fatal;
  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff000009ff) else $fatal;

  for (i = 0; i < 4; i++) begin
   @(posedge clk iff rdreq);
   assert(q[63:0] == 'h00001234000004ff) else $fatal;
  end

  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff00000aff) else $fatal;
  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff00000bff) else $fatal;
end

// check exact timing
initial begin
  #49;
  @(posedge clk);
  #1step $display("%h", q);
  assert(q == 'h00000000000000ff00000bff) else $fatal;
end

initial begin
   #20;
   $strobe("%h", dut.proc.memory[0]);
   $strobe("%h", dut.proc.memory[1]);
   $strobe("%h", dut.proc.memory[2]);
   $strobe("%h", dut.proc.memory[3]);
   $strobe("%h", dut.proc.memory[4]);
end

integer fh;

initial begin
  #50;
  wait(empty == 1);
  #5;
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_input_pre_4.ucdb");
  $finish;
end

endmodule: tb_input_pre_4
