// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../rl_config.vh"

`default_nettype none

module tb;

reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

// Interface
reg [cfg::WIDTH_DATA-1:0] qin;
reg qin_valid;
wire qin_clk = clk;

rl_encoder_if dut(.clk, .reset, .qin, .qin_valid, .qin_clk);

initial begin
  qin <= 32'b0;
  qin_valid <= 0;
  #10
  qin <= { 32'h11 };
  qin_valid <= 1;
  #1
  qin <= { 32'h11 };
  qin_valid <= 1;
  #1
  qin <= { 32'h11 };
  qin_valid <= 1;
  #1
  qin <= { 32'h22 };
  qin_valid <= 1;
  #1
  qin <= { 32'h22 };
  qin_valid <= 1;
  #1
  qin <= { 32'h22 };
  qin_valid <= 1;
  #1
  qin <= { 32'h22 };
  qin_valid <= 1;
  #1
  qin_valid <= 0;
end

integer fh;

initial begin
  #30 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $finish;
end

endmodule: tb
