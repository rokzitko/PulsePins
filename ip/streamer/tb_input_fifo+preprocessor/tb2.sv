// Purpose: additional directed storage/replay integration scenario.
//
// Exercises a second pass/store/replay pattern so the combined FIFO + preprocessor path is
// checked beyond the simplest directed case.
// Storage of elements, etc.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_input_pre_2;

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
  data <= { REPLAY, 32'h0001, 32'h04 }; // replay 1 time (do not pass)
  #1;
  wrreq <= 0;
  #10;
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
bit output_checked = 0, timing_checked = 0;
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

  for (i = 0; i < 1; i++) begin
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
  assert(q == 'h00000000000000ff00000aff) else $fatal;
  @(posedge clk iff rdreq);
  assert(q == 'h00000000000000ff00000bff) else $fatal;
  $strobe("done comparing");
  output_checked = 1;
end

// check exact timing
initial begin
  @(posedge rdreq);
  repeat (9) @(posedge clk);
  @(negedge clk);
  assert(q == 'h00000000000000ff00000bff) else $fatal;
  timing_checked = 1;
end

initial begin
   #15;
   $strobe("%h", dut.proc.memory[0]);
   $strobe("%h", dut.proc.memory[1]);
   $strobe("%h", dut.proc.memory[2]);
   $strobe("%h", dut.proc.memory[3]);
   $strobe("%h", dut.proc.memory[4]);
end

integer fh;

initial begin
  #50;
  wait(empty == 1 && output_checked && timing_checked);
  #5;
  tb_require_scenario("pass_commands", cov_passes, 6);
  tb_require_scenario("discard_commands", cov_discards);
  tb_require_scenario("stored_positions", $countones(cov_positions), 4);
  tb_require_scenario("finite_replay_words", cov_finite_words, 4);
  tb_require_scenario("checked_order", output_checked);
  tb_require_scenario("checked_timing", timing_checked);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_input_pre_2.ucdb");
  $finish;
end

initial #200 $fatal(1, "required ordering/timing scenario did not complete");

endmodule: tb_input_pre_2
