// Small-width, unrelated-clock counter test. An input history is reduced at
// each latch edge, independently of the DUT's incremental implementation.
`timescale 1ns/1ps
module tb_basic_async;
  logic clk = 0, d_clk = 0;
  always #1.7 clk = ~clk;
  initial begin #0.13; forever #1.1 d_clk = ~d_clk; end
  logic reset = 1, d_reset = 1, d = 0, d_valid = 0, d_latch = 0, high_low = 0;
  logic [2:0] addr = 0;
  wire [3:0] result;
  wire overflow;
  basic_counter #(.width_bus(4), .width_ctr(8)) dut(.clk, .clk_reset(reset), .d_clk,
    .d_cdc_reset(reset), .d_reset, .d, .d_valid, .d_latch, .high_low, .addr, .result, .overflow);
  bit history [0:1023];
  int count = 0, cycle = 0, snapshots = 0, upper_reads = 0;
  logic [7:0] snapshot [0:7];
  bit producer_done = 0;
  int live_latches = 0, live_resets = 0, overflow_checks = 0;
  always @(posedge d_clk) if (!reset) begin
    if (d_latch && d_valid && dut.ctr_total != 0) live_latches++;
    if (d_reset && d_valid && dut.ctr_total != 0) live_resets++;
  end

  always @(posedge d_clk) begin
    if (reset) begin
      count = 0;
      for (int a = 0; a < 8; a++) snapshot[a] = 0;
    end else begin
      if (d_latch) begin
        for (int a = 0; a < 8; a++) snapshot[a] = 0;
        snapshot[0] = count > 255 ? 255 : count;
        for (int n = 0; n < count; n++) begin
          if (history[n]) snapshot[3]++; else snapshot[2]++;
          if (n > 0) begin
            if (history[n] && !history[n-1]) snapshot[4]++;
            if (!history[n] && history[n-1]) snapshot[5]++;
          end
        end
        snapshots++;
      end
      if (d_reset) count = 0;
      else if (d_valid) begin history[count] = d; count++; end
    end
  end

  task automatic check_snapshot;
    @(negedge d_clk); d_latch = 1;
    @(negedge d_clk); d_latch = 0;
    repeat (20) @(negedge clk);
    for (int a = 0; a < 8; a++) begin
      for (int half = 0; half < 2; half++) begin
        @(negedge clk); addr = a; high_low = half;
        @(posedge clk); #1ps;
        assert(result === ((snapshot[a] >> (4*half)) & 4'hf)) else
          $fatal(1, "counter snapshot addr=%0d high=%0d got=%h expected=%h", a, half, result, snapshot[a]);
        if (half && result != 0) upper_reads++;
      end
    end
  endtask

  initial begin
    repeat (6) @(negedge clk); reset = 0;
    @(negedge d_clk); d_reset = 0;
    fork
      begin
        for (int n = 0; n < 600; n++) begin
          @(negedge d_clk);
          d = $urandom_range(0, 1); d_valid = (n % 7 != 0);
          d_reset = (n == 180); // reset while valid data is still arriving
          cycle = n;
        end
        @(negedge d_clk); d_valid = 0;
        producer_done = 1;
      end
      begin
        wait(cycle >= 40); check_snapshot();
        wait(cycle >= 250); check_snapshot();
        wait(cycle >= 500); check_snapshot();
      end
    join
    check_snapshot();
    assert(producer_done && count > 255 && overflow === 1'b1 && snapshot[0] == 255) else
      $fatal(1, "saturation/overflow was not exercised");
    overflow_checks++;
    assert(snapshots == 4 && upper_reads > 0) else $fatal(1, "missing latch/high-word coverage");
    @(negedge d_clk); d_reset = 1;
    @(negedge d_clk); d_reset = 0;
    check_snapshot();
    assert(overflow === 1'b0 && snapshot[0] == 0) else $fatal(1, "counter reset did not clear overflow");
    overflow_checks++;
    // Distinguish reaching the maximum from overflowing it on the next sample.
    for (int n = 0; n < 255; n++) begin @(negedge d_clk); d = 1; d_valid = 1; end
    @(negedge d_clk); d_valid = 0;
    check_snapshot();
    assert(count == 255 && snapshot[0] == 255 && overflow === 1'b0) else $fatal(1, "overflow asserted before the 256th sample");
    overflow_checks++;
    @(negedge d_clk); d = 0; d_valid = 1;
    @(negedge d_clk); d_valid = 0;
    check_snapshot();
    assert(count == 256 && snapshot[0] == 255 && overflow === 1'b1) else $fatal(1, "overflow boundary mismatch");
    overflow_checks++;
    tb_require_scenario("live_snapshots", live_latches);
    tb_require_scenario("reset_while_counting", live_resets);
    tb_require_scenario("snapshot_readbacks", snapshots, 7);
    tb_require_scenario("nonzero_high_words", upper_reads);
    tb_require_scenario("overflow_boundary_and_clear", overflow_checks, 4);
    $display("SUCCESS: asynchronous live snapshots, high words, mid-count reset and natural overflow");
    $finish;
  end
  initial #20000 $fatal(1, "asynchronous basic counter watchdog");
  `include "../../testbench/scenario_coverage.svh"
endmodule
