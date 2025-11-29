// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`default_nettype none // turn off implicit data types

module time_counter #(
  parameter width_bus = 32,
  parameter width_ctr = 64
)(
  input wire clk,
  input wire reset,

  input  wire start_async,
  input  wire stop_async,

  output reg ready,
  input wire high_low,
  output reg [width_bus-1:0] result
);

logic [width_ctr-1:0] elapsed;
logic valid;

// --- Synchronizers ---
(* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED" *) logic start_sync_1, start_sync;
(* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED" *) logic stop_sync_1,  stop_sync;

always_ff @(posedge clk) begin
  if (reset) begin
    start_sync_1 <= 0; start_sync <= 0;
    stop_sync_1  <= 0; stop_sync  <= 0;
  end else begin
    start_sync_1 <= start_async;
    start_sync   <= start_sync_1;
    stop_sync_1  <= stop_async;
    stop_sync    <= stop_sync_1;
  end
end

// --- Edge Detection ---
logic start_prev, stop_prev;
logic start_pulse, stop_pulse;

always_ff @(posedge clk) begin
  if (reset) begin
    start_prev <= 0; stop_prev <= 0;
    start_pulse <= 0; stop_pulse <= 0;
  end else begin
    start_prev <= start_sync;
    stop_prev  <= stop_sync;
    start_pulse <= start_sync & ~start_prev;
    stop_pulse  <= stop_sync  & ~stop_prev;
  end
end

// --- Counter and State ---
logic [width_ctr-1:0] counter;
logic running;

always_ff @(posedge clk) begin
  if (reset) begin
    counter <= 0;
    running <= 0;
    elapsed <= 0;
    valid   <= 0;
  end else begin
    valid <= 0; // default, asserted only for one cycle
    if (start_pulse) begin
      // restart measurement on new start
      counter <= 0;
      running <= 1;
    end else if (running) begin
      counter <= counter + 1;
    end
    if (stop_pulse && running) begin
      elapsed <= counter + 1;
      valid   <= 1;       // signal that result is ready
      running <= 0;       // stop counting
    end
  end
end

// Interfacing
always_ff @(posedge clk) begin
  if (reset) begin
    ready <= 0;
  end else if (valid) begin
    ready <= 1;
  end
end

always_ff @(posedge clk)
  result <= high_low ? elapsed[2*width_bus-1:width_bus] : elapsed[width_bus-1:0];

endmodule
