module heartbeat #(
 parameter integer CLK_FREQ_HZ  = 50_000_000, // clock frequency in Hz
 parameter integer PULSE_MS     = 50,         // pulse duration in ms
 parameter integer GAP_MS       = 50,         // gap between the two pulses in ms
 parameter integer PERIOD_MS    = 1000        // total repetition period in ms
)(
 input  wire clk,
 input  wire reset,
 output reg  heartbeat
);

// Convert to clock ticks
localparam integer TICKS_PER_MS = CLK_FREQ_HZ / 1000;
localparam integer PULSE_TICKS  = PULSE_MS  * TICKS_PER_MS;
localparam integer GAP_TICKS    = GAP_MS    * TICKS_PER_MS;
localparam integer PERIOD_TICKS = PERIOD_MS * TICKS_PER_MS;

integer counter;

always @(posedge clk) begin
 if (reset) begin
    counter   <= 0;
    heartbeat <= 1'b0;
  end else begin
    // increment or wrap counter
    if (counter == PERIOD_TICKS-1)
      counter <= 0;
    else
      counter <= counter + 1;

    // synchronous heartbeat output
    if ((counter < PULSE_TICKS) || 
        (counter >= (PULSE_TICKS + GAP_TICKS) &&
        counter <  (2*PULSE_TICKS + GAP_TICKS)))
       heartbeat <= 1'b1;
    else
      heartbeat <= 1'b0;
  end
end

endmodule
