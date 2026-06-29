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
integer cycle;
integer pulse_1ms_count;
integer pulse_1s_count;
reg last_pulse_1ms;
reg last_pulse_1s;

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

  pulse_1ms_count = 0;
  pulse_1s_count = 0;
  last_pulse_1ms = 0;
  last_pulse_1s = 0;
  reset <= 1;
  #1;
  reset <= 0;

  for (cycle = 0; cycle < 10000; cycle = cycle + 1) begin
    @(posedge clk);
    #0.01;
    if (pulse_1ms && last_pulse_1ms) $fatal(1, "pulse_1ms wider than one cycle");
    if (pulse_1s && last_pulse_1s) $fatal(1, "pulse_1s wider than one cycle");
    if (pulse_1ms) pulse_1ms_count = pulse_1ms_count + 1;
    if (pulse_1s) pulse_1s_count = pulse_1s_count + 1;
    last_pulse_1ms = pulse_1ms;
    last_pulse_1s = pulse_1s;
  end

  if (pulse_1ms_count != 1000) $fatal(1, "pulse_1ms count mismatch: %0d", pulse_1ms_count);
  if (pulse_1s_count != 1) $fatal(1, "pulse_1s count mismatch: %0d", pulse_1s_count);

  $display("PASS");
  $finish;
end

endmodule: hello_world
