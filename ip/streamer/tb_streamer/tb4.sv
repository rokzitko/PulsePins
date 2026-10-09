// Purpose: empty-sequence corner-case test for the top-level streamer.
//
// Verifies that the streamer handles an empty or near-empty programmed stream cleanly without
// spurious output elements or invalid completion behavior.
// Test for empty sequences
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_4;

reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

wire streamer_clk = clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

// Interface
reg [WIDTH_TOTAL-1:0] input_data;
reg input_valid;
wire input_ready;

wire [WIDTH_DATA-1:0] initial_value = 0;

reg trigger_enable;
reg trigger_force;
wire [31:0] qout;
wire valid;
bit occupancy_checked = 0, final_checked = 0;

always @(posedge clk) begin
  $strobe("t=%8.3f ctr=%h data=%h rdreq_i=%b empty_i=%b in_v_d=%b curr_value=%h curr_cnt=%d out_data=%h out_wrreq=%b in_rdreq=%b used_o=%d qout=%h",
    $realtime, dut.counter, dut.data, dut.rdreq_i, dut.empty_i, dut.in_valid_data,
    dut.rl0.curr_value, dut.rl0.curr_cnt,
    dut.rl0.out_data, dut.rl0.out_wrreq, dut.rl0.in_rdreq, dut.fifo0.used, qout
);
end

logic gate_enable;
assign gate_enable = 1'b1;

streamer dut(.clk, .reset, .input_data, .input_valid, .input_ready, .initial_value, .initial_reload(1'b0), .initial_value_streamer(initial_value), .trigger_enable, .trigger_force,
             .streamer_clk, .qout, .qout_valid(valid), .gate_enable, .stop(0),
             .stop_on_buffer_error(0), .trigger_in('0), .trigger_reset(1'b0)
             );

`include "check_inputs.svh"

initial begin
  input_data <= 32'b0;
  input_valid <= 0;
  #10
  input_data <= { 32'h4, 32'h1, 32'hffffffff }; // final
  input_valid <= 1;
  #1
  input_data <= 0;
  input_valid <= 0;
end

initial begin
  #20;
  assert(dut.fifo0.used == 1) else $fatal;
  occupancy_checked = 1;
end

initial begin
  trigger_enable <= 0;
  trigger_force <= 0;
  #30;
  trigger_force <= 1;
  #1;
  #1;
  #1step;
  assert(qout == 32'hffffffff) else $fatal;
  final_checked = 1;
end

integer i;

initial begin
  #30;
  for (i = 0; i < 20; i = i+1) begin
    $strobe("t=%8.3f qout=%h valid=%b",
      $realtime, qout, valid);
      #0.1;
  end
end

initial begin
  #10;
  wait(valid == 1);
  $fatal; // no strobe expected
end

integer fh;

initial begin
  #60;
  tb_require_scenario("terminator_prefilled", occupancy_checked);
  tb_require_scenario("trigger_activation", cov_activations);
  tb_require_scenario("empty_sequence_completion", cov_completions);
  tb_require_scenario("final_value_checked", final_checked);
  tb_require_scenario("no_payload", cov_payload == 0 && cov_quiet_cycles > 0);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_4.ucdb");
  $finish;
end

`include "scenarios.svh"
endmodule: tb_st_4
