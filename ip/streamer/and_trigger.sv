// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Simple trigger (synchronous)

`default_nettype none
`include "config.vh"

module and_trigger
#(
 parameter int width = `WIDTH_TRIGGER
)(
 input wire [width-1:0] i,       // trigger input signals
 input wire [width-1:0] pattern, // trigger pattern to be matched
 input wire [width-1:0] mask,    // trigger mask (1 for bits to which we are sensitive)
 input wire clk,                 // clock signal (conditions are checked when clk is asserted)
 input wire reset,               // trigger logic reset
 input wire trigger_enable,      // enable signal
 output reg o                    // goes 1 when trigger signal is detected (latched until the reset)
);

always_ff @(posedge clk)
  if (reset)
    o <= 1'b0;
  else
    if (trigger_enable)
      if (((i ~^ pattern) & mask) == mask)
        o <= 1'b1;

endmodule

`default_nettype wire
