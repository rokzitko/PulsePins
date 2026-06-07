// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Purpose: timestamp-capture core testbench.
//
// Verifies that asynchronous timing events are captured into timestamp records on the expected
// output path.

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`default_nettype none

module hello_world;

reg reset;
reg clk;
reg sig;
reg sigA;
wire [63:0] aso_data;
wire aso_valid;
wire [63:0] asoA_data;
wire asoA_valid;

reg saw_aso;
reg saw_asoA;
integer fh;

initial clk = 0;
always #0.5 clk = ~clk;

ts_core dut (
  .clk(clk),
  .reset(reset),
  .sig(sig),
  .sigA(sigA),
  .aso_valid(aso_valid),
  .aso_data(aso_data),
  .aso_ready(1'b1),
  .asoA_valid(asoA_valid),
  .asoA_data(asoA_data),
  .asoA_ready(1'b1)
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b sig=%b sigA=%b aso_valid=%b asoA_valid=%b ctr=%d aso_data=%d asoA_data=%d",
    $time, reset, sig, sigA, aso_valid, asoA_valid, dut.ctr, aso_data, asoA_data);
end

always @(posedge clk) begin
  if (aso_valid) begin
    assert(aso_data != 0) else $fatal;
    saw_aso <= 1'b1;
  end

  if (asoA_valid) begin
    assert(asoA_data != 0) else $fatal;
    saw_asoA <= 1'b1;
  end
end

initial begin
  reset <= 1;
  sig <= 0;
  sigA <= 0;
  saw_aso <= 0;
  saw_asoA <= 0;
  #2;
  reset <= 0;
  #12;
  sig <= 1;

  #2;
  sig <= 0;

  #8;
  sigA <= 1;

  #2;
  sigA <= 0;

  #20;
  assert(saw_aso) else $fatal;
  assert(saw_asoA) else $fatal;
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $finish;
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  #1000 $finish;
end

endmodule: hello_world
