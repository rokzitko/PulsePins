// Purpose: final-qout behavior test for the top-level streamer.
//
// Verifies that the final programmed output value is the one left visible after sequence
// completion, which is important for host-side final-state checks.
// Final qout setting test
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_6;

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
  $strobe("t=%8.3f q_p=%b q_m=%b used=%d fifo_empty=%b state=%d armed=%b activated=%b used_o=%d q=%h valid=%b wr_last=%b done=%b qout=%h qout_valid=%b",
    $realtime,
    dut.ct0.q_pattern, dut.ct0.q_mask, dut.ct0.used, dut.ct0.fifo_empty, dut.ct0.state,
    dut.trigger_armed, dut.trigger_activated, dut.fifo0.used,
    dut.fifo0.q, dut.fifo0.valid, dut.fifo0.wr_last,
    dut.done,
    dut.fifo0.qout, dut.fifo0.qout_valid
);
end

reg [WIDTH_TRIGGER-1:0] trigger_in;
wire [WIDTH_DATA-1:0] qout;
wire qout_valid, done, buffer_error;

streamer dut(.clk, .reset, .input_data, .input_valid, .input_ready, .initial_value,
  .initial_reload(1'b0),
  .initial_value_streamer(initial_value),
  .trigger_enable(1'b1),
  .trigger_force(1'b0),
  .trigger_reset(1'b0),
  .trigger_in,
  .gate_enable(1'b1), .stop(1'b0), .stop_on_buffer_error(1'b0),
  .qout, .qout_valid, .done, .buffer_error,
  .streamer_clk(clk) // required for the offloading elements from trigger queue
);

int samples = 0;
bit checker_done = 0;
`include "check_inputs.svh"

always @(posedge clk) begin
  if (qout_valid) begin
    assert(samples < 6 && qout === 32'h87654321) else
      $fatal(1, "final-value test sample %0d: got %h", samples, qout);
    samples++;
  end
end

initial begin
  wait(done === 1'b1);
  // The terminator changes qout without adding a valid sample, then holds it.
  repeat (8) begin
    @(negedge clk);
    assert(samples == 6 && qout === 32'hffffffff && !qout_valid && !buffer_error) else
      $fatal(1, "final value/count not held: samples=%0d qout=%h", samples, qout);
  end
  checker_done = 1;
end

initial begin
  input_data <= 32'b0;
  input_valid <= 0;
  #10
  input_data <= { 32'b0011, 32'b0, 32'b00000001_00000001 }; // trigger: 'b11 (trigger, final), mask, pattern
  input_valid <= 1;
  #1
  input_data <= { 32'h0, 32'h6, 32'h87654321 }; // 6 elements
  input_valid <= 1;
  #1
  input_data <= { 32'h4, 32'h1, 32'hffffffff }; // final
  input_valid <= 1;
  #1
  input_valid <= 0;
end

initial begin
  trigger_in <= 0;
  #30;
  assert(samples == 0 && qout === initial_value && !done) else $fatal(1, "output before trigger");
  trigger_in <= 'b01;
  #1;
end

integer fh;

initial begin
  wait(checker_done);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_6.ucdb");
  $finish;
end

initial #200 $fatal(1, "timeout waiting for six samples and stable final qout");

endmodule: tb_st_6
