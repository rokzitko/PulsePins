// Generate glitchy random signals for testing purposes. The code generates a mixture
// of bit flips, glitches, and bursts.
// Feb 2026

module rand_signal_gen #(
    // Period bounds in clock cycles (synthesizable)
    parameter int unsigned MIN_PERIOD_CYCLES  = 1000,
    parameter int unsigned MAX_PERIOD_CYCLES  = 100000,

    // PRNG seed (must be nonzero for the LFSR used here)
    parameter logic [31:0] SEED               = 32'h1
) (
    input  logic clk,
    input  logic reset,   // synchronous reset (active high)
    input  logic oe,      // output enable; when 0, output forced low and generator pauses
    output logic signal
);

    // ----------------------------
    // Safety / parameter checks
    // ----------------------------
    initial begin
        if (MIN_PERIOD_CYCLES < 1) $fatal(1, "MIN_PERIOD_CYCLES must be >= 1");
        if (MAX_PERIOD_CYCLES < MIN_PERIOD_CYCLES) $fatal(1, "MAX_PERIOD_CYCLES must be >= MIN_PERIOD_CYCLES");
        if (SEED == 32'h0) $fatal(1, "SEED must be nonzero for this LFSR");
    end

    // ----------------------------
    // 32-bit Fibonacci LFSR PRNG (maximal-length polynomial example)
    // taps: 32, 22, 2, 1  (one of the common maximal polynomials)
    // ----------------------------
    function automatic logic [31:0] lfsr_next(input logic [31:0] s);
        logic fb;
        begin
            fb = s[31] ^ s[21] ^ s[1] ^ s[0];
            lfsr_next = {s[30:0], fb};
        end
    endfunction

    // Biased-free baseline: we also use LFSR bit(s) to randomize initial baseline.
    logic [31:0] prng;

    // Random range helper (modulo bias is negligible for typical ranges; if you need
    // perfectly uniform selection, replace with rejection sampling).
    function automatic int unsigned rand_range(
        input logic [31:0] r,
        input int unsigned lo,
        input int unsigned hi
    );
        int unsigned span;
        begin
            span = hi - lo + 1;
            rand_range = lo + (r % span);
        end
    endfunction

    // ----------------------------
    // Event engine
    // ----------------------------
    typedef enum logic [1:0] { ST_IDLE, ST_GLITCH, ST_BURST } state_t;
    state_t st;

    logic baseline;             // baseline value (unbiased in steady state)
    logic sig_reg;              // internal driven value (baseline with transient mods)

    int unsigned countdown;     // cycles to next event start (in IDLE)
    int unsigned phase_cnt;     // remaining cycles within current glitch/burst segment
    int unsigned seg_left;      // remaining minimal-period segments in a burst (each segment lasts MIN_PERIOD_CYCLES)
    logic base_save;            // baseline to return to at end of glitch/burst

    // Output gating and pause behavior:
    // - When oe=0: output forced to 0 and internal counters stop (pauses generator).
    assign signal = oe ? sig_reg : 1'b0;

    // Choose next inter-event delay uniformly in [MIN_PERIOD_CYCLES, MAX_PERIOD_CYCLES]
    function automatic int unsigned next_delay(input logic [31:0] r);
        next_delay = rand_range(r, MIN_PERIOD_CYCLES, MAX_PERIOD_CYCLES);
    endfunction

    // Mode selection:
    // Make FLIP relatively frequent to keep baseline mixing fast.
    // 00/01 => flip (50%)
    // 10    => glitch (25%)
    // 11    => burst (25%)
    function automatic logic [1:0] pick_mode(input logic [31:0] r);
        pick_mode = r[1:0];
    endfunction

    // Burst repetition count: 5..10 (uniform)
    function automatic int unsigned pick_burst_reps(input logic [31:0] r);
        pick_burst_reps = rand_range(r, 5, 10);
    endfunction

    // ----------------------------
    // Sequential logic
    // ----------------------------
    always_ff @(posedge clk) begin
        if (reset) begin
            prng      <= (SEED != 32'h0) ? SEED : 32'h1;

            // Randomize initial baseline to avoid a reset-induced transient bias.
            baseline  <= 1'b0;
            sig_reg   <= 1'b0;

            st        <= ST_IDLE;
            countdown <= MIN_PERIOD_CYCLES;  // will be overwritten after first prng advance
            phase_cnt <= 0;
            seg_left  <= 0;
            base_save <= 1'b0;
        end else begin
            // Pause generator when oe=0 (output is forced low combinationally)
            if (!oe) begin
                // keep state/counters as-is (paused)
                prng <= prng;
            end else begin
                // Advance PRNG every clock for good diffusion
                prng <= lfsr_next(prng);

                // On the first active cycle after reset, also randomize baseline from PRNG
                // (safe to do repeatedly, but we only want it once; this is a simple heuristic:
                // if countdown==MIN_PERIOD_CYCLES and st==IDLE and phase_cnt==0 and seg_left==0,
                // treat as initial setup).
                if (st == ST_IDLE && phase_cnt == 0 && seg_left == 0 && countdown == MIN_PERIOD_CYCLES) begin
                    baseline  <= prng[0];
                    sig_reg   <= prng[0];
                    countdown <= next_delay(prng);
                end else begin
                    unique case (st)
                        ST_IDLE: begin
                            if (countdown != 0) begin
                                countdown <= countdown - 1;
                            end else begin
                                logic [1:0] mode;
                                mode = pick_mode(prng);

                                // Update baseline / start transient event
                                if (mode[1:0] == 2'b10) begin
                                    // GLITCH: invert for MIN_PERIOD_CYCLES, then return to baseline
                                    base_save <= baseline;
                                    sig_reg   <= ~baseline;
                                    phase_cnt <= MIN_PERIOD_CYCLES;
                                    st        <= ST_GLITCH;
                                end else if (mode[1:0] == 2'b11) begin
                                    // BURST: 5..10 repetitions, minimal period for highs and lows
                                    // Model as 2*reps segments, each lasting MIN_PERIOD_CYCLES, toggling each segment.
                                    int unsigned reps;
                                    reps      = pick_burst_reps(prng);
                                    base_save <= baseline;
                                    sig_reg   <= ~baseline;               // start with inversion
                                    phase_cnt <= MIN_PERIOD_CYCLES;
                                    seg_left  <= (2 * reps) - 1;          // one segment is already started
                                    st        <= ST_BURST;
                                end else begin
                                    // FLIP (covers 00 and 01): permanent baseline toggle
                                    baseline  <= ~baseline;
                                    sig_reg   <= ~baseline;

                                    // Next event delay
                                    countdown <= next_delay(prng);
                                end
                            end
                        end

                        ST_GLITCH: begin
                            if (phase_cnt != 0) begin
                                phase_cnt <= phase_cnt - 1;
                            end else begin
                                // Return to baseline and schedule next event
                                sig_reg   <= base_save;
                                // baseline remains unchanged
                                countdown <= next_delay(prng);
                                st        <= ST_IDLE;
                            end
                        end

                        ST_BURST: begin
                            if (phase_cnt != 0) begin
                                phase_cnt <= phase_cnt - 1;
                            end else begin
                                if (seg_left == 0) begin
                                    // Burst done: ensure we return to baseline
                                    sig_reg   <= base_save;
                                    countdown <= next_delay(prng);
                                    st        <= ST_IDLE;
                                end else begin
                                    // Next minimal-period segment: toggle and continue
                                    sig_reg   <= ~sig_reg;
                                    phase_cnt <= MIN_PERIOD_CYCLES;
                                    seg_left  <= seg_left - 1;
                                end
                            end
                        end

                        default: st <= ST_IDLE;
                    endcase
                end
            end
        end
    end

endmodule
