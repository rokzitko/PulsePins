// Purpose: activity-monitor testbench.
//
// Checks that low-rate activity detection and related status behavior respond correctly to input
// toggling over time.
`timescale 1ns/1ps

module tb_presence_detector;

    // Parameters
    localparam CLK_FREQ_HZ = 50_000_000;   // 50 MHz -> 20 ns period
    localparam WINDOW_MS   = 1;            // short window (1 ms) for sim speed
    localparam WINDOW_CYCLES = (CLK_FREQ_HZ/1000)*WINDOW_MS; // 50,000 cycles

    // Signals
    reg clk   = 0;
    reg reset = 1;
    reg sig_in = 0;
    reg pulse = 0;
    wire active;
    wire sync_active;

    // Clock: 50 MHz
    always #10 clk = ~clk;

    // DUT
    presence_detector_async_posedge #(
        .CLK_FREQ_HZ(CLK_FREQ_HZ),
        .WINDOW_MS(WINDOW_MS)
    ) dut (
        .clk(clk),
        .reset(reset),
        .sig_in(sig_in),
        .active(active)
    );

    presence_detector_sync_pulse #(
        .CLK_FREQ_HZ(CLK_FREQ_HZ),
        .WINDOW_MS(WINDOW_MS)
    ) sync_dut (
        .clk(clk),
        .reset(reset),
        .pulse(pulse),
        .active(sync_active)
    );

    // Stimulus
    initial begin
        // Use WAVES=1 with the runner to capture waveforms in the run directory.

        // Reset for a while
        repeat(5) @(negedge clk);
        reset = 0;

        // Wait some clocks
        repeat(5) @(negedge clk);

        // Synchronous one-cycle pulse must start and eventually expire the activity window.
        pulse = 1;
        @(negedge clk);
        pulse = 0;
        if (!sync_active) $fatal(1, "sync detector failed to activate");
        repeat(WINDOW_CYCLES+2) @(negedge clk);
        if (sync_active) $fatal(1, "sync detector failed to expire");

        // Generate a posedge *between* clk edges
        #7 sig_in = 1;
        #5 sig_in = 0;
        // Let any captured activity from this legacy short-pulse stimulus expire.
        repeat(WINDOW_CYCLES+5) @(posedge clk);
        #1;

        // Another pulse, this time aligned with a clk edge
        repeat(10) @(posedge clk);
        sig_in <= 1; #20 sig_in <= 0;
        repeat(10) @(posedge clk);
        #1;

        // Burst of two very quick pulses within one clk period
        #7  sig_in = 1; #2 sig_in = 0;
        #5  sig_in = 1; #2 sig_in = 0;

        // Wait long enough for window to expire
        repeat(WINDOW_CYCLES+5) @(negedge clk);

        // Another pulse to re-activate
        sig_in = 1; #10 sig_in = 0;
        repeat(10) @(posedge clk);
        #1;

        // Wait long enough for window to expire
        repeat(WINDOW_CYCLES+5) @(negedge clk);

        // Finish
        repeat(100) @(negedge clk);
        $display("PASS");
        $finish;
    end

endmodule
