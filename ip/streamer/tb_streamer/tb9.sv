// Purpose: top-level streamer mid-stream gating pause/resume test.
//
// Starts playback with the gate open, closes the gate while output data are already advancing,
// checks that FIFO progression pauses without losing ordering, then reopens the gate and confirms
// that playback resumes cleanly and reaches the expected final qout state.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_9;

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

logic gate_enable;
logic trigger_force;
logic [WIDTH_TRIGGER-1:0] trigger_in;

wire [WIDTH_DATA-1:0] qout;
wire qout_valid;
wire strobe;
wire strobe_enable;
wire buffer_error;
wire done;
wire trigger_armed;
wire trigger_activated;

integer sample_idx = 0;
integer sample_idx_at_gate_close;

// Trace the gate state, FIFO occupancy, and visible output so pause/resume failures are easy to
// diagnose from the console log.
always @(posedge clk) begin
  if (qout_valid)
    sample_idx <= sample_idx + 1;
  $strobe("t=%8.3f force=%b act=%b gate=%b rdreq=%b used=%0d qout=%h valid=%b done=%b",
    $realtime,
    trigger_force,
    trigger_activated,
    gate_enable,
    dut.rdreq,
    dut.fifo0.used,
    qout,
    qout_valid,
    done
  );
end

streamer dut(
  .clk,
  .reset,
  .input_data,
  .input_valid,
  .input_ready,
  .gate_enable,
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

// Load a short deterministic stream and start with the gate open.
initial begin
  input_data <= '0;
  input_valid <= 0;
  trigger_force <= 0;
  trigger_in <= '0;
  gate_enable <= 1;

  #10;
  input_data <= { 32'h0, 32'h3, 32'h00000011 }; // 3 cycles of 0x11
  input_valid <= 1;
  #1;
  input_data <= { 32'h0, 32'h2, 32'h00000022 }; // 2 cycles of 0x22
  input_valid <= 1;
  #1;
  input_data <= { 32'h0, 32'h2, 32'h00000044 }; // 2 cycles of 0x44
  input_valid <= 1;
  #1;
  input_data <= { 32'h4, 32'h1, 32'h00000055 }; // final qout = 0x55
  input_valid <= 1;
  #1;
  input_valid <= 0;

  #10;
  trigger_force <= 1;
end

// Close the gate after playback has already started, then reopen it later.
initial begin
  wait(sample_idx >= 3);
  #0.1;
  gate_enable <= 0;

  repeat (4) @(posedge clk);
  assert(gate_enable == 0) else $fatal;
  assert(dut.rdreq == 0) else $fatal;
  assert(qout_valid == 0) else $fatal;
  assert(done == 0) else $fatal;
  sample_idx_at_gate_close = sample_idx;

  repeat (3) @(posedge clk);
  assert(dut.rdreq == 0) else $fatal;
  assert(qout_valid == 0) else $fatal;
  assert(sample_idx == sample_idx_at_gate_close) else $fatal;
  assert(done == 0) else $fatal;

  gate_enable <= 1;
end

// Check the resumed progression order. We do not assume exact cycle timing, only the visible
// ordering before and after the gated pause.
always @(posedge clk) begin
  if (qout_valid) begin
    case (sample_idx)
      0, 1, 2: assert(qout == 32'h00000011) else $fatal;
      3, 4:    assert(qout == 32'h00000022) else $fatal;
      5, 6:    assert(qout == 32'h00000044) else $fatal;
      default: $fatal;
    endcase
  end
end

initial begin
  wait(done == 1);
  #1;
  assert(sample_idx == 7) else $fatal;
  assert(qout == 32'h00000055) else $fatal;
  assert(buffer_error == 0) else $fatal;
end

integer fh;

initial begin
  #120 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_9.ucdb");
  $finish;
end

endmodule: tb_st_9
