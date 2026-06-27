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
reg aso_ready;
reg asoA_ready;
reg [1:0] avs_s0_address;
reg avs_s0_read;
reg avs_s0_write;
wire [31:0] avs_s0_readdata;
reg [31:0] avs_s0_writedata;

reg saw_aso;
reg saw_asoA;
integer fh;

localparam [1:0] A_STATUS = 2'd0;
localparam [1:0] A_CONTROL = 2'd1;
localparam [1:0] A_OVERFLOW_COUNT = 2'd2;
localparam [1:0] A_OVERFLOWA_COUNT = 2'd3;

initial clk = 0;
always #0.5 clk = ~clk;

ts_core dut (
  .clk(clk),
  .reset(reset),
  .sig(sig),
  .sigA(sigA),
  .avs_s0_address(avs_s0_address),
  .avs_s0_read(avs_s0_read),
  .avs_s0_write(avs_s0_write),
  .avs_s0_readdata(avs_s0_readdata),
  .avs_s0_writedata(avs_s0_writedata),
  .aso_valid(aso_valid),
  .aso_data(aso_data),
  .aso_ready(aso_ready),
  .asoA_valid(asoA_valid),
  .asoA_data(asoA_data),
  .asoA_ready(asoA_ready)
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

task automatic mm_read(input logic [1:0] addr, output logic [31:0] data);
  begin
    @(negedge clk);
    avs_s0_address <= addr;
    avs_s0_read <= 1'b1;
    avs_s0_write <= 1'b0;
    @(posedge clk);
    #0.01;
    data = avs_s0_readdata;
    @(negedge clk);
    avs_s0_read <= 1'b0;
  end
endtask

task automatic mm_write(input logic [1:0] addr, input logic [31:0] data);
  begin
    @(negedge clk);
    avs_s0_address <= addr;
    avs_s0_writedata <= data;
    avs_s0_write <= 1'b1;
    avs_s0_read <= 1'b0;
    @(posedge clk);
    #0.01;
    @(negedge clk);
    avs_s0_write <= 1'b0;
    avs_s0_writedata <= '0;
  end
endtask

task automatic pulse_sig;
  begin
    @(negedge clk);
    sig <= 1'b1;
    repeat (2) @(negedge clk);
    sig <= 1'b0;
  end
endtask

task automatic pulse_sigA;
  begin
    @(negedge clk);
    sigA <= 1'b1;
    repeat (2) @(negedge clk);
    sigA <= 1'b0;
  end
endtask

initial begin
  logic [31:0] status;
  logic [31:0] count;
  logic [63:0] held;
  logic [63:0] heldA;

  reset <= 1;
  sig <= 0;
  sigA <= 0;
  aso_ready <= 1;
  asoA_ready <= 1;
  avs_s0_address <= '0;
  avs_s0_read <= 0;
  avs_s0_write <= 0;
  avs_s0_writedata <= '0;
  saw_aso <= 0;
  saw_asoA <= 0;
  repeat (4) @(posedge clk);
  reset <= 0;
  repeat (4) @(posedge clk);

  pulse_sig();
  wait (aso_valid);
  assert(aso_data != 0) else $fatal;
  @(posedge clk);
  #0.01;
  assert(!aso_valid) else $fatal;

  pulse_sigA();
  wait (asoA_valid);
  assert(asoA_data != 0) else $fatal;
  @(posedge clk);
  #0.01;
  assert(!asoA_valid) else $fatal;

  aso_ready <= 0;
  pulse_sig();
  wait (aso_valid);
  held = aso_data;
  repeat (3) @(posedge clk);
  #0.01;
  assert(aso_valid) else $fatal;
  assert(aso_data == held) else $fatal;
  mm_read(A_STATUS, status);
  assert(status[0]) else $fatal;
  assert(!status[8]) else $fatal;

  pulse_sig();
  repeat (6) @(posedge clk);
  mm_read(A_STATUS, status);
  assert(status[0]) else $fatal;
  assert(status[8]) else $fatal;
  mm_read(A_OVERFLOW_COUNT, count);
  assert(count == 1) else $fatal;
  assert(aso_data == held) else $fatal;

  aso_ready <= 1;
  @(posedge clk);
  #0.01;
  assert(!aso_valid) else $fatal;
  mm_write(A_CONTROL, 32'h1);
  mm_read(A_STATUS, status);
  assert(!status[8]) else $fatal;
  mm_read(A_OVERFLOW_COUNT, count);
  assert(count == 0) else $fatal;

  asoA_ready <= 0;
  pulse_sigA();
  wait (asoA_valid);
  heldA = asoA_data;
  pulse_sigA();
  repeat (6) @(posedge clk);
  mm_read(A_STATUS, status);
  assert(status[1]) else $fatal;
  assert(status[9]) else $fatal;
  mm_read(A_OVERFLOWA_COUNT, count);
  assert(count == 1) else $fatal;
  assert(asoA_data == heldA) else $fatal;

  asoA_ready <= 1;
  @(posedge clk);
  #0.01;
  assert(!asoA_valid) else $fatal;
  mm_write(A_CONTROL, 32'h2);
  mm_read(A_STATUS, status);
  assert(!status[9]) else $fatal;
  mm_read(A_OVERFLOWA_COUNT, count);
  assert(count == 0) else $fatal;

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
