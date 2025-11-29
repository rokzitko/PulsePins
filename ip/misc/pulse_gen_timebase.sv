`timescale 1ns/1ps
`default_nettype none

module pulse_gen_timebase #(
    // Input clock frequency in Hz (must be divisible by 1000)
    parameter int unsigned CLK_FREQ_HZ = 100_000_000
) (
    input  logic clk,      // 100 MHz clock
    input  logic rst,      // synchronous active-high reset

    output logic pulse_1ms,    // 0.001 s pulse (every 1 ms)
    output logic pulse_10ms,   // 0.01 s pulse (every 10 ms)
    output logic pulse_100ms,  // 0.1 s pulse (every 100 ms)
    output logic pulse_1s      // 1 s pulse (every 1000 ms)
);

    // ------------------------------------------------------------------------
    // Basic 1 ms tick from CLK_FREQ_HZ
    // ------------------------------------------------------------------------
    localparam int unsigned TICK_1MS_DIV       = CLK_FREQ_HZ / 1000; // 1 ms
    localparam int unsigned TICK_1MS_DIV_WIDTH = $clog2(TICK_1MS_DIV);

    // Optional static check (safe in simulation/formal; synthesizers ignore $error)
    initial begin
        if (CLK_FREQ_HZ % 1000 != 0) begin
            $error("pulse_gen_timebase: CLK_FREQ_HZ must be divisible by 1000.");
        end
    end

    logic [TICK_1MS_DIV_WIDTH-1:0] cnt_1ms;
    logic [3:0]                    cnt_10ms;
    logic [3:0]                    cnt_100ms;
    logic [3:0]                    cnt_1s;

    always_ff @(posedge clk) begin
        if (rst) begin
            cnt_1ms    <= '0;
            cnt_10ms   <= '0;
            cnt_100ms  <= '0;
            cnt_1s     <= '0;

            pulse_1ms   <= 1'b0;
            pulse_10ms  <= 1'b0;
            pulse_100ms <= 1'b0;
            pulse_1s    <= 1'b0;
        end else begin
            // default: no pulses
            pulse_1ms   <= 1'b0;
            pulse_10ms  <= 1'b0;
            pulse_100ms <= 1'b0;
            pulse_1s    <= 1'b0;

            // 1 ms base divider
            if (cnt_1ms == TICK_1MS_DIV - 1) begin
                cnt_1ms  <= '0;
                pulse_1ms <= 1'b1;   // one clock wide

                // ----------------------------------------------------------------
                // Cascade decade counters: 10 × 1 ms = 10 ms, etc.
                // ----------------------------------------------------------------
                if (cnt_10ms == 4'd9) begin
                    cnt_10ms  <= '0;
                    pulse_10ms <= 1'b1;

                    if (cnt_100ms == 4'd9) begin
                        cnt_100ms  <= '0;
                        pulse_100ms <= 1'b1;

                        if (cnt_1s == 4'd9) begin
                            cnt_1s   <= '0;
                            pulse_1s <= 1'b1; // every 1 s
                        end else begin
                            cnt_1s <= cnt_1s + 4'd1;
                        end
                    end else begin
                        cnt_100ms <= cnt_100ms + 4'd1;
                    end
                end else begin
                    cnt_10ms <= cnt_10ms + 4'd1;
                end
            end else begin
                cnt_1ms <= cnt_1ms + {{(TICK_1MS_DIV_WIDTH-1){1'b0}}, 1'b1};
            end
        end
    end

endmodule

`default_nettype wire

