// Purpose: directed top-level trigger-element test.
//
// Loads trigger-program data into the top-level streamer and checks that trigger-specific
// handling is accepted by the integration path instead of being treated like normal output data.
// Load a trigger condition
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_2;

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
  $strobe("t=%8.3f control=%h ctr=%h data=%h rdreq_i=%b empty_i=%b in_v_d=%b in_v_c=%b p=%b m=%b c=%b wrreq=%b used=%d fifo_empty=%b state=%d armed=%b activated=%b used_o=%d",
    $realtime, dut.control, dut.counter, dut.data, dut.rdreq_i, dut.empty_i, dut.in_valid_data, dut.in_valid_chain,
    dut.ct0.pattern, dut.ct0.mask, dut.ct0.control, dut.ct0.wrreq, dut.ct0.used, dut.ct0.fifo_empty, dut.ct0.state,
    dut.trigger_armed, dut.trigger_activated, dut.fifo0.used
);
end

logic [WIDTH_TRIGGER-1:0] trigger_in = '0;
wire [WIDTH_DATA-1:0] qout;
wire qout_valid, done, buffer_error, trigger_armed, trigger_activated;

streamer dut(.clk, .reset, .input_data, .input_valid, .input_ready, .initial_value,
  .initial_reload(1'b0),
  .initial_value_streamer(initial_value),
  .trigger_enable(1'b1),
  .trigger_force(1'b0),
  .trigger_reset(1'b0),
  .trigger_in,
  .gate_enable(1'b1), .stop(1'b0), .stop_on_buffer_error(1'b0),
  .qout, .qout_valid, .done, .buffer_error, .trigger_armed, .trigger_activated,
  .streamer_clk(clk) // required for the offloading elements from trigger queue
);

bit checker_done = 0;
`include "check_inputs.svh"

// A trigger and a terminator must never become ordinary output samples.
always @(posedge clk) begin
  if (!reset && $time > 8)
    assert(qout_valid === 1'b0) else $fatal(1, "trigger/terminator emitted as data");
end

initial begin
  input_data <= 32'b0;
  input_valid <= 0;
  #10
  input_data <= { 32'b0011, 32'b0, 32'b00000001_00000001 }; // trigger: 'b11 (trigger, final), mask, pattern
  input_valid <= 1;
  #1
  input_data <= { 32'h4, 32'h1, 32'hffffffff }; // final
  input_valid <= 1;
  #1
  input_valid <= 0;
end

initial begin
  wait(trigger_armed === 1'b1);
  repeat (4) @(negedge clk);
  assert(!trigger_activated && !done && qout === initial_value) else
    $fatal(1, "trigger matched before pattern=1 arrived");
  trigger_in = 1;
  wait(trigger_activated === 1'b1);
  wait(done === 1'b1);
  repeat (4) @(negedge clk);
  assert(qout === 32'hffffffff && !buffer_error) else $fatal(1, "incorrect trigger-only completion");
  checker_done = 1;
end

integer fh;

initial begin
  wait(checker_done);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_2.ucdb");
  $finish;
end

initial #200 $fatal(1, "timeout waiting for trigger arming, activation, or completion");

endmodule: tb_st_2
