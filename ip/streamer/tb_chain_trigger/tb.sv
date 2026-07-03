// Purpose: directed multi-stage trigger-program test.
//
// Loads a small trigger program, advances through the stages with matching input patterns, and
// checks final trigger assertion and trigger-reset behavior in the chain trigger engine.
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

parameter int width = WIDTH_TRIGGER;
parameter int width_control = WIDTH_TRIGGER_CONTROL;

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
logic wrfull;
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
 .wrfull,
 .o);

always @(posedge clk) begin
  $strobe("t=%8.3f i=%3b o=%b used=%d state=%d active=%b rdreq=%b one=%b | pat=%3b mask=%3b",
   $realtime, i, o, dut.used, dut.state, dut.active_stage_valid, dut.rdreq, dut.one,
   dut.at.pattern, dut.at.mask
   );
end

localparam logic [1:0] TB_S_IDLE = 0, TB_S_LOAD = 1, TB_S_WAIT = 2, TB_S_TRIGGERED = 3;

task automatic wait_state(input logic [1:0] expected);
  int n;
  begin
    n = 0;
    while (dut.state != expected && n < 100) begin
      @(posedge clk);
      #0.1;
      n++;
    end
    assert(dut.state == expected) else $fatal(1, "Timed out waiting for state %0d, got %0d", expected, dut.state);
  end
endtask

task automatic expect_stage(input logic [width-1:0] expected_pattern, input logic [width-1:0] expected_mask);
  begin
    #0.1;
    assert(dut.state == TB_S_WAIT && armed) else $fatal(1, "Trigger chain is not armed");
    assert(dut.active_stage_valid) else $fatal(1, "Active trigger stage is not marked valid");
    assert(dut.q_pattern == expected_pattern && dut.q_mask == expected_mask)
      else $fatal(1, "Expected active stage pattern=%b mask=%b, got pattern=%b mask=%b",
        expected_pattern, expected_mask, dut.q_pattern, dut.q_mask);
  end
endtask

task automatic pulse_trigger_reset(input logic [width-1:0] expected_pattern, input logic [width-1:0] expected_mask);
  begin
    @(negedge clk);
    trigger_reset <= 1;
    @(posedge clk);
    #0.1;
    assert(dut.state == TB_S_IDLE && !o) else $fatal(1, "trigger_reset did not deassert the trigger");
    assert(dut.active_stage_valid) else $fatal(1, "trigger_reset discarded the active trigger stage");
    assert(dut.q_pattern == expected_pattern && dut.q_mask == expected_mask)
      else $fatal(1, "trigger_reset changed active stage pattern=%b mask=%b", dut.q_pattern, dut.q_mask);
    @(negedge clk);
    trigger_reset <= 0;
    wait_state(TB_S_WAIT);
    expect_stage(expected_pattern, expected_mask);
  end
endtask

task automatic pulse_retrig;
  begin
    @(negedge clk);
    retrig <= 1;
    @(posedge clk);
    #0.1;
    assert(dut.state == TB_S_IDLE && !o) else $fatal(1, "retrig did not deassert the trigger");
    assert(!dut.active_stage_valid) else $fatal(1, "retrig preserved the active trigger stage");
    @(negedge clk);
    retrig <= 0;
    repeat (4) begin
      @(posedge clk);
      #0.1;
    end
    assert(dut.state == TB_S_IDLE && !armed && !o) else $fatal(1, "retrig re-armed without another queued stage");
  end
endtask

integer fh;

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
  #1;
  wrreq   <= 0;

  wait_state(TB_S_WAIT);
  expect_stage('b001, 'b001);

  i <= 'b001;
  wait_state(TB_S_LOAD);
  wait_state(TB_S_WAIT);
  expect_stage('b010, 'b010);
  i <= 0;

  pulse_trigger_reset('b010, 'b010);

  i <= 'b010;
  wait_state(TB_S_LOAD);
  pulse_trigger_reset('b100, 'b100);

  i <= 'b100;
  wait_state(TB_S_TRIGGERED);
  assert(o && dut.active_stage_valid) else $fatal(1, "Final trigger stage did not trigger");

  pulse_trigger_reset('b100, 'b100);
  wait_state(TB_S_TRIGGERED);
  assert(o && dut.active_stage_valid) else $fatal(1, "Final trigger stage did not re-trigger after trigger_reset");

  pulse_retrig();

  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_chain.ucdb");
  $finish;
end

endmodule: tb_chain
