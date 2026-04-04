// Purpose: random debug-signal generator testbench.
//
// Checks that the lightweight pseudo-random debug source produces activity as expected for use in
// bring-up/probing paths.
`timescale 1ns/1ps

module tb_rand_signal_gen;

    // -----------------------------
    // Test configuration
    // -----------------------------
    localparam int unsigned CLOCK_HZ          = 100_000_000;
    localparam time         CLK_PERIOD_NS     = 10ns;  // 100 MHz

    // Choose min/max periods in cycles
    localparam int unsigned MIN_PERIOD_CYCLES = 1;
    localparam int unsigned MAX_PERIOD_CYCLES = 5000;

    // Simulation length / statistics
    localparam int unsigned WARMUP_CYCLES     = 100_000;     // ignore initial transient
    localparam int unsigned MEASURE_CYCLES    = 10_000_000;  // measurement window
    localparam int unsigned TOTAL_CYCLES      = WARMUP_CYCLES + MEASURE_CYCLES;

    // Optional tolerance check (for a random generator, allow a small error band)
    // With MEASURE_CYCLES=2e6, sigma ~ sqrt(p(1-p)/N) ~ 0.00035 for p=0.5
    // So ±0.005 is extremely loose; tighten if you want.
    localparam real         MEAN_TOL          = 0.005;

    // -----------------------------
    // DUT I/O
    // -----------------------------
    logic clk;
    logic reset;
    logic oe;
    logic signal;

    // -----------------------------
    // Instantiate DUT
    // -----------------------------
    rand_signal_gen #(
        .MIN_PERIOD_CYCLES (MIN_PERIOD_CYCLES),
        .MAX_PERIOD_CYCLES (MAX_PERIOD_CYCLES),
        .SEED              (32'h1234_5678)
    ) dut (
        .clk    (clk),
        .reset  (reset),
        .oe     (oe),
        .signal (signal)
    );

    // -----------------------------
    // Clock generation
    // -----------------------------
    initial clk = 1'b0;
    always #(CLK_PERIOD_NS/2) clk = ~clk;

    // -----------------------------
    // VCD dump
    // -----------------------------
    initial begin
      $dumpfile("rand_signal_gen.vcd");
      $dumpvars(0, tb_rand_signal_gen);
    end

    // -----------------------------
    // Stimulus + statistics
    // -----------------------------
    longint unsigned ones_count;
    longint unsigned sample_count;

    // For optional reporting
    real mean;
    real err;

    task automatic run_cycles(input int unsigned n);
        int unsigned i;
        begin
            for (i = 0; i < n; i++) begin
                @(posedge clk);
            end
        end
    endtask

    initial begin
        // Init
        reset        = 1'b1;
        oe           = 1'b0;
        ones_count   = 0;
        sample_count = 0;

        // Hold reset a bit
        run_cycles(20);

        // Release reset, enable output after a short delay
        reset = 1'b0;
        run_cycles(10);
        oe    = 1'b1;

        // Warmup (ignore)
        run_cycles(WARMUP_CYCLES);

        // Measurement
        repeat (MEASURE_CYCLES) begin
            @(posedge clk);
            ones_count   += (signal ? 1 : 0);
            sample_count += 1;
        end

        // Compute mean
        mean = (sample_count > 0) ? (real'(ones_count) / real'(sample_count)) : 0.0;
        err  = mean - 0.5;

        $display("============================================================");
        $display("Random signal mean estimate over %0d cycles:", sample_count);
        $display("  ones_count   = %0d", ones_count);
        $display("  mean(signal) = %.8f", mean);
        $display("  mean-0.5     = %.8f", err);
        $display("============================================================");

        // Loose self-check (optional)
        if ((mean < (0.5 - MEAN_TOL)) || (mean > (0.5 + MEAN_TOL))) begin
            $error("Mean out of tolerance: mean=%.8f (tol=±%.6f)", mean, MEAN_TOL);
        end else begin
            $display("PASS: mean within ±%.6f of 0.5", MEAN_TOL);
        end

        // Let waveform settle a bit then finish
        run_cycles(200);
        $finish;
    end

endmodule
