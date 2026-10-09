// Direct functional testbench for the registered trigger-word combiner.
//
// This testbench forces the internal `cfg` selector directly so the assertions focus on the
// trigger/control-word combination semantics rather than on the Avalon-MM programming path.
// The register-interface side is covered separately in `tb2.sv`.
// Rok Zitko, 2025

`default_nettype none

module tb_combiner1;

timeunit 1ns;
timeprecision 1ps;

logic clk;
logic reset;

// Simple 1 ns clock and short reset pulse used only to establish deterministic initial state.
initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #1 reset <= 0;
end

// Trigger combiners work on a narrower trigger/control word rather than on the full 32-bit qout bus.
localparam WIDTH = 11;
logic [WIDTH-1:0] in1;
logic [WIDTH-1:0] in2;
logic [WIDTH-1:0] in3;
logic [WIDTH-1:0] in4;
logic [WIDTH-1:0] o;
logic [31:0] avs_s0_readdata;
bit checker_done = 0;

// Trace a few internal datapath signals while stepping through the fixed functional cases.
always @(posedge clk) begin
  $strobe("t=%8.3f in1=%h in2=%h in3=%h in4=%h cfg=%d x1=%h y1=%h o=%h", $realtime, in1, in2, in3, in4, dut.cfg_clk,
    dut.x1, dut.y1, o);
end

combiner_trig dut(
 .clock_clk(clk),
 .clk(clk), // output domain
 .clk_reset(reset),
 .reset_reset(reset),
 .in1,
 .in2,
 .in3,
 .in4,
 .o,
 .avs_s0_address(4'b0),
 .avs_s0_read(1'b0),
 .avs_s0_readdata(avs_s0_readdata),
 .avs_s0_write(1'b0),
 .avs_s0_writedata(32'b0)
);

wire [3:0] cov_mode = dut.cfg_clk[3:0];
`include "../../combiner/tb_combiner/scenarios.svh"

initial begin
  // Start from a fully idle input state so reset/default behavior is obvious.
  in1 <= 0;
  in2 <= 0;
  in3 <= 0;
  in4 <= 0;
  #1;
  @(negedge clk);

  // Basic selection modes.
  #5;
  in1 = 'hFF;
  #2;
  assert(o == 'hFF) else $fatal;

  // Additional inputs should not matter until the mode changes.
  #1;
  in2 = 'hAA;
  in3 = 'hBB;
  in4 = 'hCC;
  #2;
  assert(o == 'hFF) else $fatal;

  // Force `cfg` directly to isolate datapath behavior from bus programming.
  #1;
  force dut.cfg_clk = dut.SEL2;
  #2;
  assert(o == 'hAA) else $fatal;

  #1;
  force dut.cfg_clk = dut.SEL3;
  #2;
  assert(o == 'hBB) else $fatal;

  #1;
  force dut.cfg_clk = dut.SEL4;
  #2;
  assert(o == 'hCC) else $fatal;

  // Logical combination modes reused for trigger/control words.
  #1;
  force dut.cfg_clk = dut.AND;
  #2;
  assert(o == 'h88) else $fatal;

  #1;
  force dut.cfg_clk = dut.OR;
  #2;
  assert(o == 'hFF) else $fatal;

  #1;
  force dut.cfg_clk = dut.XOR;
  #2;
  assert(o == 'h22) else $fatal;

  #1;
  force dut.cfg_clk = dut.XNOR;
  #2;
  assert(o == 11'h7dd) else $fatal;
  checker_done = 1;
end

integer fh;

initial begin
  wait(checker_done);
  @(negedge clk);
  for (int mode = 0; mode <= 7; mode++) tb_require_scenario($sformatf("selected_mode_%0d", mode), cov_modes[mode]);
  tb_require_scenario("directed_checks_completed", checker_done);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
`ifndef IVERILOG
  $set_coverage_db_name("run_combiner1.ucdb");
`endif
  $finish;
end

initial #200 $fatal(1, "timeout waiting for trigger combiner checks");

endmodule: tb_combiner1
