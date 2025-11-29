// Load 3 elements in output FIFO
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_1;

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
  $strobe("t=%8.3f ctr=%h data=%h rdreq_i=%b empty_i=%b in_v_d=%b curr_value=%h curr_cnt=%d out_data=%h out_wrreq=%b in_rdreq=%b used_o=%d",
    $realtime, dut.counter, dut.data, dut.rdreq_i, dut.empty_i, dut.in_valid_data, 
    dut.rl0.curr_value, dut.rl0.curr_cnt,
    dut.rl0.out_data, dut.rl0.out_wrreq, dut.rl0.in_rdreq, dut.used_o
);
end

streamer dut(.clk, .reset, .input_data, .input_valid, .input_ready, .initial_value);

initial begin
  input_data <= 32'b0;
  input_valid <= 0;
  #10
  input_data <= { 32'h0, 32'h6, 32'h12345678 };
  input_valid <= 1;
  #1
  input_data <= { 32'h0, 32'h7, 32'hababcdcd };
  input_valid <= 1;
  #1
  input_data <= { 32'h0, 32'h8, 32'h56781234 };
  input_valid <= 1;
  #1
  input_data <= { 32'h4, 32'h1, 32'hffffffff };
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
  $set_coverage_db_name("run_st_1.ucdb");
  $finish;
end

endmodule: tb_st_1
