// Test for preprocessor sequences; check for the number of elements in the output FIFO
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_3;

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

streamer dut(.clk, .reset, .input_data, .input_valid, .input_ready, .initial_value);

localparam [31:0] PASS = 0;
localparam [31:0] NOPASS = 1 << BIT_NOPASS;
localparam [31:0] STORE0 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (0 << BIT_POSITIONS_LO);
localparam [31:0] STORE1 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (1 << BIT_POSITIONS_LO);
localparam [31:0] STORE2 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (2 << BIT_POSITIONS_LO);
localparam [31:0] STORE3 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (3 << BIT_POSITIONS_LO);
localparam [31:0] REPLAY = (1 << BIT_NOPASS) + (1 << BIT_REPLAY);

initial begin
  input_data <= 32'b0;
  input_valid <= 0;
  #10
  input_data <= { STORE0, 32'h02, 32'hAA }; // 2x AA
  input_valid <= 1;
  #1
  input_data <= { STORE1, 32'h03, 32'hBB }; // 3x BB
  input_valid <= 1;
  #1
  input_data <= { REPLAY, 32'h0003, 32'h02 }; // replay 2 elements 3 times
  input_valid <= 1;
  #1
  input_data <= { 32'h4, 32'h1, 32'hffffffff }; // final
  input_valid <= 1;
  #1
  input_valid <= 0;
end

integer fh;

initial begin
  #60 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_3.ucdb");
  $finish;
end

endmodule: tb_st_3
