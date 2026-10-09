// Purpose: top-level preprocessor/replay integration test.
//
// Exercises store + replay behavior through the full `streamer` wrapper and checks that replay
// expands into the expected number and ordering of output elements in the output FIFO.
// Test for preprocessor sequences; check for the number of elements in the output FIFO
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_3;

reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

// Interface
reg [WIDTH_TOTAL-1:0] input_data;
reg input_valid;
wire input_ready;

wire [WIDTH_DATA-1:0] initial_value = 0;

always @(posedge clk) begin
  $strobe("t=%8.3f ctr=%h data=%h rdreq_i=%b empty_i=%b in_v_d=%b curr_value=%h curr_cnt=%d out_data=%h out_wrreq=%b in_rdreq=%b used_o=%d",
    $realtime, dut.counter, dut.data, dut.rdreq_i, dut.empty_i, dut.in_valid_data,
    dut.rl0.curr_value, dut.rl0.curr_cnt,
    dut.rl0.out_data, dut.rl0.out_wrreq, dut.rl0.in_rdreq, dut.fifo0.used
);
end

logic trigger_force = 0;
wire [WIDTH_DATA-1:0] qout;
wire qout_valid, done, buffer_error;

streamer dut(.clk, .reset, .input_data, .input_valid, .input_ready,
  .initial_value, .initial_reload(1'b0), .initial_value_streamer(initial_value),
  .streamer_clk(clk), .gate_enable(1'b1), .stop(1'b0), .stop_on_buffer_error(1'b0),
  .trigger_in('0), .trigger_enable(1'b0), .trigger_force, .trigger_reset(1'b0),
  .qout, .qout_valid, .done, .buffer_error);

int samples = 0;
bit checker_done = 0;
`include "check_inputs.svh"

// Reference expansion: (2 x AA, 3 x BB) repeated three times.
always @(posedge clk) begin
  if (qout_valid) begin
    assert(samples < 15) else $fatal(1, "extra replay sample");
    assert(qout === ((samples % 5 < 2) ? 32'hAA : 32'hBB)) else
      $fatal(1, "replay sample %0d: got %h", samples, qout);
    samples++;
  end
end

initial begin
  #40;
  assert(samples == 0 && qout === initial_value && done === 1'b0) else
    $fatal(1, "replay started before forced trigger");
  trigger_force <= 1;
  wait(done === 1'b1);
  repeat (4) @(negedge clk);
  assert(samples == 15 && qout === 32'hffffffff && !qout_valid && !buffer_error) else
    $fatal(1, "incorrect replay completion: samples=%0d qout=%h", samples, qout);
  checker_done = 1;
end

localparam [31:0] PASS = 0;
localparam [31:0] NOPASS = 1 << BIT_NOPASS;
localparam [31:0] STORE0 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (0 << BIT_POSITIONS_LO);
localparam [31:0] STORE1 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (1 << BIT_POSITIONS_LO);
localparam [31:0] STORE2 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (2 << BIT_POSITIONS_LO);
localparam [31:0] STORE3 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (3 << BIT_POSITIONS_LO);
localparam [31:0] REPLAY = (1 << BIT_NOPASS) + (1 << BIT_REPLAY);

initial begin
  input_data <= 32'b0;
  input_valid <= 0;
  #10
  input_data <= { STORE0, 32'h02, 32'hAA }; // 2x AA
  input_valid <= 1;
  #1
  input_data <= { STORE1, 32'h03, 32'hBB }; // 3x BB
  input_valid <= 1;
  #1
  input_data <= { REPLAY, 32'h0003, 32'h02 }; // replay 2 elements 3 times
  input_valid <= 1;
  #1
  input_data <= { 32'h4, 32'h1, 32'hffffffff }; // final
  input_valid <= 1;
  #1
  input_valid <= 0;
end

integer fh;

initial begin
  wait(checker_done);
  @(negedge clk);
  tb_require_scenario("pretrigger_quiet_cycles", cov_quiet_cycles);
  tb_require_scenario("trigger_activation", cov_activations);
  tb_require_scenario("replayed_samples", cov_payload, 15);
  tb_require_scenario("clean_completion", cov_completions);
  tb_require_scenario("final_state_checked", checker_done);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_3.ucdb");
  $finish;
end

initial #200 $fatal(1, "timeout waiting for 15 replay samples and completion");

`include "scenarios.svh"
endmodule: tb_st_3
