// Three-step triggering
// Rok Zitko, 2025

`include "../config.vh"

`default_nettype none

module tb_chain;

timeunit 1ns;
timeprecision 1ps;

logic clk;
logic reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #2 reset <= 0;
end

parameter int width = `WIDTH_TRIGGER;
parameter int width_control = `WIDTH_TRIGGER_CONTROL;

wire wrclk = clk;
logic [width-1:0] i;
logic [width-1:0] pattern;
logic [width-1:0] mask;
logic [width_control-1:0] control;
logic wrreq;
logic trigger_enable;
logic trigger_force;
logic trigger_reset;
logic retrig;
logic armed;
logic o;

chain_trigger dut(
 .clk,
 .wrclk,
 .reset,
 .rst(reset),
 .i,
 .pattern,
 .mask,
 .control,
 .wrreq,
 .trigger_enable,
 .trigger_force,
 .trigger_reset,
 .retrig,
 .armed,
 .o);

always @(posedge clk) begin
  $strobe("t=%8.3f i=%3b o=%b used=%d state=%d rdreq=%b one=%b | pat=%3b mask=%3b",
   $realtime, i, o, dut.used, dut.state, dut.rdreq, dut.one,
   dut.at.pattern, dut.at.mask
   );
end

// Setup
initial begin
  i <= 0;
  pattern <= 0;
  mask <= 0;
  control <= 0;
  wrreq <= 0;
  trigger_enable <= 1;
  trigger_force <= 0;
  trigger_reset <= 0;
  retrig <= 0;

  #5;

  wrreq   <= 1;
  pattern <= 'b001;
  mask    <= 'b001;
  control <= 'b001;
  #1;
  wrreq   <= 1;
  pattern <= 'b010;
  mask    <= 'b010;
  control <= 'b001;
  #1;
  wrreq   <= 1;
  pattern <= 'b100;
  mask    <= 'b100;
  control <= 'b011; // final
  #1
  wrreq   <= 0;
end

// Triggers
initial begin
  #20.5
  i <= 'b001;
  #10
  i <= 'b010;
  #10
  i <= 'b100;
end

// Trigger reset
initial begin
  #50
  trigger_reset <= 1;
end

// Tests
initial begin
  #15.1
  assert(o == 0 && dut.used == 2 && dut.state == 2 && dut.one == 0) else $fatal;
end

initial begin
  #21.1
  assert(o == 0 && dut.used == 2 && dut.state == 2 && dut.one == 1) else $fatal;
end

initial begin
  #22.1
  assert(o == 0 && dut.used == 2 && dut.state == 1 && dut.one == 1) else $fatal;
  assert(dut.rdreq == 1) else $fatal;
end

initial begin
  #32.1
  assert(o == 0 && dut.used == 1 && dut.state == 1 && dut.one == 1) else $fatal;
  assert(dut.rdreq == 1) else $fatal;
end

initial begin
  #32.1
  assert(o == 0 && dut.used == 1 && dut.state == 1 && dut.one == 1) else $fatal;
  assert(dut.rdreq == 1) else $fatal;
end

initial begin
  #42.1
  assert(o == 1 && dut.used == 0 && dut.state == 3 && dut.one == 1) else $fatal;
  assert(dut.rdreq == 0) else $fatal;
end

initial begin
  #51.1
  assert(o == 0) else $fatal;
end

integer fh;

initial begin
  #60 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_chain.ucdb");
  $finish;
end

endmodule: tb_chain
