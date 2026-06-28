// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Software-visible Avalon-ST multiplexer.
//
// Responsibilities of this block:
//   - select one of two Avalon-ST input streams
//   - forward only the selected stream to the shared output
//   - provide backpressure only to the selected input
//   - count successful transfers on each input path
//
// Architectural overview lives in `ip/st_mux/README.md` and `docs/docs/st_mux.md`.

`default_nettype none

module st_mux_if(
input wire clk,
input wire reset,

// sinks
input wire [31:0] asi_data1,
input wire asi_valid1,
input wire asi_channel1,
output reg asi_ready1,

input wire [31:0] asi_data2,
input wire asi_valid2,
input wire asi_channel2,
output reg asi_ready2,

// source
output wire [31:0] aso_data,
output wire aso_valid,
output wire aso_channel,
input wire aso_ready,

// Avalon-MM port for control
input wire [2:0] avs_s0_address,
input wire avs_s0_read,
input wire avs_s0_write,
output reg [31:0] avs_s0_readdata,
input wire [31:0] avs_s0_writedata
);

logic channel; // active channel: 0 = channel1, 1 = channel2
logic requested_channel;
logic [63:0] ctr1;
logic [63:0] ctr2;

// Count successful handshakes on each input path.
always @(posedge clk) begin
  if (reset) begin
    ctr1 <= 0;
    ctr2 <= 0;
  end else begin
    if (asi_valid1 && asi_ready1) begin // both must be high!
      ctr1 <= ctr1+1;
    end
    if (asi_valid2 && asi_ready2) begin
      ctr2 <= ctr2+1;
    end
  end
end

// The mux is purely combinational in the data path; `channel` only controls selection.
assign aso_data  = (channel == 1'b1 ? asi_data2  : asi_data1);
assign aso_valid = (channel == 1'b1 ? asi_valid2 : asi_valid1);
assign asi_ready1 = (channel == 1'b0 ? aso_ready : 1'b0);
assign asi_ready2 = (channel == 1'b1 ? aso_ready : 1'b0);
assign aso_channel = channel;

wire selected_stalled = aso_valid && !aso_ready;
logic requested_channel_next;

always_comb begin
  requested_channel_next = requested_channel;
  if (avs_s0_write && avs_s0_address == 3'b0) begin
    requested_channel_next = avs_s0_writedata[0];
  end
end

always_ff @(posedge clk) begin
  if (reset) begin
    channel <= 1'b0;
    requested_channel <= 1'b0;
  end else begin
    requested_channel <= requested_channel_next;
    if (!selected_stalled) begin
      channel <= requested_channel_next;
    end
  end
end

always_ff @(posedge clk) begin
  if (reset) begin
    avs_s0_readdata <= 0;
  end else if (avs_s0_read) begin
    // Readback exposes the 64-bit transfer counters as low/high 32-bit words.
    case (avs_s0_address)
      3'b000:  avs_s0_readdata <= ctr1[31:0];
      3'b001:  avs_s0_readdata <= ctr1[63:32];
      3'b010:  avs_s0_readdata <= ctr2[31:0];
      3'b011:  avs_s0_readdata <= ctr2[63:32];
      default: avs_s0_readdata <= 0;
    endcase
  end
end

endmodule: st_mux_if

`default_nettype wire
