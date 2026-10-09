// Exhaustive 8-bit trigger predicate plus synchronous reset/enable/latch semantics.
// 256 constant-mask instances check every input/pattern/mask triple in 65,536
// clocked trials (16,777,216 comparisons), without sampling DUT internals.
`timescale 1ns/1ps
`include "../config.vh"
`default_nettype none

module tb_and_exhaustive;
  localparam int VALUES = 1 << WIDTH_TRIGGER;
  logic clk = 0;
  always #1 clk = ~clk;
  logic reset = 1, enable = 0;
  logic [WIDTH_TRIGGER-1:0] pattern = 0, value = 0;
  wire [VALUES-1:0] outputs;
  for (genvar m = 0; m < VALUES; m++) begin : masks
    and_trigger dut(.clk, .reset, .trigger_enable(enable), .i(value),
                    .pattern, .mask(WIDTH_TRIGGER'(m)), .o(outputs[m]));
  end

  task automatic tick(input bit rst, en,
                      input logic [WIDTH_TRIGGER-1:0] pat, val,
                      input logic [VALUES-1:0] expected);
    logic [VALUES-1:0] previous;
    begin
      @(negedge clk);
      previous = outputs;
      reset = rst;
      enable = en;
      pattern = pat;
      value = val;
      #1ps;
      assert(outputs === previous) else $fatal(1, "trigger changed away from the sampling edge");
      @(posedge clk);
      #1ps;
      if (outputs !== expected) begin
        for (int m = 0; m < VALUES; m++)
          if (outputs[m] !== expected[m])
            $fatal(1, "mask=%h pattern=%h input=%h reset=%b enable=%b got=%b expected=%b",
              WIDTH_TRIGGER'(m), pat, val, rst, en, outputs[m], expected[m]);
      end
    end
  endtask

  logic [VALUES-1:0] expected;
  longint comparisons = 0;
  int control_trials = 0;
  integer fh;
  initial begin
    assert(WIDTH_TRIGGER == 8) else $fatal(1, "review exhaustive runtime budget for changed trigger width");
    for (int p = 0; p < VALUES; p++) begin
      for (int v = 0; v < VALUES; v++) begin
        // Specification model: compare the two independently masked operands.
        for (int m = 0; m < VALUES; m++) expected[m] = ((v & m) == (p & m));
        tick(1, 1, p, v, '0);
        tick(0, 1, p, v, expected);
        comparisons += VALUES;
      end
      tick(1, 1, p, p, '0);   // reset wins over a matching, enabled condition
      tick(0, 0, p, p, '0);   // matching while disabled cannot arm the output
      tick(0, 0, p, ~p, '0);
      tick(0, 1, p, p, '1);   // re-enable; zero mask also matches
      tick(0, 0, p, ~p, '1);  // disable must not clear an already latched trigger
      tick(0, 1, p, ~p, '1);  // mismatches must not clear it either
      control_trials++;
    end
    assert(comparisons == 16777216 && control_trials == 256) else $fatal(1, "incomplete exhaustive coverage");
    $display("SUCCESS: %0d trigger comparisons, %0d reset/enable/latch trials", comparisons, control_trials);
    fh = $fopen("SUCCESS", "w");
    $fclose(fh);
    $finish;
  end
  initial #2000000 $fatal(1, "exhaustive trigger watchdog: comparisons=%0d", comparisons);
endmodule
