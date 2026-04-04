// Purpose: additional combined FIFO + preprocessor integration test.
//
// Extends coverage of combined-path corner cases beyond the earlier directed and backpressure
// scenarios.
// Storage of elements, etc.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_input_pre_6;

`include "common_init.sv"

// look around used2=2032

parameter logic [31:0] NR = 'd2100;

initial begin
  wrreq <= 0;
  data <= 96'b0;
  #4
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
  data <= { REPLAY, NR, 32'h04 }; // replay 4 elements many (NR) times
  #1
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h01aa }; // store (do not pass)
  #1;
  wrreq <= 1;
  data <= { TERMINATE, 32'd0, 32'd0 };
  #1;
  wrreq <= 0;
end

int j;
initial begin
  #2060;
  for (j = 0; j < 2000; j++) begin
    rdreq <= 1;
    #10;
    rdreq <= 0;
    #2;
  end
end

// checker for expected output
integer i;
initial begin
  rdreq <= 0;
  #42050 rdreq <= 1; // delayed trigger

  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff000008ff) else $fatal;
  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff000009ff) else $fatal;

  for (i = 0; i < NR; i++) begin
   @(posedge clk iff rdreq);
   assert(q[63:0] == 'h00001234000004ff) else $fatal;
   @(posedge clk iff rdreq);
   assert(q[63:0] == 'h00001234000005ee) else $fatal;
   @(posedge clk iff rdreq);
   assert(q[63:0] == 'h00001234000006dd) else $fatal;
   @(posedge clk iff rdreq);
   assert(q[63:0] == 'h00001234000007cc) else $fatal;
  end

  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff000001aa) else $fatal;

  @(posedge clk iff rdreq);
  $fatal; // should never be reached
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
  #20000;
//  wait(empty == 1);
//  #5;
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_input_pre_6.ucdb");
  $finish;
end

endmodule: tb_input_pre_6
