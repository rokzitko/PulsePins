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
  // Generate random activity
  repeat (1000) begin
    #1;
    i1  = $urandom_range(0, 1);
    i2  = $urandom_range(0, 1);
    sel = $urandom_range(0, 1);
  end
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");

  reset <= 1;
  #1;
  reset <= 0;

  #1050 $finish;
end

endmodule: hello_world

