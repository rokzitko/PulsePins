// Public-register reference checks for distinct channel selectors and optional
// features, with unrelated data/control clocks. Compile with and without SUM2/CC.
`timescale 1ns/1ps
module tb_counter_channels;
  logic clk = 0, d_clk = 0;
  always #1 clk = ~clk;
  initial begin #0.19; forever #1.7 d_clk = ~d_clk; end
  logic reset = 1, d_valid = 0;
  logic [31:0] d = 0;
  logic [3:0] avs_s0_address = 0;
  logic avs_s0_read = 0, avs_s0_write = 0;
  logic [31:0] avs_s0_writedata = 0;
  wire [31:0] avs_s0_readdata;
  counter_if dut(.clk, .reset, .d, .d_valid, .d_clk, .avs_s0_address,
    .avs_s0_read, .avs_s0_write, .avs_s0_readdata, .avs_s0_writedata);
  bit a [0:63], b [0:63], c [0:63];
  int sel_a, sel_b, sel_c, highs, lag_matches, run_length, sum2_l, sum2_h;
  logic [31:0] got;
  int checked_sets = 0, live_resets = 0, checked_results = 0;
  logic [15:0] checked_instruments = 0;
  always @(posedge d_clk) if (!reset && dut.d_reset && d_valid && dut.bc.ctr_total != 0) live_resets++;

  task automatic write_reg(input logic [3:0] address, input logic [31:0] value);
    @(negedge clk); avs_s0_address = address; avs_s0_writedata = value; avs_s0_write = 1;
    @(negedge clk); avs_s0_write = 0;
  endtask
  task automatic read_result(input int instrument, address, input logic [31:0] expected);
    write_reg(1, instrument); write_reg(3, address);
    @(negedge clk); avs_s0_address = 0; avs_s0_read = 1;
    @(posedge clk); #1ps; got = avs_s0_readdata;
    assert(got === expected) else $fatal(1, "instrument=%0d address=%0d got=%0d expected=%0d", instrument, address, got, expected);
    checked_results++;
    checked_instruments[instrument] = 1;
    @(negedge clk); avs_s0_read = 0;
  endtask
  task automatic latch_results;
    write_reg(7, 2);
    repeat (8) @(negedge d_clk);
    write_reg(7, 0);
    repeat (24) @(negedge clk);
  endtask

  initial begin
    repeat (8) @(negedge clk); reset = 0;
    repeat (20) @(negedge d_clk);
    for (int phase = 0; phase < 2; phase++) begin
      sel_a = phase ? 31 : 2; sel_b = phase ? 0 : 9; sel_c = phase ? 17 : 23;
      write_reg(4, sel_a); write_reg(5, sel_b); write_reg(6, sel_c);
      repeat (20) @(negedge d_clk);
      // Reset while the selected inputs are actively being sampled, keeping
      // selector CDC state alive through the instrument-only reset.
      d_valid = 1; d = '1;
      repeat (12) @(negedge d_clk);
      write_reg(7, 1);
      repeat (10) @(negedge d_clk);
      d_valid = 0;
      repeat (3) @(negedge d_clk);
      write_reg(7, 0);
      repeat (10) @(negedge d_clk);
      latch_results(); read_result(1, 0, 0);

      highs = 0; sum2_l = 0; sum2_h = 0; run_length = 0;
      for (int n = 0; n < 64; n++) begin
        a[n] = ((n + phase) % 3 == 0);
        b[n] = (n % 5 < 2);
        c[n] = ((n + 2) % 7 < 4);
        highs += a[n];
        if (n > 0 && a[n] != a[n-1]) begin
          if (a[n-1]) sum2_h += run_length*run_length;
          else sum2_l += run_length*run_length;
          run_length = 0;
        end
        run_length++;
        @(negedge d_clk); d = $urandom;
        d[sel_a] = a[n]; d[sel_b] = b[n]; d[sel_c] = c[n]; d_valid = 1;
      end
      if (a[63]) sum2_h += run_length*run_length; else sum2_l += run_length*run_length;
      @(negedge d_clk); d_valid = 0;
      repeat (8) @(negedge d_clk);
      latch_results();
      read_result(1, 0, 64); read_result(1, 2, 64-highs); read_result(1, 3, highs);
      for (int lag = 1; lag <= 3; lag++) begin
        lag_matches = 0;
        for (int n = lag; n < 64; n++) if (b[n-lag] == c[n]) lag_matches++;
`ifdef COUNTER_CC
        read_result(7, lag, lag_matches);
`else
        read_result(7, lag, 0);
`endif
      end
`ifdef DO_SUM2
      read_result(2, 12, sum2_l); read_result(2, 13, sum2_h);
`else
      read_result(2, 12, 0); read_result(2, 13, 0);
`endif
      write_reg(2, 1); read_result(1, 0, 0); write_reg(2, 0);
      checked_sets++;
    end
    tb_require_scenario("selector_sets_checked", checked_sets, 2);
    tb_require_scenario("reset_while_counting", live_resets, 2);
    tb_require_scenario("register_results_checked", checked_results, 20);
    tb_require_scenario("basic_counter_readback", checked_instruments[1]);
    tb_require_scenario("run_statistics_readback", checked_instruments[2]);
    tb_require_scenario("correlation_feature_readback", checked_instruments[7]);
    $display("SUCCESS: two distinct selector sets, asynchronous sampling/reset, optional-feature register checks");
    $finish;
  end
  initial #30000 $fatal(1, "counter channel/feature watchdog");
  `include "../../testbench/scenario_coverage.svh"
endmodule
