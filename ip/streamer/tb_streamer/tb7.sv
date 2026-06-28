// Purpose: top-level streamer gating integration test.
//
// Verifies that trigger activation can occur while the gate is closed, but output FIFO
// progression remains blocked until the gate is opened. Once the gate opens, playback must
// resume cleanly, preserve ordering, and reach the expected final qout state.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_7;

localparam logic [WIDTH_DATA-1:0] INITIAL_VALUE_STREAMER = WIDTH_DATA'(32'ha5a5_a5a5);

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

// Trace the gate state, trigger state, and FIFO occupancy so gating failures are diagnosable
// without immediately opening a waveform viewer.
always @(posedge clk) begin
  $strobe("t=%8.3f trig_in=%b force=%b armed=%b act=%b gate=%b rdreq=%b used=%0d qout=%h valid=%b done=%b",
    $realtime,
    trigger_in,
    trigger_force,
    trigger_armed,
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
  .initial_reload(1'b0),
  .initial_value_streamer(INITIAL_VALUE_STREAMER),
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

// Load a short deterministic stream while the gate is still closed.
initial begin
  input_data <= '0;
  input_valid <= 0;
  trigger_in <= '0;
  trigger_force <= 0;
  gate_enable <= 0;

  #10;
  input_data <= { 32'h0, 32'h3, 32'h00000011 }; // 3 cycles of 0x11
  input_valid <= 1;
  #1;
  input_data <= { 32'h0, 32'h2, 32'h00000022 }; // 2 cycles of 0x22
  input_valid <= 1;
  #1;
  input_data <= { 32'h4, 32'h1, 32'h00000033 }; // final qout = 0x33
  input_valid <= 1;
  #1;
  input_valid <= 0;
end

// Activate the trigger while the gate remains closed, then open the gate later.
initial begin
  #30;
  trigger_force <= 1;
  #20;
  gate_enable <= 1;
end

integer used_before_gate;
integer sample_idx = 0;

// While the gate is closed, trigger activation may happen but the output FIFO must not advance.
initial begin
  wait(trigger_activated == 1);
  used_before_gate = dut.fifo0.used;
  repeat (4) @(posedge clk);
  assert(gate_enable == 0) else $fatal;
  assert(dut.rdreq == 0) else $fatal;
  assert(dut.trigger_latch == 0) else $fatal;
  assert(dut.fifo0.used == used_before_gate) else $fatal;
  assert(qout == INITIAL_VALUE_STREAMER) else $fatal;
  assert(qout_valid == 0) else $fatal;
  assert(done == 0) else $fatal;
end

// Once the gate opens, output progression should resume in order.
always @(posedge clk) begin
  if (qout_valid) begin
    case (sample_idx)
      0, 1, 2: assert(qout == 32'h00000011) else $fatal;
      3, 4:    assert(qout == 32'h00000022) else $fatal;
      default: $fatal;
    endcase
    sample_idx = sample_idx + 1;
  end
end

initial begin
  #55;
  assert(gate_enable == 1) else $fatal;
  assert(dut.fifo0.used < used_before_gate) else $fatal;
end

initial begin
  wait(done == 1);
  #1;
  assert(sample_idx == 5) else $fatal;
  assert(qout == 32'h00000033) else $fatal;
  assert(buffer_error == 0) else $fatal;
end

integer fh;

initial begin
  #90 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_7.ucdb");
  $finish;
end

endmodule: tb_st_7
