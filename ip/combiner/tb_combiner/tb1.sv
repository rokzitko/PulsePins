// Direct functional testbench for the registered qout combiner.
//
// This testbench bypasses the Avalon-MM programming path and forces the internal `cfg`
// selector directly. The goal is to validate the pure datapath semantics of each supported
// mode with fixed, easy-to-check input vectors. Register-interface behavior is covered in
// `tb2.sv`.
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

// Trace a few internal datapath signals while stepping through the fixed functional cases.
always @(posedge clk) begin
  $strobe("t=%8.3f in1=%h in2=%h in3=%h in4=%h cfg=%d x1=%h y1=%h o=%h", $realtime, in1, in2, in3, in4, dut.cfg,
    dut.x1, dut.y1, o);
end

combiner dut(
 .clock_clk(clk),
 .clk(clk), // output domain
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
  force dut.cfg = dut.SEL2;
  #2;
  assert(o == 'hAA) else $fatal;

  #1;
  force dut.cfg = dut.SEL3;
  #2;
  assert(o == 'hBB) else $fatal;

  #1;
  force dut.cfg = dut.SEL4;
  #2;
  assert(o == 'hCC) else $fatal;

  // Logical combination modes.
  #1;
  force dut.cfg = dut.AND;
  #2;
  assert(o == 'h88) else $fatal;

  #1;
  force dut.cfg = dut.OR;
  #2;
  assert(o == 'hFF) else $fatal;

  #1;
  force dut.cfg = dut.XOR;
  #2;
  assert(o == 'h22) else $fatal;

  #1;
  force dut.cfg = dut.XNOR;
  #2;
  assert(o == 'hffffffdd) else $fatal;

  #1;
  force dut.cfg = dut.MAJ;
  #2;
  assert(o == 'hAA) else $fatal;

  // Arithmetic modes.
  #1;
  in1 <= 1;
  in2 <= 2;
  in3 <= 3;
  in4 <= 4;
  force dut.cfg = dut.SUM12;
  #2;
  assert(o == 'd3) else $fatal;

  #1;
  in1 <= 1;
  in2 <= 2;
  in3 <= 3;
  in4 <= 4;
  force dut.cfg = dut.SUM1234;
  #2;
  assert(o == 'd10) else $fatal;

  #1;
  in1 <= 20;
  in2 <= 10;
  in3 <= 3;
  in4 <= 4;
  force dut.cfg = dut.DIFF12;
  #2;
  assert(o == 'd10) else $fatal;

  // Block-composition modes.
  #1;
  in1 <= 32'h11223344;
  in2 <= 32'h55667788;
  in3 <= 32'haabbccdd;
  in4 <= 32'h11223344;
  force dut.cfg = dut.BLOCK8;
  #2;
  assert(o == 32'h44dd8844) else $fatal;

  #1;
  in1 <= 32'h11223344;
  in2 <= 32'h55667788;
  in3 <= 32'haabbccdd;
  in4 <= 32'h11223344;
  force dut.cfg = dut.BLOCK16;
  #2;
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
