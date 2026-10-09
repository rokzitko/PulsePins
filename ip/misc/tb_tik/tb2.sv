// Exact tick phase/spacing around powers of two, including PERIOD=1 and reset.
`timescale 1ns/1ps
module tb_tick_boundaries;
  logic clk = 0, reset = 1;
  always #0.5 clk = ~clk;
  wire [6:0] output_ticks;
  wire [6:0] coverage_complete;
  for (genvar g = 0; g < 7; g++) begin : periods
    localparam int P = g < 3 ? g+1 : g == 3 ? 7 : g == 4 ? 8 : g == 5 ? 9 : 16;
    tik #(.PERIOD(P)) dut(.clk, .reset, .tik(output_ticks[g]));
    int age = 0, last_tick = -1, checked_ticks = 0;
    always @(posedge clk) begin
      if (reset) begin age = 0; last_tick = -1; end
      else age++;
      #1ps;
      assert(output_ticks[g] === (age % P == P-1)) else $fatal(1, "tick period %0d phase mismatch at %0d", P, age);
      if (!reset && output_ticks[g]) begin
        if (last_tick >= 0) assert(age-last_tick == P) else $fatal(1, "tick period %0d spacing mismatch", P);
        last_tick = age; checked_ticks++;
      end
    end
    assign coverage_complete[g] = checked_ticks >= 8;
  end
  initial begin
    repeat (3) @(negedge clk); reset = 0;
    repeat (83) @(negedge clk); reset = 1;
    repeat (3) @(negedge clk); reset = 0;
    repeat (83) @(negedge clk);
    assert(&coverage_complete) else $fatal(1, "tick boundary scenario incomplete");
    $display("SUCCESS: exact tick phase/spacing at periods 1,2,3,7,8,9,16"); $finish;
  end
endmodule
