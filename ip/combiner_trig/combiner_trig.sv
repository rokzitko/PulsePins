// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Žitko

// Multiplexer with advanced features:
// - bit inversion on inputs and output
// - bit masking on inputs and output
// - overrides on inputs and output
// - multiplexing, bitwise logic operations, block combinations, arithmetic sums and differences
// - readback of inputs and output
// NOTE: Both inputs and outputs are registered; the signals will be delayed by two clock ticks.
// Rok Zitko, Sep 2025

module combiner_trig #(
 parameter int WIDTH = 11
)(
 input  wire [3:0]       avs_s0_address,
 input  wire             avs_s0_read,
 output reg  [31:0]      avs_s0_readdata,
 input  wire             avs_s0_write,
 input  wire [31:0]      avs_s0_writedata,
 input  wire             clock_clk,         // clock in the control domain
 input  wire             reset_reset,
 input  wire             clk,               // clock in the signal domain
 input  wire [WIDTH-1:0] in1,
 input  wire [WIDTH-1:0] in2,
 input  wire [WIDTH-1:0] in3,
 input  wire [WIDTH-1:0] in4,
 output reg  [WIDTH-1:0] o
);

logic [31:0] cfg; // configuration register

localparam WIDTH_MODE = 4;
logic [WIDTH_MODE-1:0] mode; // selected input in multiplexer mode or some combining operation
assign mode = cfg[WIDTH_MODE-1:0];

localparam [WIDTH_MODE-1:0] SEL1 = 4'd0, SEL2 = 4'd1, SEL3 = 4'd2, SEL4 = 4'd3,         // select
                            AND = 4'd4, OR = 4'd5, XOR = 4'd6, XNOR = 4'd7, MAJ = 4'd8, // logic operation
                            BLOCK8 = 4'd9, BLOCK16 = 4'd10,                             // blocking
                            SUM12 = 4'd11, SUM1234 = 4'd12,                             // algebraic sum
                            DIFF12 = 4'd13;                                             // algebraic difference

`define B_FORCEo 16
`define B_FORCE1 17
`define B_FORCE2 18
`define B_FORCE3 19
`define B_FORCE4 20

// true = put the value in valueo on the output port o instead of the computed value
logic forceo;
assign forceo = cfg[`B_FORCEo];

// true = use the value in value? instead of the value in the input port in?
logic force1, force2, force3, force4;
assign force1 = cfg[`B_FORCE1];
assign force2 = cfg[`B_FORCE2];
assign force3 = cfg[`B_FORCE3];
assign force4 = cfg[`B_FORCE4];

`define B_RBo 24
`define B_RB1 25
`define B_RB2 26
`define B_RB3 27
`define B_RB4 28

// false = read the value in value?
// true = read the value in the in/out port
logic rbo, rb1, rb2, rb3, rb4;
assign rbo = cfg[`B_RBo];
assign rb1 = cfg[`B_RB1];
assign rb2 = cfg[`B_RB2];
assign rb3 = cfg[`B_RB3];
assign rb4 = cfg[`B_RB4];

logic [WIDTH-1:0] inverto, invert1, invert2, invert3, invert4; // inversion patterns
logic [WIDTH-1:0] masko, mask1, mask2, mask3, mask4;           // filter masks
logic [WIDTH-1:0] valueo, value1, value2, value3, value4;      // override values

// Avalon MM port addresses
`define C_CFG    4'b0000

`define C_INVo   4'b0001
`define C_INV1   4'b0100
`define C_INV2   4'b0101
`define C_INV3   4'b0110
`define C_INV4   4'b0111

`define C_MASKo  4'b0010
`define C_MASK1  4'b1000
`define C_MASK2  4'b1001
`define C_MASK3  4'b1010
`define C_MASK4  4'b1011

`define C_VALUEo 4'b0011
`define C_VALUE1 4'b1100
`define C_VALUE2 4'b1101
`define C_VALUE3 4'b1110
`define C_VALUE4 4'b1111

// Upon reset, the in1 is passed unmodified to output.

always_ff @(posedge clock_clk) begin
  if (reset_reset) begin
    cfg <= SEL1;
    inverto <= '0;
    invert1 <= '0;
    invert2 <= '0;
    invert3 <= '0;
    invert4 <= '0;
    masko <= ~'0; // all ones
    mask1 <= ~'0;
    mask2 <= ~'0;
    mask3 <= ~'0;
    mask4 <= ~'0;
    valueo <= '0;
    value1 <= '0;
    value2 <= '0;
    value3 <= '0;
    value4 <= '0;
  end else if (avs_s0_write) begin
    unique case (avs_s0_address)
      `C_CFG:  cfg      <= avs_s0_writedata;
      `C_INVo: inverto  <= avs_s0_writedata[WIDTH-1:0];
      `C_INV1: invert1  <= avs_s0_writedata[WIDTH-1:0];
      `C_INV2: invert2  <= avs_s0_writedata[WIDTH-1:0];
      `C_INV3: invert3  <= avs_s0_writedata[WIDTH-1:0];
      `C_INV4: invert4  <= avs_s0_writedata[WIDTH-1:0];
      `C_MASKo: masko   <= avs_s0_writedata[WIDTH-1:0];
      `C_MASK1: mask1   <= avs_s0_writedata[WIDTH-1:0];
      `C_MASK2: mask2   <= avs_s0_writedata[WIDTH-1:0];
      `C_MASK3: mask3   <= avs_s0_writedata[WIDTH-1:0];
      `C_MASK4: mask4   <= avs_s0_writedata[WIDTH-1:0];
      `C_VALUEo: valueo <= avs_s0_writedata[WIDTH-1:0];
      `C_VALUE1: value1 <= avs_s0_writedata[WIDTH-1:0];
      `C_VALUE2: value2 <= avs_s0_writedata[WIDTH-1:0];
      `C_VALUE3: value3 <= avs_s0_writedata[WIDTH-1:0];
      `C_VALUE4: value4 <= avs_s0_writedata[WIDTH-1:0];
    endcase
  end
end

always_ff @(posedge clock_clk) begin
  if (reset_reset) begin
    avs_s0_readdata <= 0;
  end else if (avs_s0_read) begin
    case (avs_s0_address)
      `C_CFG:      avs_s0_readdata <= cfg;
      `C_INVo:     avs_s0_readdata <= inverto;
      `C_INV1:     avs_s0_readdata <= invert1;
      `C_INV2:     avs_s0_readdata <= invert2;
      `C_INV3:     avs_s0_readdata <= invert3;
      `C_INV4:     avs_s0_readdata <= invert4;
      `C_MASKo:    avs_s0_readdata <= masko;
      `C_MASK1:    avs_s0_readdata <= mask1;
      `C_MASK2:    avs_s0_readdata <= mask2;
      `C_MASK3:    avs_s0_readdata <= mask3;
      `C_MASK4:    avs_s0_readdata <= mask4;
      `C_VALUEo:   avs_s0_readdata <= (rbo ? o : valueo);
      `C_VALUE1:   avs_s0_readdata <= (rb1 ? in1 : value1);
      `C_VALUE2:   avs_s0_readdata <= (rb2 ? in2 : value2);
      `C_VALUE3:   avs_s0_readdata <= (rb3 ? in3 : value3);
      `C_VALUE4:   avs_s0_readdata <= (rb4 ? in4 : value4);
    endcase
  end
end

// Apply inversion on selected bits
// Note: If forcing enable, the value is forced before inversion and masking.
logic [WIDTH-1:0] x1, x2, x3, x4;

always_ff @(posedge clk) begin
  x1 <= (force1 ? value1 : in1) ^ invert1;
  x2 <= (force2 ? value2 : in2) ^ invert2;
  x3 <= (force3 ? value3 : in3) ^ invert3;
  x4 <= (force4 ? value4 : in4) ^ invert4;
end

// Apply filter masks
logic [WIDTH-1:0] y1, y2, y3, y4;
assign y1 = x1 & mask1;
assign y2 = x2 & mask2;
assign y3 = x3 & mask3;
assign y4 = x4 & mask4;

function automatic logic [31:0] majority4 (
 input logic [31:0] a,
 input logic [31:0] b,
 input logic [31:0] c,
 input logic [31:0] d
);
// At each bit position: majority = 1 if at least 3 inputs are 1
majority4 = (a & b & c) |
            (a & b & d) |
            (a & c & d) |
            (b & c & d);
endfunction

logic [WIDTH-1:0] z;
always_comb begin
  unique case (mode)
    SEL1:    z = y1;
    SEL2:    z = y2;
    SEL3:    z = y3;
    SEL4:    z = y4;
    AND:     z = y1 & y2 & y3 & y4;
    OR:      z = y1 | y2 | y3 | y4;
    XOR:     z = y1 ^ y2 ^ y3 ^ y4;
    XNOR:    z = y1 ~^ y2 ~^ y3 ~^ y4;
//    MAJ:     z = majority4(y1, y2, y3, y4);
//    BLOCK8:  z = { y4[7:0], y3[7:0], y2[7:0], y1[7:0] };
//    BLOCK16: z = { y2[15:0], y1[15:0] };
//    SUM12:   z = y1+y2;
//    SUM1234: z = y1+y2+y3+y4;
//    DIFF12:  z = y1-y2;
    default: z = '0;
  endcase
end

// Note: if forcing, the value replaces the combiner output (i.e., it is not affected by inversion and masking).
always_ff @(posedge clk) begin
  o <= (forceo ? valueo : (z ^ inverto) & masko);
end

endmodule
