// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Avalon-ST multiplexer

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

logic channel; // 0 = channel1, 1 = channel2
logic [63:0] ctr1;
logic [63:0] ctr2;

// Gather statistics
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

assign aso_data  = (channel == 1'b1 ? asi_data2  : asi_data1);
assign aso_valid = (channel == 1'b1 ? asi_valid2 : asi_valid1);
assign asi_ready1 = (channel == 1'b0 ? aso_ready : 1'b0);
assign asi_ready2 = (channel == 1'b1 ? aso_ready : 1'b0);
assign aso_channel = channel;

always_ff @(posedge clk) begin
  if (reset) begin
    channel <= 0;
  end else if (avs_s0_write) begin
    case (avs_s0_address)
      3'b0: channel <= avs_s0_writedata[0];
    endcase
  end
end
                  
always_ff @(posedge clk) begin
  if (reset) begin
    avs_s0_readdata <= 0;
  end else if (avs_s0_read) begin
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
