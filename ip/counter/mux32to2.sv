// SPDX-License-Identifier: MIT
// 32-to-2 Multiplexer

`default_nettype none

module mux32to2 (
    input  logic [31:0] in,
    input  logic [4:0]  sel1,   // selector for output 1
    input  logic [4:0]  sel2,   // selector for output 2
    output logic        out1,   // output 1
    output logic        out2    // output 2
);

    assign out1 = in[sel1];
    assign out2 = in[sel2];

endmodule

`default_nettype wire
