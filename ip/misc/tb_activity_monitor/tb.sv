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
    wire active;

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

    // Stimulus
    initial begin
        $dumpfile("tb_presence_detector.vcd");
        $dumpvars(0, tb_presence_detector);

        // Reset for a while
        #100 reset = 0;

        // Wait some clocks
        repeat(5) @(posedge clk);

        // Generate a posedge *between* clk edges
        #7 sig_in = 1;
        #5 sig_in = 0;

        // Another pulse, this time aligned with a clk edge
        repeat(10) @(posedge clk);
        sig_in = 1; #20 sig_in = 0;

        // Burst of two very quick pulses within one clk period
        #7  sig_in = 1; #2 sig_in = 0;
        #5  sig_in = 1; #2 sig_in = 0;

        // Wait long enough for window to expire
        #(WINDOW_CYCLES*30);

        // Another pulse to re-activate
        sig_in = 1; #10 sig_in = 0;

        // Wait long enough for window to expire
        #(WINDOW_CYCLES*30);

        // Finish
        #(2000);
        $finish;
    end

endmodule
