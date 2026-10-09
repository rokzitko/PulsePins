// Purpose: no-strobe/readback edge-case test for the run-length encoder core.
//
// Exercises a corner case related to no-strobe style behavior and the corresponding activation
// handling in the encoder.
// Case of no-strobe element: this situation would require restoring the "activated" signal.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../rl_config.vh"

`default_nettype none

module tb;

reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

// Interface
reg [cfg::WIDTH_DATA-1:0] data;
reg valid;
wire data_clk = clk;
bit stimulus_done = 0;
bit checker_done = 0;

always @(posedge clk) begin
  $strobe("t=%8.3f data=%h valid=%b run_value=%4h run_count=%4d j=%h wrreq=%b",
    $realtime, data, valid, dut.run_value, dut.run_count, dut.j, dut.wrreq
  );
end

rl_encoder dut(.clk, .reset, .data, .valid, .data_clk, .mode(1'b1), .strobe(1'b0), .rdreq(1'b0));

initial begin
  data <= 32'b0;
  valid <= 0;
  #10
  data <= { 32'h11 };
  valid <= 1;
  #1
  data <= { 32'h11 };
  valid <= 1;
  #1
  data <= { 32'h11 };
  valid <= 1;
  #1
  data <= { 32'hAA };
  valid <= 0;
  #1
  data <= { 32'h22 };
  valid <= 1;
  #1
  data <= { 32'hFF };
  valid <= 0;
  stimulus_done = 1;
end

initial begin
   wait (dut.wrreq == 1);
   @(posedge clk);
   assert(dut.j == 'h0000000300000011) else $fatal;
   #1;

   wait (dut.wrreq == 1);
   @(posedge clk);
   assert(dut.j == 'h0000000100000022) else $fatal;
   checker_done = 1;
end

integer fh;

initial begin
  wait(stimulus_done && checker_done);
  @(negedge clk);
  tb_require_scenario("accepted_samples", cov_samples, 4);
  tb_require_scenario("separated_valid_blocks", cov_blocks, 2);
  tb_require_scenario("gap_flushes", cov_flushes, 2);
  tb_require_scenario("encoded_runs", cov_encoded, 2);
  tb_require_scenario("run_values_checked", checker_done);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $finish;
end

initial #100 $fatal(1, "timeout waiting for encoded runs");

`include "scenarios.svh"
endmodule: tb
