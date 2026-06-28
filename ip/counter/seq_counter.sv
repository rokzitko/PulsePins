// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`default_nettype none // turn off implicit data types

// Histogram counter for short binary sequences.
//
// This block observes one selected channel and counts how often each bit pattern of a
// fixed window length appears. It supports two modes:
//   - rolling windows (`rolling=1`): overlapping subsequences
//   - snapshot windows (`rolling=0`): non-overlapping chunks
//
// The resulting histogram is stored as an array of counters indexed by the observed bit
// pattern. A latched copy is exposed to software through `counter_if.sv`.

module seq_counter
#(
  parameter length = 1,          // size of window in bits
  parameter width_addr = length, // address for reading counters: same as length
  parameter width_bus = 32,      // Avalon MM data bus width
  parameter width_ctr = 64,      // Counter sizes
  parameter rolling = 1          // 1=rolling (overlapping), 0=snapshots (non-overlapping)
)(
  input wire clk,
  input wire clk_reset,
  input wire d_clk,
  input wire d_cdc_reset,
  input wire reset,
  input wire d,
  input wire valid,
  input wire latch,
  input wire high_low,
  input wire [width_addr-1:0] addr,
  output logic [width_bus-1:0] result
);

localparam [length-1:0] max_nr = {length{1'b1}}; // last histogram index
localparam width = $clog2(length+1);             // number of bits needed for the fill counter

logic [width_ctr-1:0] ctr [0:max_nr];   // internal counters
logic [width_ctr-1:0] ctr_r [0:max_nr]; // registered counters for readout
logic [width-1:0] l;  // current data block length counter
logic [length-1:0] w; // current data window; used as histogram index when wr=1
logic wr;             // write flag

// Pipeline input so the window generator and counter update remain simple.
logic d_reg, valid_reg;
always_ff @(posedge d_clk) begin
  if (reset) begin
    d_reg <= 0;
    valid_reg <= 0;
  end else begin
    d_reg <= d;
    valid_reg <= valid;
  end
end

// Pipeline histogram write requests by one cycle.
logic [length-1:0]  w_q;
logic               wr_q;

always_ff @(posedge d_clk) begin
  if (reset) begin
    w_q  <= '0;
    wr_q <= 1'b0;
  end else begin
    w_q  <= w;
    wr_q <= wr;
  end
end

integer i;
always_ff @(posedge d_clk) begin
  if (reset) begin
    for (i = 0; i <= max_nr; i++)
      ctr[i] <= '0;
  end else begin
    if (wr_q)
      ctr[w_q] <= ctr[w_q]+1;
  end
end

always_ff @(posedge d_clk) begin
  if (reset) begin
    l <= 0;
    w <= 0;
    wr <= 0;
  end else if (!valid_reg) begin
    l <= 0;
    w <= 0;
    wr <= 0;
  end else if (valid_reg) begin
    if (rolling == 1) begin
      // The window moves from MSB to LSB, so neighboring histogram samples overlap.
      w <= {w[length-2:0], d_reg};
      if (l < length) begin
        l <= l+1;
        if (l == length-1)
          wr <= 1;
        else
          wr <= 0;
      end else begin
        wr <= 1;
      end
    end else begin // rolling == 0
      // Build a non-overlapping chunk by filling the window from MSB to LSB.
      w[length-l-1] <= d_reg; // fill from MSB to LSB
      if (l == length-1) begin
        wr <= 1;
        l <= 0;
      end else begin
        wr <= 0;
        l <= l+1;
      end
    end
  end
end

localparam int SNAPSHOT_COUNTERS = 1 << length;
localparam int SNAPSHOT_W = SNAPSHOT_COUNTERS*width_ctr;
logic [SNAPSHOT_W-1:0] snapshot_d;
logic [SNAPSHOT_W-1:0] snapshot_clk;

integer pack_i;
always_comb begin
  for (pack_i = 0; pack_i < SNAPSHOT_COUNTERS; pack_i++) begin
    snapshot_d[pack_i*width_ctr +: width_ctr] = ctr[pack_i];
    ctr_r[pack_i] = snapshot_clk[pack_i*width_ctr +: width_ctr];
  end
end

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
always_comb
  v = ctr_r[addr];

always_ff @(posedge clk) begin
  if (clk_reset)
    result <= '0;
  else
    result <= high_low ? v[2*width_bus-1:width_bus] : v[width_bus-1:0];
end

endmodule
