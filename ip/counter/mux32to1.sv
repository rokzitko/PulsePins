// SPDX-License-Identifier: MIT
// 32-to-1 Multiplexer for single-bit inputs

`default_nettype none

module mux32to1 (
    input  logic [31:0] in,   // 32 single-bit inputs packed as a vector
    input  logic [4:0]  sel,  // 5-bit selector
    output logic        out   // single-bit output
);

    // Direct vector indexing (SystemVerilog feature)
    assign out = in[sel];

endmodule

`default_nettype wire
