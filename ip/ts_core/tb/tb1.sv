// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`default_nettype none

module hello_world;

reg reset;
reg clk;
reg signal;
wire [63:0] aso_data;
wire aso_valid;

initial clk = 0;
always #0.5 clk = ~clk;

ts_core dut (
  .clk(clk),
  .reset,
  .signal,
  .aso_data,
  .aso_valid
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b signal=%b aso_valid=%b ctr=%d aso_data=%d",
    $time, reset, signal, aso_valid, dut.ctr, aso_data);
end

initial begin
  reset <= 1;
  signal <= 0;
  #2;
  reset <= 0;
  #12;
  signal <= 1;

  #2;
  //assert(result == 3) else $fatal;

  #20 $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000 $finish;
end

endmodule: hello_world
