// Purpose: top-level trigger + replay integration test.
//
// Loads a short trigger program followed by stored elements and a replay request. The test
// verifies that replayed output does not begin until the trigger condition is met, then checks
// that the replay expansion preserves ordering and reaches the expected final qout state.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_10;

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

wire [WIDTH_DATA-1:0] qout;
wire qout_valid;
wire strobe;
wire strobe_enable;
wire buffer_error;
wire done;
wire trigger_armed;
wire trigger_activated;

localparam [31:0] STORE0 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (0 << BIT_POSITIONS_LO);
localparam [31:0] STORE1 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (1 << BIT_POSITIONS_LO);
localparam [31:0] REPLAY = (1 << BIT_NOPASS) + (1 << BIT_REPLAY);

integer sample_idx = 0;

// Trace trigger state, replay activity, and output progression so failures are easy to diagnose.
always @(posedge clk) begin
  if (qout_valid)
    sample_idx <= sample_idx + 1;
  $strobe("t=%8.3f trig_in=%b armed=%b act=%b used_trig=%0d used_out=%0d qout=%h valid=%b done=%b retrigreq=%b",
    $realtime,
    trigger_in,
    trigger_armed,
    trigger_activated,
    dut.ct0.used,
    dut.fifo0.used,
    qout,
    qout_valid,
    done,
    dut.retrig_requested
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
  .trigger_force(1'b0),
  .trigger_reset(1'b0),
  .trigger_armed,
  .trigger_activated,
  .stop(1'b0),
  .stop_on_buffer_error(1'b0)
);

// Load a trigger program plus two stored elements and replay them twice.
initial begin
  input_data <= '0;
  input_valid <= 0;
  trigger_in <= '0;

  #10;
  input_data <= { 32'b0011, 32'b0, 32'b00000001_00000001 }; // final trigger: mask=1, pattern=1
  input_valid <= 1;
  #1;
  input_data <= { STORE0, 32'h2, 32'h000000AA }; // 2 cycles of 0xAA, stored only
  input_valid <= 1;
  #1;
  input_data <= { STORE1, 32'h3, 32'h000000BB }; // 3 cycles of 0xBB, stored only
  input_valid <= 1;
  #1;
  input_data <= { REPLAY, 32'h0002, 32'h02 }; // replay 2 stored elements 2 times
  input_valid <= 1;
  #1;
  input_data <= { 32'h4, 32'h1, 32'h000000CC }; // final qout = 0xCC
  input_valid <= 1;
  #1;
  input_valid <= 0;
end

// Trigger later so the test can confirm that replayed output does not start early.
initial begin
  #35;
  trigger_in <= 'b1;
end

// Before the trigger arrives, the stored/replayed content must not yet appear on qout.
initial begin
  #25;
  assert(trigger_activated == 0) else $fatal;
  assert(sample_idx == 0) else $fatal;
  assert(qout == 0) else $fatal;
  assert(done == 0) else $fatal;
end

// After the trigger, replayed payload must appear in the expected order:
// 2x AA, 3x BB, repeated twice, then final qout CC.
always @(posedge clk) begin
  if (qout_valid) begin
    case (sample_idx)
      0, 1, 5, 6:       assert(qout == 32'h000000AA) else $fatal;
      2, 3, 4, 7, 8, 9: assert(qout == 32'h000000BB) else $fatal;
      default: $fatal;
    endcase
  end
end

initial begin
  wait(done == 1);
  #1;
  assert(sample_idx == 10) else $fatal;
  assert(qout == 32'h000000CC) else $fatal;
  assert(buffer_error == 0) else $fatal;
end

integer fh;

initial begin
  #120 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_10.ucdb");
  $finish;
end

endmodule: tb_st_10
