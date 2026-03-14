// Retriggering test
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_5;

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

always @(posedge clk) begin
  $strobe("t=%8.3f q_p=%b q_m=%b usd=%d fifoemp=%b st=%d armd=%b activ'd=%b used_o=%d dn=%b retrigreq=%b retrig=%b qo=%h va=%b",
    $realtime,
    dut.ct0.q_pattern[2:0], dut.ct0.q_mask[2:0], dut.ct0.used, dut.ct0.fifo_empty, dut.ct0.state,
    dut.trigger_armed, dut.trigger_activated, dut.fifo0.used, dut.done, dut.retrig_requested, dut.retrig,
    dut.fifo0.qout, dut.fifo0.qout_valid
);
end

reg [WIDTH_TRIGGER-1:0] trigger_in;

streamer dut(.clk, .reset, .input_data, .input_valid, .input_ready,
  .initial_value('0),
  .gate_enable('1),           // must be 1, because rdreq = trigger_activated && gate_enable
  .stop_on_buffer_error('0),  // must be set to 0 or 1
  .stop('0),                  // must be 0
  .trigger_enable(1'b1),
  .trigger_force(1'b0),
  .trigger_reset(1'b0),
  .trigger_in,
  .streamer_clk(clk) // required for the offloading elements from trigger queue
);

initial begin
  input_data <= 32'b0;
  input_valid <= 0;

  #10
  input_data <= { 32'b0011, 32'b0, 32'b00000001_00000001 }; // trigger: 'b11 (trigger, final), mask, pattern
  input_valid <= 1;
  #1
  input_data <= { 32'h0, 32'h6, 32'h12345678 }; // 6 elements
  input_valid <= 1;
  #1
  input_data <= { 32'b1_0000_0000_0000_0000, 32'h1, 32'hffffffff }; // retrig
  input_valid <= 1;
  #1
  input_data <= { 32'b0011, 32'b0, 32'b00000010_00000010 }; // trigger: 'b11 (trigger, final), mask, pattern
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

  // First trigger at time 60
  #60;
  trigger_in <= 'b01;

  // Second trigger at time 80
  #20;
  trigger_in <= 'b10;
  #1;
end

integer fh;

initial begin
  #100 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_5.ucdb");
  $finish;
end

endmodule: tb_st_5
