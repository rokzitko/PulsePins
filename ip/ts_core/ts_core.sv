`default_nettype none

module ts_core #(
  parameter CTR_W = 64                 // counter width
) (
  // Clock and reset
  input  wire              clk,        // timebase clock (e.g. 100 MHz)
  input  wire              reset,      // active-high synchronous reset

  // Asynchronous signal input
  input  wire              sig,        // edge-detected input (asynchronous)
  input  wire              sigA,       // edge-detected input (asynchronous)

  // Avalon-ST output interface
  output wire              aso_valid,  // 1-cycle pulse per captured event
  output wire [CTR_W-1:0]  aso_data,   // captured counter value
  input wire aso_ready,

  output wire              asoA_valid, // 1-cycle pulse per captured event
  output wire [CTR_W-1:0]  asoA_data,  // captured counter value
  input wire asoA_ready
);

  // ============================================================
  // Free-running counter
  // ============================================================
  reg [CTR_W-1:0] ctr;
  always_ff @(posedge clk) begin
    if (reset)
      ctr <= '0;
    else
      ctr <= ctr + 1'b1;
  end

  // ============================================================
  // Synchronize and edge-detect the asynchronous input
  // ============================================================
  reg s1, s2;
  always_ff @(posedge clk) begin
    if (reset) begin
      s1 <= 1'b0;
      s2 <= 1'b0;
    end else begin
      s1 <= sig;
      s2 <= s1;
    end
  end

  wire signal_rise = s1 & ~s2;

  reg sA1, sA2;
  always_ff @(posedge clk) begin
    if (reset) begin
      sA1 <= 1'b0;
      sA2 <= 1'b0;
    end else begin
      sA1 <= sigA;
      sA2 <= sA1;
    end
  end

  wire signalA_rise = sA1 & ~sA2;

  // ============================================================
  // Capture timestamp on rising edge
  // ============================================================
  reg [CTR_W-1:0] ctr_cap;
  reg             cap_valid;

  always_ff @(posedge clk) begin
    if (reset) begin
      ctr_cap   <= '0;
      cap_valid <= 1'b0;
    end else begin
      cap_valid <= signal_rise;
      if (signal_rise)
        ctr_cap <= ctr;
    end
  end

  reg [CTR_W-1:0] ctr_capA;
  reg             cap_validA;

  always_ff @(posedge clk) begin
    if (reset) begin
      ctr_capA   <= '0;
      cap_validA <= 1'b0;
    end else begin
      cap_validA <= signalA_rise;
      if (signalA_rise)
        ctr_capA <= ctr;
    end
  end

  // ============================================================
  // Avalon-ST output
  // ============================================================
  assign aso_valid = cap_valid & aso_ready; // do not assert valid if the FIFO is not ready
  assign aso_data  = ctr_cap;

  assign asoA_valid = cap_validA & asoA_ready; // do not assert valid if the FIFO is not ready
  assign asoA_data  = ctr_capA;

endmodule

`default_nettype wire
