// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`default_nettype none

module hello_world;

reg reset;
reg clk;

initial clk = 0;
always #0.5 clk = ~clk;

reg high_low;
reg d, valid;
reg d_prev;
reg start_async, stop_async;
wire ready;
wire [31:0] result;

time_counter dut (
  .clk,
  .reset,
  .start_async,
  .stop_async,
  .high_low,
  .result,
  .ready
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b d=%b valid=%b start=%b stop=%b counter=%4d elapsed=%4d ready=%b result=%4d",
    $time, reset, d, valid, start_async, stop_async, dut.counter, dut.elapsed, ready, result);
end

always @(posedge clk) begin
  if (reset) begin
    d_prev <= 0;
  end else begin
    d_prev <= d;
  end
end

always @(posedge clk) begin
  if (reset) begin
    start_async <= 0;
    stop_async <= 0;
  end else begin
    start_async <= d && !d_prev;
    stop_async <= !d && d_prev;
  end
end

initial begin
  reset <= 1;
  high_low <= 0;
  #2;
  reset <= 0;
  #1;
  // 0011110

  valid <= 1;
  d <= 0;
  #1;
  d <= 0;
  #1;
  d <= 1;
  #1;
  d <= 1;
  #1;
  d <= 1;
  #1;
  d <= 0;
  #10;

  #1;
  assert(result == 3) else $fatal;

  #1 $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000 $finish;
end

endmodule: hello_world
