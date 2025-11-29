// Simple simulation test bench
// Rok Zitko, 2025

`default_nettype none

module tb_mux1;

timeunit 1ns;
timeprecision 1ps;

logic clk;
logic reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #1 reset <= 0;
end

always @(posedge clk) begin
  $strobe("t=%8.3f channel=%b asi_ready1=%b asi_valid1=%b ctr1=%d ctr2=%d", $realtime, dut.channel, dut.asi_ready1, dut.asi_valid1, dut.ctr1, dut.ctr2);
end

logic [31:0] asi_data1;
logic asi_valid1;
logic asi_ready1;
logic [31:0] asi_data2;
logic asi_valid2;
logic asi_ready2;

logic aso_ready;
logic [31:0] aso_data;
logic aso_valid;
logic aso_channel;

logic [31:0] avs_s0_readdata;
logic [31:0] avs_s0_writedata;
logic [2:0] avs_s0_address;
logic avs_s0_write;
logic avs_s0_read;

st_mux_if dut(
 .clk,
 .reset,
 .asi_data1,
 .asi_valid1,
 .asi_ready1,
 .asi_data2,
 .asi_valid2,
 .asi_ready2,
 .aso_ready,
 .aso_data,
 .aso_valid,
 .aso_channel,
 .avs_s0_readdata,
 .avs_s0_writedata,
 .avs_s0_address,
 .avs_s0_write,
 .avs_s0_read
);

initial begin
  asi_ready1 <= 1;
  aso_ready <= 1;

  #1;

  asi_data1 <= 1;
  asi_valid1 <= 1;
  #10;
  asi_valid1 <= 0;
  #5;

  assert(aso_data == 1) else $fatal;

  asi_data2 <= 2;
  asi_valid2 <= 1;
  #10;
  asi_valid2 <= 0;
  #5;

  assert(dut.ctr1 == 10) else $fatal;
  assert(dut.ctr2 == 0) else $fatal;

  avs_s0_address <= 0;
  avs_s0_writedata <= 1;
  avs_s0_write <= 1;
  #1;
  avs_s0_write <= 0;

  asi_data2 <= 2;
  asi_valid2 <= 1;
  #10;
  asi_valid2 <= 0;
  #5;

  assert(dut.ctr1 == 10) else $fatal;
  assert(dut.ctr2 == 10) else $fatal;
end

integer fh;

initial begin
  #60 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_mux1.ucdb");
  $finish;
end

endmodule: tb_mux1
