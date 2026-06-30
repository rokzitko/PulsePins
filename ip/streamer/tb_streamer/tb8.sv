// Purpose: top-level streamer underrun / buffer_error test.
//
// Starts with too little output data, forces triggering, and verifies that once the output FIFO
// runs out of data the streamer raises `buffer_error` instead of reporting clean completion.
// A late terminator is then written to verify that output progression stops without asserting
// public `done`, which remains clean-completion-only.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_8;

logic clk;
logic reset;

// Simple 1 ns clock and short reset pulse used only to establish deterministic initial state.
initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

logic [WIDTH_TOTAL-1:0] input_data;
logic input_valid;
wire input_ready;

logic [WIDTH_TRIGGER-1:0] trigger_in;
logic trigger_force;

wire [WIDTH_DATA-1:0] qout;
wire qout_valid;
wire strobe;
wire strobe_enable;
wire buffer_error;
wire done;
wire trigger_armed;
wire trigger_activated;

integer valid_count = 0;
logic underrun_checked = 1'b0;
logic terminal_checked = 1'b0;

localparam logic [WIDTH_CONTROL-1:0] CONTROL_REGULAR = '0;
localparam logic [WIDTH_CONTROL-1:0] CONTROL_FINAL = WIDTH_CONTROL'(1) << BIT_TERMINATE;

// Trace the trigger state, FIFO occupancy, and error status so underrun behavior is easy to
// diagnose from the console log alone.
always @(posedge clk) begin
  if (qout_valid)
    valid_count <= valid_count + 1;
  $strobe("t=%8.3f force=%b act=%b used=%0d rdreq=%b q=%h valid=%b done=%b term=%b buffer_error=%b",
    $realtime,
    trigger_force,
    trigger_activated,
    dut.fifo0.used,
    dut.rdreq,
    qout,
    qout_valid,
    done,
    dut.fifo0.terminal_seen,
    buffer_error
  );
end

streamer dut(
  .clk,
  .reset,
  .input_data,
  .input_valid,
  .input_ready,
  .gate_enable(1'b1),
  .initial_value('0),
  .initial_reload(1'b0),
  .initial_value_streamer('0),
  .streamer_clk(clk),
  .qout,
  .qout_valid,
  .strobe,
  .strobe_enable,
  .buffer_error,
  .done,
  .trigger_in,
  .trigger_enable(1'b1),
  .trigger_force(trigger_force),
  .trigger_reset(1'b0),
  .trigger_armed,
  .trigger_activated,
  .stop(1'b0),
  .stop_on_buffer_error(1'b0)
);

task automatic send_word(input logic [WIDTH_CONTROL-1:0] control,
                         input logic [WIDTH_COUNTER-1:0] counter,
                         input logic [WIDTH_DATA-1:0] value);
  begin
    @(negedge clk);
    input_data <= { control, counter, value };
    input_valid <= 1;
    @(posedge clk);
    assert(input_ready == 1) else $fatal(1, "input FIFO unexpectedly not ready");
    @(negedge clk);
    input_valid <= 0;
    input_data <= '0;
  end
endtask

// Load one ordinary element first and hold the terminator back so playback must underrun once
// the trigger has activated and the output FIFO empties. The late terminator checks that
// stop_on_buffer_error=0 permits recovery to an idle terminal state without asserting `done`.
initial begin
  input_data <= '0;
  input_valid <= 0;
  trigger_in <= '0;
  trigger_force <= 0;

  wait(reset == 0);
  repeat (7) @(negedge clk);
  send_word(CONTROL_REGULAR, WIDTH_COUNTER'(3), WIDTH_DATA'(32'h00000011));

  repeat (10) @(negedge clk);
  trigger_force <= 1;

  wait(buffer_error == 1);
  send_word(CONTROL_FINAL, WIDTH_COUNTER'(1), WIDTH_DATA'(32'h00000022));
end

// Sanity check before triggering: no completion and no underrun yet.
initial begin
  #18;
  assert(buffer_error == 0) else $fatal;
  assert(done == 0) else $fatal;
end

// Once the FIFO is exhausted, the core should report buffer_error instead of done and keep
// advancing because stop_on_buffer_error is disabled.
initial begin
  wait(buffer_error == 1);
  #0.1;
  assert(done == 0) else $fatal;
  assert(trigger_activated == 1) else $fatal;
  assert(valid_count > 0) else $fatal;
  assert(qout == WIDTH_DATA'(32'h00000011)) else $fatal;
  underrun_checked = 1'b1;
end

// The late terminator stops progression, but it must not turn the failed run into a clean one.
initial begin
  wait(dut.fifo0.terminal_seen == 1);
  #0.1;
  assert(buffer_error == 1) else $fatal;
  assert(done == 0) else $fatal;
  assert(trigger_activated == 0) else $fatal;
  assert(qout == WIDTH_DATA'(32'h00000022)) else $fatal;
  terminal_checked = 1'b1;
end

integer fh;

initial begin
  wait(underrun_checked && terminal_checked);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_8.ucdb");
  $finish;
end

initial begin
  #200 $fatal(1, "tb8 timeout");
end

endmodule: tb_st_8
