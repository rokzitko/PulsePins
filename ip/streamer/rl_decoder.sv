// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Rok Zitko

// Runlength decoder core

//`default_nettype none // turn off implicit data types
`include "config.vh"

// Show-ahead input FIFO (in_valid/rdreq) -> masked RLE decode -> throttled output FIFO
module rl_decoder
(
  input  wire                     clk,
  input  wire                     reset,          // active-high reset

  // Input FIFO (show-ahead semantics)
  input  wire                     in_valid,       // tuple valid (not empty)
  input  wire [WIDTH_DATA-1:0]    in_data,
  input  wire [WIDTH_CONTROL-1:0] in_control,     // carried through, unmodified
  input  wire [WIDTH_COUNTER-1:0] in_counter,     // repeat count
  input  wire [3:0]               in_opmode,      // 00 load, 01 clear, 10 set, 11 flip, etc.
  output reg                      in_rdreq,       // pop current tuple on next clk

  // Output FIFO (throttled)
  input  wire                     out_almost_full,
  output reg [WIDTH_DATA-1:0]         out_data,
  output reg [WIDTH_CONTROL-1:0]      out_control,
  output reg                      out_wrreq,

  // Initial previous value for the very first effective run
  input  wire [WIDTH_DATA-1:0]        initial_data
);

  // ---- op(mode) application: combines previous output with incoming data ----
  function automatic logic [WIDTH_DATA-1:0]
  apply_op(input logic [WIDTH_DATA-1:0] prev,
           input logic [WIDTH_DATA-1:0] dat,
           input logic [3:0]        opmode);
    unique case (bit_op_t'(opmode))
      BITLOAD:  apply_op = dat;           // load
      BITSET:   apply_op = prev |  dat;   // set   bits in mask (OR)
      BITCLEAR: apply_op = prev & ~dat;   // clear bits in mask
      BITFLIP:  apply_op = prev ^  dat;   // flip  bits in mask (XOR)
      BITNOT:   apply_op = ~prev;         // invert all bits
      BITAND:   apply_op = prev & dat;    // bitwise AND
      BITOR:    apply_op = prev | dat;    // bitwise OR (same as BITSET)
      BITXOR:   apply_op = prev ^ dat;    // bitwise XOR (same as BITFLIP)
      BITXNOR:  apply_op = prev ^~ dat;   // bitwise XNOR (equivalence)
      BITSLL:   apply_op = prev << dat;   // shift left logical
      BITSRL:   apply_op = prev >> dat;   // shift right logical
      default:   apply_op = dat;           // BITLOAD
    endcase
  endfunction

  // ---- State ----
  logic [WIDTH_DATA-1:0]    prev_value;      // last emitted element
  logic [WIDTH_DATA-1:0]    curr_value;      // value being repeated
  logic [WIDTH_COUNTER-1:0] curr_cnt;        // remaining repeats
  logic [WIDTH_CONTROL-1:0] curr_control;    // pass-through control for this run

  // Convenience
  wire can_emit = (curr_cnt != '0) && !out_almost_full;

  // ---- Main process (no prefetch; bubbles allowed between runs) ----
  always_ff @(posedge clk) begin
    if (reset) begin
      prev_value  <= initial_data;
      curr_value  <= '0;
      curr_cnt    <= '0;
      curr_control<= '0;
      out_wrreq   <= 1'b0;
      out_data    <= '0;
      out_control <= '0;
      in_rdreq    <= 1'b0;
    end else begin
      // defaults each cycle
      out_wrreq <= 1'b0;
      in_rdreq  <= 1'b0;

      // 1) Emit when we have repeats and sink is ready
      if (can_emit) begin
        out_wrreq   <= 1'b1;
        out_data    <= curr_value;
        out_control <= curr_control;
        curr_cnt    <= curr_cnt - 1'b1;
        prev_value  <= curr_value;     // track last emitted element
      end

      // 2) If current run is exhausted, try to load a new one (one bubble acceptable)
      if (curr_cnt == '0) begin
        if (in_valid) begin
          if (in_counter == '0) begin
            // Skip zero-length run (bubble); pop and look again next cycle
            in_rdreq <= 1'b1;
          end else begin
            // Consume tuple and begin a new run
            in_rdreq     <= 1'b1;
            curr_value   <= apply_op(prev_value, in_data, in_opmode);
            curr_cnt     <= in_counter;
            curr_control <= in_control;
            // Note: first element will be emitted on a later cycle when !out_almost_full
          end
        end
      end
    end
  end

  // ---- Simulation guards ----
  // synthesis translate_off
  always_ff @(posedge clk) if (!reset) begin
    assert(!(in_rdreq && !in_valid))
      else $fatal("in_rdreq asserted when input not valid");
  end
  // synthesis translate_on

endmodule
