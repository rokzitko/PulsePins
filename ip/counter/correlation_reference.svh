// Test-only offline reference for the correlation benches. Record public input
// samples, then calculate each lag from the complete history after capture.
// The including bench provides SAMPLE_COUNT, length, reference_first/second,
// and the public snapshot/readout interface.
logic reference_a [0:SAMPLE_COUNT-1];
logic reference_b [0:SAMPLE_COUNT-1];
int sample_count = 0;
int cov_latches = 0, cov_checked_lags = 0;
int cov_a_high = 0, cov_a_low = 0, cov_b_high = 0, cov_b_low = 0;
`include "../testbench/scenario_coverage.svh"

always @(posedge clk) begin
  if (reset) begin
    sample_count = 0;
  end else if (valid) begin
    assert(sample_count < SAMPLE_COUNT) else $fatal(1, "too many correlation samples");
    assert((reference_first === 1'b0 || reference_first === 1'b1) &&
           (reference_second === 1'b0 || reference_second === 1'b1)) else
      $fatal(1, "unknown correlation inputs: %b %b", reference_first, reference_second);
    reference_a[sample_count] = reference_first;
    reference_b[sample_count] = reference_second;
    if (reference_first) cov_a_high++; else cov_a_low++;
    if (reference_second) cov_b_high++; else cov_b_low++;
    sample_count++;
  end
end

always @(posedge clk) if (!reset && latch && sample_count > 0) cov_latches++;

task automatic check_correlation;
  int lag, n;
  logic [31:0] expected;
  begin
    assert(sample_count == SAMPLE_COUNT) else
      $fatal(1, "captured %0d correlation samples, expected %0d", sample_count, SAMPLE_COUNT);
    for (lag = 0; lag <= length; lag++) begin
      expected = 0;
      if (lag == 0) begin
        expected = sample_count;
      end else begin
        for (n = lag; n < sample_count; n++)
          if (reference_b[n] == reference_a[n-lag]) expected++;
      end
      @(negedge clk);
      addr = lag;
      high_low = 0;
      @(posedge clk);
      #0.01;
      assert(result === expected) else
        $fatal(1, "correlation address %0d: got %0d expected %0d", lag, result, expected);
      cov_checked_lags++;
    end
    tb_require_scenario("valid_samples", sample_count, SAMPLE_COUNT);
    tb_require_scenario("snapshot_after_samples", cov_latches);
    tb_require_scenario("checked_lag_addresses", cov_checked_lags, length+1);
    tb_require_scenario("first_input_high", cov_a_high);
    tb_require_scenario("second_input_high", cov_b_high);
    if (REQUIRE_VARIATION) begin
      tb_require_scenario("first_input_low", cov_a_low);
      tb_require_scenario("second_input_low", cov_b_low);
    end
    $display("SUCCESS: checked sample count and all %0d correlation lags", length);
  end
endtask
