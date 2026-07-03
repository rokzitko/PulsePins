// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Žitko

// Registered output combiner.
//
// This is the main late-stage output-routing block for combining multiple streamer outputs.
// It supports input/output forcing, inversion, masking, multiple combination modes, and
// readback of either stored override values or live ports.
//
// Important timing fact: both inputs and outputs are registered, so the combiner is part
// of the timed datapath rather than transparent glue logic.
// Architectural overview lives in `ip/combiner/README.md` and `docs/docs/combiner.md`.

module combiner #(
 parameter int WIDTH = 32
)(
 input  wire [3:0]       avs_s0_address,
 input  wire             avs_s0_read,
 output reg  [31:0]      avs_s0_readdata,
 input  wire             avs_s0_write,
 input  wire [31:0]      avs_s0_writedata,
 input  wire             clock_clk,         // clock in the control domain
 input  wire             reset_reset,
 input  wire             clk,               // clock in the signal domain
 input  wire             clk_reset,         // reset synchronized to clk
 input  wire [WIDTH-1:0] in1,
 input  wire [WIDTH-1:0] in2,
 input  wire [WIDTH-1:0] in3,
 input  wire [WIDTH-1:0] in4,
 output reg  [WIDTH-1:0] o
);

logic [31:0] cfg; // configuration register
logic cfg_update;

localparam WIDTH_MODE = 4;
logic [WIDTH_MODE-1:0] mode; // selected input in multiplexer mode or some combining operation

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

// true = use the value in value? instead of the value in the input port in?
logic force1, force2, force3, force4;

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

localparam int CONFIG_W = 32 + 15*WIDTH;
localparam logic [CONFIG_W-1:0] CONFIG_RESET = {
  {(5*WIDTH){1'b0}}, // values
  {(5*WIDTH){1'b1}}, // masks
  {(5*WIDTH){1'b0}}, // inversion patterns
  28'b0, SEL1        // cfg
};

logic [CONFIG_W-1:0] config_clock;
logic [CONFIG_W-1:0] config_clk;
logic [31:0] cfg_clk;
logic [WIDTH-1:0] inverto_clk, invert1_clk, invert2_clk, invert3_clk, invert4_clk;
logic [WIDTH-1:0] masko_clk, mask1_clk, mask2_clk, mask3_clk, mask4_clk;
logic [WIDTH-1:0] valueo_clk, value1_clk, value2_clk, value3_clk, value4_clk;

assign config_clock = {value4, value3, value2, value1, valueo,
                       mask4, mask3, mask2, mask1, masko,
                       invert4, invert3, invert2, invert1, inverto,
                       cfg};
assign {value4_clk, value3_clk, value2_clk, value1_clk, valueo_clk,
        mask4_clk, mask3_clk, mask2_clk, mask1_clk, masko_clk,
        invert4_clk, invert3_clk, invert2_clk, invert1_clk, inverto_clk,
        cfg_clk} = config_clk;

assign mode = cfg_clk[WIDTH_MODE-1:0];
assign forceo = cfg_clk[`B_FORCEo];
assign force1 = cfg_clk[`B_FORCE1];
assign force2 = cfg_clk[`B_FORCE2];
assign force3 = cfg_clk[`B_FORCE3];
assign force4 = cfg_clk[`B_FORCE4];

cdc_bus_update #(
  .WIDTH(CONFIG_W),
  .RESET_VALUE(CONFIG_RESET)
) config_cdc (
  .src_clk(clock_clk),
  .src_reset(reset_reset),
  .src_data(config_clock),
  .src_update(cfg_update),
  .src_busy(),
  .dst_clk(clk),
  .dst_reset(clk_reset),
  .dst_accept(1'b1),
  .dst_data(config_clk),
  .dst_valid(),
  .dst_pending()
);

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

// Upon reset, `in1` is passed to the output with no inversion, masking, or forcing.

always_ff @(posedge clock_clk) begin
  if (reset_reset) begin
    cfg <= SEL1;
    inverto <= 0;
    invert1 <= 0;
    invert2 <= 0;
    invert3 <= 0;
    invert4 <= 0;
    masko <= ~0; // 0xFFFFFFFF
    mask1 <= ~0;
    mask2 <= ~0;
    mask3 <= ~0;
    mask4 <= ~0;
    valueo <= 0;
    value1 <= 0;
    value2 <= 0;
    value3 <= 0;
    value4 <= 0;
    cfg_update <= 1'b0;
  end else begin
    cfg_update <= avs_s0_write;
    if (avs_s0_write) begin
      unique case (avs_s0_address)
        `C_CFG:  cfg      <= avs_s0_writedata;
        `C_INVo: inverto  <= avs_s0_writedata;
        `C_INV1: invert1  <= avs_s0_writedata;
        `C_INV2: invert2  <= avs_s0_writedata;
        `C_INV3: invert3  <= avs_s0_writedata;
        `C_INV4: invert4  <= avs_s0_writedata;
        `C_MASKo: masko   <= avs_s0_writedata;
        `C_MASK1: mask1   <= avs_s0_writedata;
        `C_MASK2: mask2   <= avs_s0_writedata;
        `C_MASK3: mask3   <= avs_s0_writedata;
        `C_MASK4: mask4   <= avs_s0_writedata;
        `C_VALUEo: valueo <= avs_s0_writedata;
        `C_VALUE1: value1 <= avs_s0_writedata;
        `C_VALUE2: value2 <= avs_s0_writedata;
        `C_VALUE3: value3 <= avs_s0_writedata;
        `C_VALUE4: value4 <= avs_s0_writedata;
      endcase
    end
  end
end

logic [WIDTH-1:0] o_clock, in1_clock, in2_clock, in3_clock, in4_clock;

cdc_snapshot #(.WIDTH(WIDTH)) o_snapshot (
  .src_clk(clk), .src_reset(clk_reset), .src_data(o),
  .dst_clk(clock_clk), .dst_reset(reset_reset), .dst_data(o_clock), .dst_valid()
);

cdc_snapshot #(.WIDTH(WIDTH)) in1_snapshot (
  .src_clk(clk), .src_reset(clk_reset), .src_data(in1),
  .dst_clk(clock_clk), .dst_reset(reset_reset), .dst_data(in1_clock), .dst_valid()
);

cdc_snapshot #(.WIDTH(WIDTH)) in2_snapshot (
  .src_clk(clk), .src_reset(clk_reset), .src_data(in2),
  .dst_clk(clock_clk), .dst_reset(reset_reset), .dst_data(in2_clock), .dst_valid()
);

cdc_snapshot #(.WIDTH(WIDTH)) in3_snapshot (
  .src_clk(clk), .src_reset(clk_reset), .src_data(in3),
  .dst_clk(clock_clk), .dst_reset(reset_reset), .dst_data(in3_clock), .dst_valid()
);

cdc_snapshot #(.WIDTH(WIDTH)) in4_snapshot (
  .src_clk(clk), .src_reset(clk_reset), .src_data(in4),
  .dst_clk(clock_clk), .dst_reset(reset_reset), .dst_data(in4_clock), .dst_valid()
);

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
      `C_VALUEo:   avs_s0_readdata <= (rbo ? o_clock : valueo);
      `C_VALUE1:   avs_s0_readdata <= (rb1 ? in1_clock : value1);
      `C_VALUE2:   avs_s0_readdata <= (rb2 ? in2_clock : value2);
      `C_VALUE3:   avs_s0_readdata <= (rb3 ? in3_clock : value3);
      `C_VALUE4:   avs_s0_readdata <= (rb4 ? in4_clock : value4);
    endcase
  end
end

// Apply input forcing first, then inversion.
logic [WIDTH-1:0] x1, x2, x3, x4;

always_ff @(posedge clk) begin
  if (clk_reset) begin
    x1 <= '0;
    x2 <= '0;
    x3 <= '0;
    x4 <= '0;
  end else begin
    x1 <= (force1 ? value1_clk : in1) ^ invert1_clk;
    x2 <= (force2 ? value2_clk : in2) ^ invert2_clk;
    x3 <= (force3 ? value3_clk : in3) ^ invert3_clk;
    x4 <= (force4 ? value4_clk : in4) ^ invert4_clk;
  end
end

// Apply input masks after forcing and inversion.
logic [WIDTH-1:0] y1, y2, y3, y4;
assign y1 = x1 & mask1_clk;
assign y2 = x2 & mask2_clk;
assign y3 = x3 & mask3_clk;
assign y4 = x4 & mask4_clk;

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
    MAJ:     z = majority4(y1, y2, y3, y4);
    BLOCK8:  z = { y4[7:0], y3[7:0], y2[7:0], y1[7:0] };
    BLOCK16: z = { y2[15:0], y1[15:0] };
    SUM12:   z = y1+y2;
    SUM1234: z = y1+y2+y3+y4;
    DIFF12:  z = y1-y2;
    default: z = 32'b0;
  endcase
end

// Output forcing bypasses the normal output inversion/masking path.
always_ff @(posedge clk) begin
  if (clk_reset) begin
    o <= '0;
  end else begin
    o <= (forceo ? valueo_clk : (z ^ inverto_clk) & masko_clk);
  end
end

endmodule
