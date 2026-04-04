// Purpose: top-level streamer underrun / buffer_error test.
//
// Loads a deliberately unterminated stream, forces triggering, and verifies that once the output
// FIFO runs out of data the streamer raises `buffer_error` instead of reporting clean completion.
// This checks the core robustness path that host-side software later inspects after playback.
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

// Trace the trigger state, FIFO occupancy, and error status so underrun behavior is easy to
// diagnose from the console log alone.
always @(posedge clk) begin
  if (qout_valid)
    valid_count <= valid_count + 1;
  $strobe("t=%8.3f force=%b act=%b used=%0d rdreq=%b q=%h valid=%b done=%b buffer_error=%b",
    $realtime,
    trigger_force,
    trigger_activated,
    dut.fifo0.used,
    dut.rdreq,
    qout,
    qout_valid,
    done,
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

// Load one ordinary element and intentionally omit the terminating element so playback must
// eventually underrun once the trigger has activated and the FIFO empties.
initial begin
  input_data <= '0;
  input_valid <= 0;
  trigger_in <= '0;
  trigger_force <= 0;

  #10;
  input_data <= { 32'h0, 32'h3, 32'h00000011 };
  input_valid <= 1;
  #1;
  input_valid <= 0;

  #10;
  trigger_force <= 1;
end

// Sanity check before triggering: no completion and no underrun yet.
initial begin
  #18;
  assert(buffer_error == 0) else $fatal;
  assert(done == 0) else $fatal;
end

// Once the FIFO is exhausted, the core should report buffer_error instead of done.
initial begin
  wait(buffer_error == 1);
  #1;
  assert(done == 0) else $fatal;
  assert(valid_count > 0) else $fatal;
  assert(qout == 32'h00000011) else $fatal;
end

integer fh;

initial begin
  #80 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_8.ucdb");
  $finish;
end

endmodule: tb_st_8
