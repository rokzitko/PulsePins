// Purpose: timing helper / tick-generation testbench.
//
// Exercises the small timing utility block used by other low-level support modules.
`timescale 1ns/1ps
`default_nettype none

module hello_world;

reg reset;
reg clk;
wire tik;

initial clk = 0;
always #0.5 clk = ~clk;

tik #( .PERIOD(10) ) dut (
 .clk(clk),
 .reset(reset),
 .tik(tik)
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b counter=%d tik=%b",
    $time, reset, dut.counter, tik);
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");
  $display("width=%d", dut.WIDTH);

  reset <= 1;
  #1;
  reset <= 0;

  #20 $finish;
end

endmodule: hello_world
