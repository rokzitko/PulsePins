// Preprocessor-specific coverage at its accepted-command/output boundary.
// The including bench connects cov_din, cov_valid, cov_ready, cov_output,
// cov_output_ready, cov_active and cov_infinite to the DUT ports/state.
`include "../../testbench/scenario_coverage.svh"
int cov_passes = 0, cov_discards = 0, cov_stores = 0, cov_replays = 0;
int cov_zero_length = 0, cov_one_length = 0, cov_max_length = 0, cov_bad_length = 0;
int cov_finite_words = 0, cov_infinite_words = 0, cov_words = 0, cov_replay_stalls = 0;
int cov_reset_finite = 0, cov_reset_infinite = 0;
logic [MEMORY_POSITIONS-1:0] cov_positions = '0;
always @(posedge clk) begin
  if (reset && cov_active) begin
    if (cov_infinite) cov_reset_infinite++; else cov_reset_finite++;
  end
  if (!reset) begin
    if (cov_valid && cov_ready) begin
      if (!cov_din[64+BIT_NOPASS]) cov_passes++;
      else if (cov_din[64+BIT_STORE] && !cov_din[64+BIT_REPLAY]) begin
        cov_stores++;
        cov_positions[cov_din[64+BIT_POSITIONS_LO +: $clog2(MEMORY_POSITIONS)]] = 1;
      end else if (!cov_din[64+BIT_STORE] && cov_din[64+BIT_REPLAY]) begin
        cov_replays++;
        if (cov_din[31:0] == 0) cov_zero_length++;
        if (cov_din[31:0] == 1) cov_one_length++;
        if (cov_din[31:0] == MEMORY_POSITIONS) cov_max_length++;
        if (cov_din[31:0] > MEMORY_POSITIONS) cov_bad_length++;
      end else if (!cov_din[64+BIT_STORE] && !cov_din[64+BIT_REPLAY]) cov_discards++;
    end
    if (cov_active && !cov_output_ready) cov_replay_stalls++;
    if (cov_output && cov_output_ready) begin
      cov_words++;
      if (cov_active) begin
        if (cov_infinite) cov_infinite_words++; else cov_finite_words++;
      end
    end
  end
end
