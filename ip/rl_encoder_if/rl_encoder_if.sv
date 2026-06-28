// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

// Software-visible wrapper around the readback run-length encoder.
//
// Responsibilities of this module:
//   - connect sampled output data to the RL encoder core
//   - expose encoded runs on Avalon-ST for software readout
//   - provide a small Avalon-MM register file for reset, mode, pulse count, CRC, and overflow status
//
// Architectural overview lives in `ip/rl_encoder_if/README.md` and `docs/docs/readback.md`.

`default_nettype none // turn off implicit data types
`include "rl_config.vh"

module rl_encoder_if
(
input wire clk,
input wire reset,

output wire [cfg::WIDTH_TOTAL-1:0] aso_data,
output wire aso_valid,
input wire aso_ready,

input wire [1:0] avs_s0_address,
input wire avs_s0_read,
input wire avs_s0_write,
output reg [cfg::WIDTH_AVS-1:0] avs_s0_readdata,
input wire [cfg::WIDTH_AVS-1:0] avs_s0_writedata,

input wire [cfg::WIDTH_DATA-1:0] qin,
input wire qin_valid,
input wire qin_strobe,
input wire qin_clk
);

logic rdreq;         // Avalon-ST dequeue handshake
logic empty;         // 1 if the FIFO buffer is empty
logic reset_counter; // reset rl_encoder; clears the FIFO
logic mode;          // 1 = valid/qin_clk; 0 = dormant strobe mode behind WEIRD_CLOCK

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

assign aso_valid = ~empty;
assign rdreq = aso_valid && aso_ready; // dequeue only when downstream accepts valid data

// Clock selection for the auxiliary pulse counter and CRC path.
logic input_clk;
logic is_valid;

`ifdef WEIRD_CLOCK
  // Optional strobe-clocked readback mode. Normal builds keep this hidden because
  // qin_clk/valid sampling is the only supported mode.
  assign input_clk = mode ? qin_clk : qin_strobe;
  assign is_valid = (mode == 1 && qin_valid) || (mode == 0);
`else
  assign input_clk = qin_clk;
  assign is_valid = qin_valid;
`endif

logic input_cdc_reset;
logic input_reset;

cdc_bit_sync #(.RESET_VALUE(1'b1)) input_cdc_reset_sync (
  .dst_clk(input_clk),
  .dst_reset(reset),
  .async_in(reset),
  .sync_out(input_cdc_reset)
);

cdc_bit_sync #(.RESET_VALUE(1'b1)) input_reset_sync (
  .dst_clk(input_clk),
  .dst_reset(reset),
  .async_in(reset | reset_counter),
  .sync_out(input_reset)
);

// Pulse counter counts observed input samples, not encoded output elements.
logic [cfg::WIDTH_COUNTER-1:0] counter;
always_ff @(posedge input_clk) begin
  if (input_reset) begin // global or rl_encoder-specific reset
    counter <= 0;
  end else begin
    if (is_valid) begin
      counter <= counter + 1;
    end
  end
end

// CRC is computed over the observed data words in the sampled-input domain.
logic [31:0] crc_out;
logic        crc_valid;

crc32 crc32_inst (
 .clk(input_clk),
 .reset(input_reset),
 .data_en(is_valid),
 .data_in(qin[31:0]),
 .crc_out(crc_out),
 .crc_valid(crc_valid)
);

localparam int STATUS_SNAPSHOT_W = 1 + cfg::WIDTH_COUNTER + 32;
logic [STATUS_SNAPSHOT_W-1:0] status_input;
logic [STATUS_SNAPSHOT_W-1:0] status_clk;
logic overflow_clk;
logic [cfg::WIDTH_COUNTER-1:0] counter_clk;
logic [31:0] crc_out_clk;

assign status_input = {overflow, counter, crc_out};
assign {overflow_clk, counter_clk, crc_out_clk} = status_clk;

cdc_snapshot #(.WIDTH(STATUS_SNAPSHOT_W)) status_snapshot (
  .src_clk(input_clk),
  .src_reset(input_cdc_reset),
  .src_data(status_input),
  .dst_clk(clk),
  .dst_reset(reset),
  .dst_data(status_clk),
  .dst_valid()
);

always_ff @(posedge clk) begin
  if (reset) begin
    reset_counter <= 0;
    mode <= 1'b1;
  end else if (avs_s0_write) begin
    // The control interface is intentionally small: reset and mode select are the only writable knobs.
    unique case (avs_s0_address)
      2'b00: {reset_counter} <= avs_s0_writedata[0];
      2'b01: {mode}          <= avs_s0_writedata[0];
      default: ;
    endcase
  end
end

always_ff @(posedge clk) begin
  if (reset) begin
    avs_s0_readdata <= 0;
  end else if (avs_s0_read) begin
    // Software can inspect FIFO-empty state, encoder mode, overflow, observed sample count, and CRC.
    unique case (avs_s0_address)
      2'b00: avs_s0_readdata <= $bits(avs_s0_readdata)'({ overflow_clk, mode, reset_counter, empty });
      2'b01: avs_s0_readdata <= counter_clk;
      2'b10: avs_s0_readdata <= crc_out_clk;
      default: avs_s0_readdata <= 0;
    endcase
  end
end

endmodule

`default_nettype wire
