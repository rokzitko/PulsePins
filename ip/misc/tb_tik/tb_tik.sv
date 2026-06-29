// Purpose: timing helper / tick-generation testbench.
//
// Exercises the small timing utility block used by other low-level support modules.
`timescale 1ns/1ps
`default_nettype none

module hello_world;

reg reset;
reg clk;
wire tik;
integer cycle;
integer tik_count;
reg last_tik;

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

  tik_count = 0;
  last_tik = 0;
  reset <= 1;
  #1;
  reset <= 0;

  for (cycle = 0; cycle < 30; cycle = cycle + 1) begin
    @(posedge clk);
    #0.01;
    if (tik && last_tik) $fatal(1, "tik wider than one cycle");
    if (tik) tik_count = tik_count + 1;
    last_tik = tik;
  end

  if (tik_count != 3) $fatal(1, "tik count mismatch: %0d", tik_count);

  $display("PASS");
  $finish;
end

endmodule: hello_world
