module activity_monitor #(
    parameter integer CLK_FREQ_HZ      = 50_000_000, // input clock frequency
    parameter integer BLINK_PERIOD_MS  = 100,        // blink period in ms
    parameter integer ACTIVITY_WINDOW_MS = 200       // window for detecting activity
)(
    input  wire clk,
    input  wire reset,
    input  wire sig_in,
    output reg  activity
);

    // ------------------------------------------------------------------------
    // Convert parameters into cycle counts
    localparam integer BLINK_PERIOD_CYCLES   = (CLK_FREQ_HZ / 1000) * BLINK_PERIOD_MS;
    localparam integer ACTIVITY_WINDOW_CYCLES = (CLK_FREQ_HZ / 1000) * ACTIVITY_WINDOW_MS;

    // ------------------------------------------------------------------------
    // Detect changes on sig_in
    reg sig_in_d;
    always @(posedge clk or posedge reset) begin
        if (reset)
            sig_in_d <= 0;
        else
            sig_in_d <= sig_in;
    end

    wire sig_changed = (sig_in ^ sig_in_d);

    // ------------------------------------------------------------------------
    // Activity timer: counts down whenever a change is detected
    reg [$clog2(ACTIVITY_WINDOW_CYCLES+1)-1:0] activity_timer;

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            activity_timer <= 0;
        end else if (sig_changed) begin
            activity_timer <= ACTIVITY_WINDOW_CYCLES; // reload on activity
        end else if (activity_timer != 0) begin
            activity_timer <= activity_timer - 1;
        end
    end

    wire active = (activity_timer != 0);

    // ------------------------------------------------------------------------
    // Blinker: free-running counter
    reg [$clog2(BLINK_PERIOD_CYCLES)-1:0] blink_counter;

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            blink_counter <= 0;
        end else if (blink_counter == BLINK_PERIOD_CYCLES-1) begin
            blink_counter <= 0;
        end else begin
            blink_counter <= blink_counter + 1;
        end
    end

    wire blink_signal = blink_counter < (BLINK_PERIOD_CYCLES/2); // 50% duty cycle

    // ------------------------------------------------------------------------
    // Final output
    always @(posedge clk or posedge reset) begin
        if (reset)
            activity <= 1'b0;
        else
            activity <= active ? blink_signal : 1'b0;
    end

endmodule

// Presence detector for pulses that are already synchronous to clk.
// Unlike presence_detector_async_posedge, this block never uses the pulse input as a clock.
module presence_detector_sync_pulse #(
    parameter integer CLK_FREQ_HZ = 50_000_000,
    parameter integer WINDOW_MS   = 200
)(
    input  wire clk,
    input  wire reset,
    input  wire pulse,
    output reg  active
);
    localparam integer WINDOW_CYCLES = (CLK_FREQ_HZ/1000) * WINDOW_MS;
    localparam integer W_W = (WINDOW_CYCLES>0) ? $clog2(WINDOW_CYCLES+1) : 1;

    reg [W_W-1:0] win_cnt;

    always @(posedge clk) begin
        if (reset) begin
            win_cnt <= {W_W{1'b0}};
            active  <= 1'b0;
        end else if (pulse) begin
            win_cnt <= WINDOW_CYCLES[W_W-1:0];
            active  <= 1'b1;
        end else if (win_cnt != 0) begin
            win_cnt <= win_cnt - 1'b1;
            active  <= 1'b1;
        end else begin
            active  <= 1'b0;
        end
    end
endmodule

// Presence detector (posedge-only) with asynchronous event latch and synchronous timeout.
//
module presence_detector_async_posedge #(
    parameter integer CLK_FREQ_HZ = 50_000_000,
    parameter integer WINDOW_MS   = 200
)(
    input  wire clk,       // system clock
    input  wire reset,     // async clear for clk-domain state
    input  wire sig_in,    // asynchronous signal; we care about posedges
    output reg  active     // 1 if a posedge occurred within the last WINDOW_MS
);
    // ------------------------------------------------------------------------
    // Window in cycles
    localparam integer WINDOW_CYCLES = (CLK_FREQ_HZ/1000) * WINDOW_MS;
    localparam integer W_W = (WINDOW_CYCLES>0) ? $clog2(WINDOW_CYCLES+1) : 1;

    // ------------------------------------------------------------------------
    // Async event latch (event_latch):
    //  - SET:   on posedge sig_in (captures *presence* of a posedge at any time)
    //  - CLEAR: driven from clk domain via clr_async (level-active async clear)
    //
    reg event_latch;
    reg clr_async;  // generated in clk domain; asserted until we observe latch cleared

    always @(posedge sig_in or posedge clr_async or posedge reset) begin
        if (reset)         event_latch <= 1'b0;
        else if (clr_async) event_latch <= 1'b0;  // async clear (level)
        else               event_latch <= 1'b1;   // set on each posedge
    end

    // ------------------------------------------------------------------------
    // Synchronize the latch into clk domain (2-FF)
    reg event_meta, event_sync;
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            event_meta <= 1'b0;
            event_sync <= 1'b0;
        end else begin
            event_meta <= event_latch;
            event_sync <= event_meta;
        end
    end

    // Rising-edge detect in clk domain => one-cycle pulse per “seen” posedge
    reg event_sync_d;
    always @(posedge clk or posedge reset) begin
        if (reset) event_sync_d <= 1'b0;
        else       event_sync_d <= event_sync;
    end
    wire event_pulse = event_sync & ~event_sync_d;

    // ------------------------------------------------------------------------
    // Handshake to clear the async latch:
    //  - Assert clr_async when we first see event_pulse.
    //  - Hold it until the synchronized view (event_sync) goes LOW again.
    //
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            clr_async <= 1'b0;
        end else if (event_pulse) begin
            clr_async <= 1'b1;
        end else if (~event_sync) begin
            clr_async <= 1'b0;
        end
    end

    // ------------------------------------------------------------------------
    // Window timer: reload on event_pulse, else count down to 0
    reg [W_W-1:0] win_cnt;
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            win_cnt <= {W_W{1'b0}};
            active  <= 1'b0;
        end else if (event_pulse) begin
            win_cnt <= WINDOW_CYCLES[W_W-1:0];
            active  <= 1'b1;
        end else if (win_cnt != 0) begin
            win_cnt <= win_cnt - 1'b1;
            active  <= 1'b1;
        end else begin
            active  <= 1'b0;
        end
    end
endmodule
