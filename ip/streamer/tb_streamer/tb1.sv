// Purpose: directed top-level streamer smoke test.
//
// Loads a short deterministic element stream and checks that regular elements reach the
// streamer datapath/output FIFO as expected. This is the simplest top-level functional check
// of the `streamer` integration path.
// Load 3 elements in output FIFO
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_1;

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

// Independent expansion of the three programmed run lengths (6 + 7 + 8).
always @(posedge clk) begin
  if (qout_valid) begin
    assert(samples < 21) else $fatal(1, "extra output sample");
    if (samples < 6)
      assert(qout === 32'h12345678) else $fatal(1, "first run sample %0d", samples);
    else if (samples < 13)
      assert(qout === 32'hababcdcd) else $fatal(1, "second run sample %0d", samples);
    else
      assert(qout === 32'h56781234) else $fatal(1, "third run sample %0d", samples);
    samples++;
  end
end

initial begin
  input_data <= 32'b0;
  input_valid <= 0;
  #10
  input_data <= { 32'h0, 32'h6, 32'h12345678 };
  input_valid <= 1;
  #1
  input_data <= { 32'h0, 32'h7, 32'hababcdcd };
  input_valid <= 1;
  #1
  input_data <= { 32'h0, 32'h8, 32'h56781234 };
  input_valid <= 1;
  #1
  input_data <= { 32'h4, 32'h1, 32'hffffffff };
  input_valid <= 1;
  #1
  input_valid <= 0;
end

initial begin
  #40;
  assert(samples == 0 && qout === initial_value && done === 1'b0) else
    $fatal(1, "playback started before forced trigger");
  trigger_force <= 1;
  wait(done === 1'b1);
  repeat (4) @(negedge clk);
  assert(samples == 21 && qout === 32'hffffffff && !qout_valid && !buffer_error) else
    $fatal(1, "incorrect completion: samples=%0d qout=%h", samples, qout);
  checker_done = 1;
end

integer fh;

initial begin
  wait(checker_done);
  @(negedge clk);
  tb_require_scenario("pretrigger_quiet_cycles", cov_quiet_cycles);
  tb_require_scenario("trigger_activation", cov_activations);
  tb_require_scenario("payload_samples", cov_payload, 21);
  tb_require_scenario("clean_completion", cov_completions);
  tb_require_scenario("final_state_checked", checker_done);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_1.ucdb");
  $finish;
end

initial #200 $fatal(1, "timeout waiting for 21 streamer samples and completion");

`include "scenarios.svh"
endmodule: tb_st_1
