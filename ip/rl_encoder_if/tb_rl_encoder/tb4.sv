// Case of retriggering (similar to no_strobe)
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
reg [cfg::WIDTH_DATA-1:0] data;
reg valid;
wire data_clk = clk;

always @(posedge clk) begin
  $strobe("t=%8.3f data=%h valid=%b run_value=%4h run_count=%4d j=%h wrreq=%b",
    $realtime, data, valid, dut.run_value, dut.run_count, dut.j, dut.wrreq
  );
end

rl_encoder dut(.clk, .reset, .data, .valid, .data_clk, .mode(1));

initial begin
  data <= 32'b0;
  valid <= 0;
  #10
  data <= { 32'h11 };
  valid <= 1;
  #1
  data <= { 32'h11 };
  valid <= 1;
  #1
  data <= { 32'h11 };
  valid <= 1;
  #1
  data <= { 32'hAA };
  valid <= 0;
  #1
  data <= { 32'hAA };
  valid <= 0;
  #1
  data <= { 32'hAA };
  valid <= 0;
  #1
  data <= { 32'h22 };
  valid <= 1;
  #1
  data <= { 32'h22 };
  valid <= 1;
  #1
  data <= { 32'hFF };
  valid <= 0;
end

initial begin
   wait (dut.wrreq == 1);
   @(posedge clk);
   assert(dut.j == 'h0000000300000011) else $fatal;
   #1;

   wait (dut.wrreq == 1);
   @(posedge clk);
   assert(dut.j == 'h0000000200000022) else $fatal;
end

integer fh;

initial begin
  #30 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $finish;
end

endmodule: tb
