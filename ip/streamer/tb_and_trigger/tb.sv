// Simple simulation test bench
// Rok Zitko, 2025

`include "../config.vh"

`default_nettype none

module tb_and;

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

logic [`WIDTH_TRIGGER-1:0] i;
logic [`WIDTH_TRIGGER-1:0] pattern;
logic [`WIDTH_TRIGGER-1:0] mask;
logic trigger_enable;
logic o;

always @(posedge clk) begin
  $strobe("t=%8.3f reset=%b trigger_enable=%b i=%b o=%b", $realtime, reset, trigger_enable, i, o);
end

and_trigger dut(.i(i), .pattern(pattern), .mask(mask), .clk(clk), .reset(reset), .trigger_enable(trigger_enable), .o(o));

initial begin
  i       <= 8'b00000000;
  pattern <= 8'b00000001;
  mask    <= 8'b00000001;
  trigger_enable <= 0;
  #2 trigger_enable <= 1;
  #1;
  assert(o == 1'b0) else $fatal;
  #3.5 i <= 8'hFF;
  $display("Triggered at t=%0.3f", $realtime);
  #0 assert(o == 1'b0) else $fatal; // before clock tick
  @(posedge clk);
  #1step assert(o == 1'b1) else $fatal; // after clock tick
end

integer fh;

initial begin
  #10 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_and.ucdb");
  $finish;
end

endmodule: tb_and
