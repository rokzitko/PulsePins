`default_nettype none

// Dual-path timestamp capture core.
//
// This block runs a free-running counter in `clk` and captures that counter value on the
// rising edge of two asynchronous inputs, `sig` and `sigA`. Each capture path drives its
// own Avalon-ST output so software can consume the PPS path and the auxiliary path
// independently.
//
// Captured events are held until the downstream FIFO accepts them. If a second event arrives
// while a previous event on the same path is still pending, the event is dropped and reported
// through the Avalon-MM overflow status/count registers.

module ts_core #(
  parameter CTR_W = 64                 // counter width
) (
  // Clock and reset
  input  wire              clk,        // timebase clock (e.g. 100 MHz)
  input  wire              reset,      // active-high synchronous reset

  // Asynchronous signal input
  input  wire              sig,        // edge-detected input (asynchronous)
  input  wire              sigA,       // edge-detected input (asynchronous)

  // Avalon-MM status/control interface
  input  wire [1:0]        avs_s0_address,
  input  wire              avs_s0_read,
  input  wire              avs_s0_write,
  output reg  [31:0]       avs_s0_readdata,
  input  wire [31:0]       avs_s0_writedata,

  // Avalon-ST output interface
  output wire              aso_valid,  // asserted while a captured event is pending
  output wire [CTR_W-1:0]  aso_data,   // captured counter value
  input wire aso_ready,

  output wire              asoA_valid, // asserted while a captured event is pending
  output wire [CTR_W-1:0]  asoA_data,  // captured counter value
  input wire asoA_ready
);

  localparam logic [1:0] A_STATUS = 2'd0;
  localparam logic [1:0] A_CONTROL = 2'd1;
  localparam logic [1:0] A_OVERFLOW_COUNT = 2'd2;
  localparam logic [1:0] A_OVERFLOWA_COUNT = 2'd3;

  function automatic logic [31:0] saturated_increment(input logic [31:0] value);
    saturated_increment = (value == 32'hffff_ffff) ? value : value + 32'd1;
  endfunction

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
  // Synchronize and edge-detect the asynchronous inputs.
  // ============================================================
  reg s1, s2, s3;
  always_ff @(posedge clk) begin
    if (reset) begin
      s1 <= 1'b0;
      s2 <= 1'b0;
      s3 <= 1'b0;
    end else begin
      s1 <= sig;
      s2 <= s1;
      s3 <= s2;
    end
  end

  wire signal_rise = s2 & ~s3;

  reg sA1, sA2, sA3;
  always_ff @(posedge clk) begin
    if (reset) begin
      sA1 <= 1'b0;
      sA2 <= 1'b0;
      sA3 <= 1'b0;
    end else begin
      sA1 <= sigA;
      sA2 <= sA1;
      sA3 <= sA2;
    end
  end

  wire signalA_rise = sA2 & ~sA3;

  // ============================================================
  // Capture and hold events until the downstream Avalon-ST sink accepts them.
  // ============================================================
  logic [CTR_W-1:0] aso_data_r, aso_data_next;
  logic             aso_valid_r, aso_valid_next;
  logic             overflow_r, overflow_next;
  logic [31:0]      overflow_count_r, overflow_count_next;

  logic [CTR_W-1:0] asoA_data_r, asoA_data_next;
  logic             asoA_valid_r, asoA_valid_next;
  logic             overflowA_r, overflowA_next;
  logic [31:0]      overflowA_count_r, overflowA_count_next;

  wire aso_accept = aso_valid_r && aso_ready;
  wire asoA_accept = asoA_valid_r && asoA_ready;
  wire clear_overflow = avs_s0_write && avs_s0_address == A_CONTROL && avs_s0_writedata[0];
  wire clear_overflowA = avs_s0_write && avs_s0_address == A_CONTROL && avs_s0_writedata[1];

  always_comb begin
    aso_data_next = aso_data_r;
    aso_valid_next = aso_valid_r;
    overflow_next = clear_overflow ? 1'b0 : overflow_r;
    overflow_count_next = clear_overflow ? 32'd0 : overflow_count_r;

    if (signal_rise) begin
      if (aso_valid_r && !aso_ready) begin
        overflow_next = 1'b1;
        overflow_count_next = saturated_increment(overflow_count_next);
      end else begin
        aso_data_next = ctr;
        aso_valid_next = 1'b1;
      end
    end else if (aso_accept) begin
      aso_valid_next = 1'b0;
    end
  end

  always_comb begin
    asoA_data_next = asoA_data_r;
    asoA_valid_next = asoA_valid_r;
    overflowA_next = clear_overflowA ? 1'b0 : overflowA_r;
    overflowA_count_next = clear_overflowA ? 32'd0 : overflowA_count_r;

    if (signalA_rise) begin
      if (asoA_valid_r && !asoA_ready) begin
        overflowA_next = 1'b1;
        overflowA_count_next = saturated_increment(overflowA_count_next);
      end else begin
        asoA_data_next = ctr;
        asoA_valid_next = 1'b1;
      end
    end else if (asoA_accept) begin
      asoA_valid_next = 1'b0;
    end
  end

  always_ff @(posedge clk) begin
    if (reset) begin
      aso_data_r <= '0;
      aso_valid_r <= 1'b0;
      overflow_r <= 1'b0;
      overflow_count_r <= 32'd0;

      asoA_data_r <= '0;
      asoA_valid_r <= 1'b0;
      overflowA_r <= 1'b0;
      overflowA_count_r <= 32'd0;
    end else begin
      aso_data_r <= aso_data_next;
      aso_valid_r <= aso_valid_next;
      overflow_r <= overflow_next;
      overflow_count_r <= overflow_count_next;

      asoA_data_r <= asoA_data_next;
      asoA_valid_r <= asoA_valid_next;
      overflowA_r <= overflowA_next;
      overflowA_count_r <= overflowA_count_next;
    end
  end

  assign aso_valid = aso_valid_r;
  assign aso_data  = aso_data_r;

  assign asoA_valid = asoA_valid_r;
  assign asoA_data  = asoA_data_r;

  // ============================================================
  // Avalon-MM status/control register file.
  // ============================================================
  logic [31:0] status_word;
  always_comb begin
    status_word = 32'd0;
    status_word[0] = aso_valid_r;
    status_word[1] = asoA_valid_r;
    status_word[8] = overflow_r;
    status_word[9] = overflowA_r;
  end

  always_ff @(posedge clk) begin
    if (reset) begin
      avs_s0_readdata <= 32'd0;
    end else if (avs_s0_read) begin
      unique case (avs_s0_address)
        A_STATUS:          avs_s0_readdata <= status_word;
        A_CONTROL:         avs_s0_readdata <= 32'd0;
        A_OVERFLOW_COUNT:  avs_s0_readdata <= overflow_count_r;
        A_OVERFLOWA_COUNT: avs_s0_readdata <= overflowA_count_r;
      endcase
    end
  end

endmodule

`default_nettype wire
