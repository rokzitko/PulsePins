// Direct functional testbench for the combinational qout combiner.
//
// This testbench forces the internal `cfg` selector directly, so the assertions exercise only
// the pure combinational datapath semantics of each mode. Avalon-MM programming of the same
// modes is tested separately in `tb2.sv`.
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

localparam WIDTH = 32;
logic [WIDTH-1:0] in1;
logic [WIDTH-1:0] in2;
logic [WIDTH-1:0] in3;
logic [WIDTH-1:0] in4;
logic [WIDTH-1:0] o;

// Trace the selected mode and output while stepping through the deterministic functional cases.
always @(posedge clk) begin
  $strobe("t=%8.3f in1=%h in2=%h in3=%h in4=%h cfg=%d o=%h", $realtime, in1, in2, in3, in4, dut.cfg, o);
end

combiner_comb dut(
 .clock_clk(clk),
 .reset_reset(reset),
 .in1,
 .in2,
 .in3,
 .in4,
 .o
);

initial begin
  // Start from a fully idle input state so reset/default behavior is obvious.
  in1 <= 0;
  in2 <= 0;
  in3 <= 0;
  in4 <= 0;
  #1;

  // Basic selection modes.
  #2;
  in1 = 'hFF;
  #2;
  assert(o == 'hFF) else $fatal;

  // Additional inputs should not matter until the mode changes.
  #1;
  in2 = 'hAA;
  in3 = 'hBB;
  in4 = 'hCC;
  #0;
  assert(o == 'hFF) else $fatal;

  // Force `cfg` directly to isolate datapath behavior from bus programming.
  #1;
  force dut.cfg = dut.SEL2;
  #0;
  assert(o == 'hAA) else $fatal;

  #1;
  force dut.cfg = dut.SEL3;
  #0;
  assert(o == 'hBB) else $fatal;

  #1;
  force dut.cfg = dut.SEL4;
  #0;
  assert(o == 'hCC) else $fatal;

  // Logical combination modes.
  #1;
  force dut.cfg = dut.AND;
  #0;
  assert(o == 'h88) else $fatal;

  #1;
  force dut.cfg = dut.OR;
  #0;
  assert(o == 'hFF) else $fatal;

  #1;
  force dut.cfg = dut.XOR;
  #0;
  assert(o == 'h22) else $fatal;

  #1;
  force dut.cfg = dut.XNOR;
  #0;
  assert(o == 'hffffffdd) else $fatal;

  #1;
  force dut.cfg = dut.MAJ;
  #0;
  assert(o == 'hAA) else $fatal;

  // Arithmetic modes. These checks use short delays because the DUT is combinational.
  #1;
  in1 <= 1;
  in2 <= 2;
  in3 <= 3;
  in4 <= 4;
  force dut.cfg = dut.SUM12;
  #1;
  assert(o == 'd3) else $fatal;

  #1;
  in1 <= 1;
  in2 <= 2;
  in3 <= 3;
  in4 <= 4;
  force dut.cfg = dut.SUM1234;
  #1;
  assert(o == 'd10) else $fatal;

  #1;
  in1 <= 20;
  in2 <= 10;
  in3 <= 3;
  in4 <= 4;
  force dut.cfg = dut.DIFF12;
  #1;
  assert(o == 'd10) else $fatal;

  // Block-composition modes.
  #1;
  in1 <= 32'h11223344;
  in2 <= 32'h55667788;
  in3 <= 32'haabbccdd;
  in4 <= 32'h11223344;
  force dut.cfg = dut.BLOCK8;
  #1;
  assert(o == 32'h44dd8844) else $fatal;

  #1;
  in1 <= 32'h11223344;
  in2 <= 32'h55667788;
  in3 <= 32'haabbccdd;
  in4 <= 32'h11223344;
  force dut.cfg = dut.BLOCK16;
  #1;
  assert(o == 32'h77883344) else $fatal;
end

integer fh;

initial begin
  #20 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_combiner1.ucdb");
  $finish;
end

endmodule: tb_combiner1
