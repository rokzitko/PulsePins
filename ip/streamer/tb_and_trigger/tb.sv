// Purpose: focused functional test for the simple single-stage trigger primitive.
//
// This testbench verifies the mask/pattern trigger behavior used as a building block for more
// complex trigger routing.
// Simple simulation test bench
// Rok Zitko, 2025

`include "../config.vh"

`default_nettype none

module tb_and;

timeunit 1ns;
timeprecision 1ps;

logic clk;
logic reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #1 reset <= 0;
end

logic [WIDTH_TRIGGER-1:0] i;
logic [WIDTH_TRIGGER-1:0] pattern;
logic [WIDTH_TRIGGER-1:0] mask;
logic trigger_enable;
logic o;
int mismatch_cycles = 0, matching_cycles = 0;
bit sampled_trigger_checked = 0;
always @(posedge clk) if (!reset && trigger_enable) begin
  if ((i & mask) == (pattern & mask)) matching_cycles++;
  else mismatch_cycles++;
end

always @(posedge clk) begin
  $strobe("t=%8.3f reset=%b trigger_enable=%b i=%b o=%b", $realtime, reset, trigger_enable, i, o);
end

and_trigger dut(.i(i), .pattern(pattern), .mask(mask), .clk(clk), .reset(reset), .trigger_enable(trigger_enable), .o(o));

initial begin
  i       <= 8'b00000000;
  pattern <= 8'b00000001;
  mask    <= 8'b00000001;
  trigger_enable <= 0;
  #2 trigger_enable <= 1;
  #1;
  assert(o == 1'b0) else $fatal;
  #3.5 i <= 8'hFF;
  $display("Triggered at t=%0.3f", $realtime);
  #0 assert(o == 1'b0) else $fatal; // before clock tick
  @(posedge clk);
  #1ps assert(o == 1'b1) else $fatal; // after clock tick, at the declared timeprecision
  sampled_trigger_checked = 1;
end

integer fh;

initial begin
  #10;
  tb_require_scenario("enabled_mismatches", mismatch_cycles);
  tb_require_scenario("enabled_matches", matching_cycles);
  tb_require_scenario("registered_trigger_checked", sampled_trigger_checked);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
`ifndef IVERILOG
  $set_coverage_db_name("run_and.ucdb");
`endif
  $finish;
end

`include "../../testbench/scenario_coverage.svh"
endmodule: tb_and
