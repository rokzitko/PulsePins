// Exact activity windows, sub-clock asynchronous pulses, burst coalescing and reset.
`timescale 1ns/1ps
module tb_activity_windows;
  logic clk = 0, reset = 0, sig = 0, pulse = 0;
  always #1 clk = ~clk;
  wire [2:0] async_active, sync_active;
  int cycle = 0, async_due = -1, async_last = -1, sync_last = -1;
  int async_events = 0, sync_events = 0;
  logic [2:0] observed_async = 0, observed_sync = 0;
  int reset_pending = 0, reset_active = 0;
  always @(posedge reset) begin
    if (async_due > cycle) reset_pending++;
    if (|async_active) reset_active++;
  end
  for (genvar g = 0; g < 3; g++) begin : windows
    localparam int N = g == 0 ? 0 : g == 1 ? 1 : 8;
    presence_detector_async_posedge #(.CLK_FREQ_HZ(1000), .WINDOW_MS(N)) a(
      .clk, .reset, .sig_in(sig), .active(async_active[g]));
    presence_detector_sync_pulse #(.CLK_FREQ_HZ(1000), .WINDOW_MS(N)) s(
      .clk, .reset, .pulse, .active(sync_active[g]));
    always @(negedge clk) begin
      if (!reset) begin
        assert(async_active[g] === (async_last >= 0 && cycle-async_last <= N)) else $fatal(1, "async window %0d mismatch at cycle %0d", N, cycle);
        assert(sync_active[g] === (sync_last >= 0 && cycle-sync_last <= N)) else $fatal(1, "sync window %0d mismatch", N);
        if (async_active[g]) observed_async[g] = 1;
        if (sync_active[g]) observed_sync[g] = 1;
      end
    end
  end
  always @(posedge clk) begin
    cycle++;
    if (reset) begin async_due = -1; async_last = -1; sync_last = -1; end
    else begin
      if (cycle == async_due) begin async_last = cycle; async_events++; end
      if (pulse) begin sync_last = cycle; sync_events++; end
    end
  end
  task automatic asynchronous_pulse(input bit burst);
    @(negedge clk); #0.13; sig = 1; async_due = cycle + 3;
    #0.17; sig = 0;
    if (burst) begin #0.11; sig = 1; #0.14; sig = 0; end
  endtask
  initial begin
    reset <= 1; // deliver an actual edge to the asynchronous event-latch reset
    repeat (5) @(negedge clk); reset = 0;
    for (int n = 0; n < 3; n++) begin
      asynchronous_pulse(n == 1);
      repeat (16) @(negedge clk);
      pulse = 1;
      @(negedge clk); pulse = 0;
      repeat (16) @(negedge clk);
      assert(async_active == 0 && sync_active == 0) else $fatal(1, "activity did not expire");
    end
    asynchronous_pulse(0);
    @(negedge clk); reset = 1;
    repeat (4) @(negedge clk); reset = 0;
    repeat (12) @(negedge clk);
    assert(async_active == 0) else $fatal(1, "pre-reset event appeared after reset");
    asynchronous_pulse(0);
    repeat (4) @(negedge clk);
    assert(async_active[2]) else $fatal(1, "active-reset scenario was not reached");
    reset = 1; #1ps;
    assert(async_active == 0) else $fatal(1, "asynchronous activity reset failed");
    repeat (4) @(negedge clk); reset = 0;
    asynchronous_pulse(0); repeat (20) @(negedge clk);
    assert(async_events >= 5 && sync_events == 3) else $fatal(1, "missing activity cases");
    for (int window = 0; window < 3; window++) begin
      tb_require_scenario($sformatf("async_window_%0d", window), observed_async[window]);
      tb_require_scenario($sformatf("sync_window_%0d", window), observed_sync[window]);
    end
    tb_require_scenario("reset_pending_detection", reset_pending);
    tb_require_scenario("reset_active_window", reset_active);
    $display("SUCCESS: async short-pulse capture, burst coalescing, windows 0/1/8, expiry and reset");
    $finish;
  end
  initial #5000 $fatal(1, "activity window watchdog");
  `include "../../testbench/scenario_coverage.svh"
endmodule
