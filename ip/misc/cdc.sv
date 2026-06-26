// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

// Reusable clock-domain crossing helpers.
//
// These helpers cover the common PulsePins CDC cases:
// - individual level bits,
// - latest-value multi-bit configuration updates,
// - continuously refreshed multi-bit readback snapshots.

`default_nettype none

module cdc_bit_sync #(
  parameter int STAGES = 3,
  parameter bit RESET_VALUE = 1'b0
)(
  input  wire dst_clk,
  input  wire dst_reset,
  input  wire async_in,
  output wire sync_out
);

  initial assert (STAGES >= 2)
    else $error("cdc_bit_sync requires at least two stages");

  (*
  altera_attribute = {
    "-name SYNCHRONIZER_IDENTIFICATION \"FORCED IF ASYNCHRONOUS\"; ",
    "-name DONT_MERGE_REGISTER ON; ",
    "-name PRESERVE_REGISTER ON" }
  *) logic [STAGES-1:0] sync_ff;

  always_ff @(posedge dst_clk) begin
    if (dst_reset)
      sync_ff <= {STAGES{RESET_VALUE}};
    else
      sync_ff <= {sync_ff[STAGES-2:0], async_in};
  end

  assign sync_out = sync_ff[STAGES-1];

endmodule

module cdc_bus_update #(
  parameter int WIDTH = 1,
  parameter logic [WIDTH-1:0] RESET_VALUE = '0
)(
  input  wire             src_clk,
  input  wire             src_reset,
  input  wire [WIDTH-1:0] src_data,
  input  wire             src_update,
  output wire             src_busy,

  input  wire             dst_clk,
  input  wire             dst_reset,
  input  wire             dst_accept,
  output logic [WIDTH-1:0] dst_data,
  output logic             dst_valid,
  output wire              dst_pending
);

  // src_reset and dst_reset must represent the same logical reset event for this toggle
  // handshake. If only the payload-producing logic resets, keep this helper out of reset so
  // request/ack phase is preserved.

  // Source holds src_send_data stable from request toggle until the destination has sampled it
  // and returned the acknowledgement toggle. Additional source updates collapse to latest value.
  logic [WIDTH-1:0] src_send_data;
  logic [WIDTH-1:0] src_pending_data;
  logic             src_pending_valid;
  logic             src_req_tgl;
  logic             src_inflight;

  logic src_ack_sync_d;
  logic dst_ack_tgl;

  wire src_ack_event = src_ack_sync ^ src_ack_sync_d;
  wire src_can_send = !src_inflight || src_ack_event;

  (*
  altera_attribute = {
    "-name SYNCHRONIZER_IDENTIFICATION \"FORCED IF ASYNCHRONOUS\"; ",
    "-name DONT_MERGE_REGISTER ON; ",
    "-name PRESERVE_REGISTER ON" }
  *) logic src_ack_meta, src_ack_sync;

  always_ff @(posedge src_clk) begin
    if (src_reset) begin
      src_send_data       <= RESET_VALUE;
      src_pending_data    <= RESET_VALUE;
      src_pending_valid   <= 1'b0;
      src_req_tgl         <= 1'b0;
      src_inflight        <= 1'b0;
      src_ack_meta        <= 1'b0;
      src_ack_sync        <= 1'b0;
      src_ack_sync_d      <= 1'b0;
    end else begin
      src_ack_meta       <= dst_ack_tgl;
      src_ack_sync       <= src_ack_meta;
      src_ack_sync_d     <= src_ack_sync;

      if (src_ack_event)
        src_inflight <= 1'b0;

      if (src_update) begin
        src_pending_data  <= src_data;
        src_pending_valid <= 1'b1;
      end

      if (src_can_send && (src_update || src_pending_valid)) begin
        src_send_data     <= src_update ? src_data : src_pending_data;
        src_req_tgl       <= ~src_req_tgl;
        src_inflight      <= 1'b1;
        src_pending_valid <= 1'b0;
      end
    end
  end

  assign src_busy = src_inflight || src_pending_valid;

  (*
  altera_attribute = {
    "-name SYNCHRONIZER_IDENTIFICATION \"FORCED IF ASYNCHRONOUS\"; ",
    "-name DONT_MERGE_REGISTER ON; ",
    "-name PRESERVE_REGISTER ON" }
  *) logic [1:0] dst_req_sync_chain;

  logic dst_req_sync_d;
  logic [WIDTH-1:0] dst_pending_data;
  logic             dst_pending_valid;

  wire dst_req_sync = dst_req_sync_chain[1];
  wire dst_req_event = dst_req_sync ^ dst_req_sync_d;

  always_ff @(posedge dst_clk) begin
    if (dst_reset) begin
      dst_req_sync_chain <= 2'b00;
      dst_req_sync_d     <= 1'b0;
      dst_ack_tgl        <= 1'b0;
      dst_data           <= RESET_VALUE;
      dst_valid          <= 1'b0;
      dst_pending_data   <= RESET_VALUE;
      dst_pending_valid  <= 1'b0;
    end else begin
      dst_req_sync_chain <= {dst_req_sync_chain[0], src_req_tgl};
      dst_req_sync_d     <= dst_req_sync;

      if (dst_req_event) begin
        dst_ack_tgl <= ~dst_ack_tgl;
        if (dst_accept) begin
          dst_data          <= src_send_data;
          dst_valid         <= 1'b1;
          dst_pending_valid <= 1'b0;
        end else begin
          dst_pending_data  <= src_send_data;
          dst_pending_valid <= 1'b1;
        end
      end else if (dst_accept && dst_pending_valid) begin
        dst_data          <= dst_pending_data;
        dst_valid         <= 1'b1;
        dst_pending_valid <= 1'b0;
      end
    end
  end

  assign dst_pending = dst_pending_valid;

endmodule

module cdc_snapshot #(
  parameter int WIDTH = 1,
  parameter logic [WIDTH-1:0] RESET_VALUE = '0
)(
  input  wire              src_clk,
  input  wire              src_reset,
  input  wire [WIDTH-1:0]  src_data,

  input  wire              dst_clk,
  input  wire              dst_reset,
  output wire [WIDTH-1:0]  dst_data,
  output wire              dst_valid
);

  cdc_bus_update #(
    .WIDTH(WIDTH),
    .RESET_VALUE(RESET_VALUE)
  ) update_inst (
    .src_clk,
    .src_reset,
    .src_data,
    .src_update(1'b1),
    .src_busy(),
    .dst_clk,
    .dst_reset,
    .dst_accept(1'b1),
    .dst_data,
    .dst_valid,
    .dst_pending()
  );

endmodule

`default_nettype wire
