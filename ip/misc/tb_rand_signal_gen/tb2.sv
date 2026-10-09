// Scheduling bounds and true pause semantics, including singleton and
// non-power-of-two period ranges. All event/PRNG state must freeze while disabled.
`timescale 1ns/1ps
module tb_random_boundaries;
  logic clk = 0, reset = 1, oe = 0;
  always #0.5 clk = ~clk;
  wire [2:0] value;
  wire [2:0] coverage_complete;
  for (genvar g = 0; g < 3; g++) begin : ranges
    localparam int LO = g == 2 ? 3 : 1;
    localparam int HI = g == 0 ? 1 : g == 1 ? 2 : 10;
    rand_signal_gen #(.MIN_PERIOD_CYCLES(LO), .MAX_PERIOD_CYCLES(HI), .SEED(g+1)) dut(.clk, .reset, .oe, .signal(value[g]));
    logic [132:0] held;
    bit minimum_seen = 0, maximum_seen = 0, high_seen = 0;
    int delay_value, paused = 0, reloads = 0, previous_count;
    initial begin
      for (int n = 0; n < 512; n++) begin
        delay_value = dut.next_delay(n);
        assert(delay_value >= LO && delay_value <= HI) else $fatal(1, "random delay outside configured bounds");
        if (delay_value == LO) minimum_seen = 1;
        if (delay_value == HI) maximum_seen = 1;
      end
      assert(minimum_seen && maximum_seen) else $fatal(1, "random delay endpoints unreachable");
    end
    always @(posedge clk) begin
      held = {dut.prng, dut.countdown, dut.phase_cnt, dut.seg_left, dut.st, dut.baseline, dut.sig_reg, dut.base_save};
      previous_count = dut.countdown;
      #1ps;
      if (!reset) begin
        assert(dut.countdown <= HI && dut.phase_cnt <= LO && dut.seg_left < 20) else $fatal(1, "event duration bound violated");
        if (!oe) begin
          assert(value[g] === 1'b0 && held === {dut.prng, dut.countdown, dut.phase_cnt, dut.seg_left, dut.st, dut.baseline, dut.sig_reg, dut.base_save}) else
            $fatal(1, "disabled random generator advanced or drove output");
          paused++;
        end else begin
          assert(!$isunknown(value[g])) else $fatal(1, "unknown random output");
          if (value[g]) high_seen = 1;
          if (dut.countdown > previous_count) begin
            assert(dut.countdown >= LO) else $fatal(1, "inter-event reload below minimum");
            reloads++;
          end
        end
      end
    end
    assign coverage_complete[g] = paused > 0 && high_seen && (LO == HI || reloads > 0);
    task automatic report_scenarios;
      tb_require_scenario($sformatf("range_%0d_minimum", g), minimum_seen);
      tb_require_scenario($sformatf("range_%0d_maximum", g), maximum_seen);
      tb_require_scenario($sformatf("range_%0d_paused_cycles", g), paused);
      tb_require_scenario($sformatf("range_%0d_active_output", g), high_seen);
      if (LO != HI) tb_require_scenario($sformatf("range_%0d_reload_events", g), reloads);
    endtask
  end
  initial begin
    repeat (4) @(negedge clk); reset = 0;
    for (int n = 0; n < 2000; n++) begin @(negedge clk); oe = n % 37 >= 9; end
    assert(&coverage_complete) else $fatal(1, "random boundary coverage incomplete");
    ranges[0].report_scenarios(); ranges[1].report_scenarios(); ranges[2].report_scenarios();
    $display("SUCCESS: random-generator period endpoints, reload bounds and pause/resume"); $finish;
  end
  `include "../../testbench/scenario_coverage.svh"
endmodule
