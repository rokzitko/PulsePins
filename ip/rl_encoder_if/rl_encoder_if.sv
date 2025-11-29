// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Avalon-ST/MM interface for RL encoder

`default_nettype none // turn off implicit data types
`include "rl_config.vh"

module rl_encoder_if
(
input wire clk,
input wire reset,

output wire [cfg::WIDTH_TOTAL-1:0] aso_data,
output wire aso_valid,
input wire aso_ready,

input wire avs_s0_address,
input wire avs_s0_read,
input wire avs_s0_write,
output reg [cfg::WIDTH_AVS-1:0] avs_s0_readdata,
input wire [cfg::WIDTH_AVS-1:0] avs_s0_writedata,

input wire [cfg::WIDTH_DATA-1:0] qin,
input wire qin_valid,
input wire qin_strobe,
input wire qin_clk
);

logic rdreq;         // handshaking signal
logic empty;         // 1 if the FIFO buffer is empty
logic reset_counter; // reset rl_encoder; clears the FIFO
logic mode;          // 0 = valid, 1 = strobe

logic [cfg::WIDTH_DATA+cfg::WIDTH_COUNTER-1:0] j;
logic overflow;

rl_encoder rl0 (
.clk(clk),
.reset(reset | reset_counter), // global or rl_encoder-specific reset

.mode(mode),

.data(qin),
.valid(qin_valid),
.strobe(qin_strobe),
.data_clk(qin_clk),

.element(j),
.rdreq(rdreq),
.empty(empty),

.overflow
);

assign aso_data = { {cfg::WIDTH_CONTROL{1'b0}}, j };

assign rdreq = aso_ready && ~empty; // handle backpressure
assign aso_valid = rdreq;

// Clock selection
logic input_clk;
logic is_valid;

`ifdef WEIRD_CLOCK
  assign input_clk = mode ? qin_clk : qin_strobe;
  assign is_valid = (mode == 1 && qin_valid) || (mode == 0);
`else
  assign input_clk = qin_clk;
  assign is_valid = qin_valid;
`endif

// Pulse counter
logic [cfg::WIDTH_COUNTER-1:0] counter;
always_ff @(posedge input_clk) begin
  if (reset || reset_counter) begin // global or rl_encoder-specific reset
    counter <= 0;
  end else begin
    if (is_valid) begin
      counter <= counter + 1;
    end
  end
end

always_ff @(posedge clk) begin
  if (reset) begin
    reset_counter <= 0;
  end else if (avs_s0_write) begin
    unique case (avs_s0_address)
      1'b0: {reset_counter} <= avs_s0_writedata[0];
      1'b1: {mode}          <= avs_s0_writedata[0];
    endcase
  end
end

always_ff @(posedge clk) begin
  if (reset) begin
    avs_s0_readdata <= 0;
  end else if (avs_s0_read) begin
    unique case (avs_s0_address)
      1'b0: avs_s0_readdata <= { overflow, mode, reset_counter, empty };
      1'b1: avs_s0_readdata <= counter;
    endcase
  end
end

endmodule

`default_nettype wire
