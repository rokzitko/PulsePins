// Test Avalon MM interface
// Rok Zitko, 2025

`default_nettype none

module tb_combiner2;

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

localparam WIDTH = 32;
logic [WIDTH-1:0] in1;
logic [WIDTH-1:0] in2;
logic [WIDTH-1:0] in3;
logic [WIDTH-1:0] in4;
logic [WIDTH-1:0] o;

logic [3:0] avs_s0_address;
logic avs_s0_read;
logic [31:0] avs_s0_readdata;
logic avs_s0_write;
logic [31:0] avs_s0_writedata;

always @(posedge clk) begin
  $strobe("t=%8.3f in1=%h in2=%h in3=%h in4=%h cfg=%d o=%h", $realtime, in1, in2, in3, in4, dut.cfg, o);
end

combiner dut(
 .clock_clk(clk),
 .clk(clk),
 .reset_reset(reset),
 .in1,
 .in2,
 .in3,
 .in4,
 .o,
 .avs_s0_address,
 .avs_s0_read,
 .avs_s0_readdata,
 .avs_s0_write,
 .avs_s0_writedata
);

task set_cfg(input logic [31:0] cfg);
  $display("cfg=%h", cfg);
  avs_s0_address = 0;
  avs_s0_writedata = cfg;
  avs_s0_write = 1;
  #1;
  avs_s0_write = 0;
  #2;
endtask

task testit;
  in1 = $urandom();
  in2 = $urandom();
  in3 = $urandom();
  in4 = $urandom();
  #1;

  #1;
  set_cfg(dut.SEL1);
  assert(o == in1) else $fatal;

  #1;
  set_cfg(dut.SEL2);
  assert(o == in2) else $fatal;

  #1;
  set_cfg(dut.SEL3);
  assert(o == in3) else $fatal;

  #1;
  set_cfg(dut.SEL4);
  assert(o == in4) else $fatal;

  #1;
  set_cfg(dut.AND);
  assert(o == (in1 & in2 & in3 & in4)) else $fatal;

  #1;
  set_cfg(dut.OR);
  assert(o == (in1 | in2 | in3 | in4)) else $fatal;

  #1;
  set_cfg(dut.XOR);
  assert(o == (in1 ^ in2 ^ in3 ^ in4)) else $fatal;

  #1;
  set_cfg(dut.XNOR);
  assert(o == (in1 ^~ in2 ^~ in3 ^~ in4)) else $fatal;

  #1;
  set_cfg(dut.BLOCK8);
  assert(o == {in4[7:0], in3[7:0], in2[7:0], in1[7:0]}) else $fatal;

  #1;
  set_cfg(dut.BLOCK16);
  assert(o == {in2[15:0], in1[15:0]}) else $fatal;

  #1;
  set_cfg(dut.SUM12);
  assert(o == (in1+in2)) else $fatal;

  #1;
  set_cfg(dut.SUM1234);
  assert(o == (in1+in2+in3+in4)) else $fatal;

  #1;
  set_cfg(dut.DIFF12);
  assert(o == (in1-in2)) else $fatal;

  #1;
  set_cfg('hF);
  assert(o == 0) else $fatal;
endtask

int i;

initial begin
#5;
  for (int i = 0; i < 500; i = i+1) begin
    testit();
  end
end

integer fh;

initial begin
  #10000 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_combiner2.ucdb");
  $finish;
end

endmodule: tb_combiner2
