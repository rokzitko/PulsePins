// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`default_nettype none // turn off implicit data types

// Packet/validity statistics for one sampled stream.
//
// This block interprets `valid` as a packet-presence qualifier and accumulates:
//   - total sampled clock ticks
//   - ticks with valid data
//   - idle ticks without valid data
//   - packet begin/end events
//   - sum and sum-of-squares of packet lengths
//
// The counters live in `d_clk` and are copied into readout registers on `latch` so the
// control side can read a stable snapshot through `counter_if.sv`.

module packet_stats
#(
  parameter width_addr = 3,
  parameter width_bus = 32,
  parameter width_ctr = 64
)(
  input wire clk,
  input wire clk_reset,
  input wire d_clk,
  input wire d_cdc_reset,
  input wire reset,
  input wire valid,
  input wire latch,
  input wire high_low,
  input wire [width_addr-1:0] addr,
  output logic [width_bus-1:0] result,
  output wire overflow
);

logic [width_ctr-1:0] ctr_total,        // total sampled clock ticks
                      ctr_valid,        // valid data counter
                      ctr_idle,         // idle (!valid) counter
                      ctr_pkt_begin,    // number of assertions of valid signal
                      ctr_pkt_end,      // number of deassertions of valid signal
                      ctr_pkt_len_sum,  // packet length (sum)
                      ctr_pkt_len_sum2; // packet length (sum squared)
logic [width_ctr-1:0] ctr_total_r, ctr_valid_r, ctr_idle_r,
                      ctr_pkt_begin_r, ctr_pkt_end_r,
                      ctr_pkt_len_sum_r, ctr_pkt_len_sum2_r;
logic [width_ctr-1:0] pkt_len;          // current packet length while `valid` stays high
logic valid_prev;

always_ff @(posedge d_clk) begin
  if (reset) begin
    ctr_total <= 0;
    ctr_valid <= 0;
    ctr_idle <= 0;
    ctr_pkt_begin <= 0;
    ctr_pkt_end <= 0;
    ctr_pkt_len_sum <= 0;
    ctr_pkt_len_sum2 <= 0;
    pkt_len <= 0;
    valid_prev <= 0; // not 'valid' here, avoid X propagation
  end else begin
    if (ctr_total != {width_ctr{1'b1}}) // saturate counter (overflow will also be asserted)
      ctr_total <= ctr_total+1;
    if (valid)
      ctr_valid <= ctr_valid+1;
    if (!valid)
      ctr_idle <= ctr_idle+1;
    if (valid && !valid_prev) begin
      // Start of a new packet.
      ctr_pkt_begin <= ctr_pkt_begin+1;
      pkt_len <= 1;
    end else if (!valid && valid_prev) begin
      // End of packet: commit the accumulated packet length statistics.
      ctr_pkt_end <= ctr_pkt_end+1;
      ctr_pkt_len_sum <= ctr_pkt_len_sum + pkt_len;
      ctr_pkt_len_sum2 <= ctr_pkt_len_sum2 + pkt_len*pkt_len;
      pkt_len <= 0;
    end else if (valid && valid_prev) begin
      pkt_len <= pkt_len+1;
    end
    valid_prev <= valid;
  end
end

logic overflow_d;

always_ff @(posedge d_clk) begin
  if (reset)
    overflow_d <= 0;
  else if (ctr_total == {width_ctr{1'b1}})
    overflow_d <= 1;
end

cdc_bit_sync overflow_sync (
  .dst_clk(clk),
  .dst_reset(clk_reset),
  .async_in(overflow_d),
  .sync_out(overflow)
);

localparam int SNAPSHOT_W = 7*width_ctr;
logic [SNAPSHOT_W-1:0] snapshot_d;
logic [SNAPSHOT_W-1:0] snapshot_clk;

assign snapshot_d = {ctr_pkt_len_sum2, ctr_pkt_len_sum, ctr_pkt_end,
                     ctr_pkt_begin, ctr_idle, ctr_valid, ctr_total};
assign {ctr_pkt_len_sum2_r, ctr_pkt_len_sum_r, ctr_pkt_end_r,
        ctr_pkt_begin_r, ctr_idle_r, ctr_valid_r, ctr_total_r} = snapshot_clk;

cdc_bus_update #(
  .WIDTH(SNAPSHOT_W),
  .RESET_VALUE('0)
) snapshot_cdc (
  .src_clk(d_clk),
  .src_reset(d_cdc_reset),
  .src_data(snapshot_d),
  .src_update(latch),
  .src_busy(),
  .dst_clk(clk),
  .dst_reset(clk_reset),
  .dst_accept(1'b1),
  .dst_data(snapshot_clk),
  .dst_valid(),
  .dst_pending()
);

logic [width_ctr-1:0] v;
always_comb begin
  case (addr)
    3'd0: v = ctr_total_r;
    3'd1: v = ctr_valid_r;
    3'd2: v = ctr_idle_r;
    3'd3: v = ctr_pkt_begin_r;
    3'd4: v = ctr_pkt_end_r;
    3'd5: v = ctr_pkt_len_sum_r;
    3'd6: v = ctr_pkt_len_sum2_r;
    default: v = '0;
  endcase
end

always_ff @(posedge clk) begin
  if (clk_reset)
    result <= '0;
  else
    result <= high_low ? v[2*width_bus-1:width_bus] : v[width_bus-1:0];
end

endmodule
