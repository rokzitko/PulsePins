// Purpose: software-visible wrapper test for the readback encoder interface.
//
// Checks that the Avalon-ST/Avalon-MM wrapper around the encoder core exposes the expected
// transport and status behavior.
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
reg qin_strobe;

wire [cfg::WIDTH_TOTAL-1:0] aso_data;
wire aso_valid;
reg aso_ready;

reg [1:0] avs_s0_address;
reg avs_s0_read;
reg avs_s0_write;
wire [cfg::WIDTH_AVS-1:0] avs_s0_readdata;
reg [cfg::WIDTH_AVS-1:0] avs_s0_writedata;

rl_encoder_if dut(
  .clk, .reset,
  .aso_data, .aso_valid, .aso_ready,
  .avs_s0_address, .avs_s0_read, .avs_s0_write, .avs_s0_readdata, .avs_s0_writedata,
  .qin, .qin_valid, .qin_strobe, .qin_clk
);

initial begin
  qin <= 32'b0;
  qin_valid <= 0;
  qin_strobe <= 0;
  aso_ready <= 0;
  avs_s0_address <= '0;
  avs_s0_read <= 0;
  avs_s0_write <= 0;
  avs_s0_writedata <= '0;
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

initial begin
  wait (aso_valid == 1'b1);
  repeat (3) begin
    @(posedge clk);
    assert(aso_valid == 1'b1) else $fatal;
    assert(dut.rdreq == 1'b0) else $fatal;
  end
  aso_ready <= 1'b1;
  #1step assert(dut.rdreq == 1'b1) else $fatal;
end

integer fh;

initial begin
  #50 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $finish;
end

endmodule: tb
