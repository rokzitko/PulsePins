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
reg [`WIDTH_TOTAL-1:0] input_data;
reg input_valid;
wire input_ready;

wire [`WIDTH_DATA-1:0] initial_value = 0;

always @(posedge clk) begin
  $strobe("t=%8.3f control=%h ctr=%h data=%h rdreq_i=%b empty_i=%b in_v_d=%b in_v_c=%b p=%b m=%b c=%b wrreq=%b used=%d fifo_empty=%b state=%d armed=%b activated=%b used_o=%d",
    $realtime, dut.control, dut.counter, dut.data, dut.rdreq_i, dut.empty_i, dut.in_valid_data, dut.in_valid_chain,
    dut.ct0.pattern, dut.ct0.mask, dut.ct0.control, dut.ct0.wrreq, dut.ct0.used, dut.ct0.fifo_empty, dut.ct0.state,
    dut.trigger_armed, dut.trigger_activated, dut.used_o
);
end

streamer dut(.clk, .reset, .input_data, .input_valid, .input_ready, .initial_value,
  .trigger_enable(1'b1),
  .trigger_force(1'b0),
  .trigger_reset(1'b0),
  .streamer_clk(clk) // required for the offloading elements from trigger queue
);

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
  #5;
end

integer fh;

initial begin
  #50 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_2.ucdb");
  $finish;
end

endmodule: tb_st_2
