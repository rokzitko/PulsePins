// Purpose: signal-multiplexer helper testbench.
//
// Verifies the small reusable mux block used in miscellaneous routing situations.
`timescale 1ns/1ps
`default_nettype none

module hello_world;

reg reset;
reg clk;

initial clk = 0;
always #0.5 clk = ~clk;

reg i1, i2;
wire o;
reg [0:0] sel;
integer inputs;
integer s;

sig_mux #( .INPUTS(2) ) dut (
 .i( {i2, i1} ),
 .o,
 .sel
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b i1=%b i2=%b sel=%b o=%b",
    $time, reset, i1, i2, sel, o);
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");

  i1 = 0;
  i2 = 0;
  sel = 0;
  reset <= 1;
  #1;
  reset <= 0;

  for (inputs = 0; inputs < 4; inputs = inputs + 1) begin
    {i2, i1} = inputs[1:0];
    for (s = 0; s < 2; s = s + 1) begin
      sel = s[0:0];
      #1;
      if (o !== (s == 0 ? i1 : i2)) begin
        $fatal(1, "mux mismatch inputs=%b sel=%0d got=%b", {i2, i1}, s, o);
      end
    end
  end

  $display("PASS");
  $finish;
end

endmodule: hello_world
