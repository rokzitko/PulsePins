// Purpose: pulse/tick generator testbench.
//
// Verifies the periodic pulse outputs used as shared low-rate timing utilities elsewhere in the
// design.
`timescale 1ns/1ps
`default_nettype none

module hello_world;

reg reset;
reg clk;

initial clk = 0;
always #0.5 clk = ~clk;

wire pulse_1ms;
wire pulse_1s;

pulse_gen_timebase #( .CLK_FREQ_HZ(10_000) ) dut (
 .clk(clk),
 .rst(reset),
 .pulse_1ms(pulse_1ms),
 .pulse_1s(pulse_1s)
);

always @(posedge clk) begin
  #0.01;
  $display("t=%t reset=%b 1ms=%b 1s=%b",
    $time, reset, pulse_1ms, pulse_1s);
end

initial begin
  $timeformat(-9, 2, " ns", 20);
  $display("Hello world");

  reset <= 1;
  #1;
  reset <= 0;

  #10000 $finish;
end

endmodule: hello_world
