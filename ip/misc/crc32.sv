// Streaming CRC-32 (Ethernet/IEEE 802.3), 32-bit input, 1 word/clk.
// No tables/ROM/BRAM: pure XOR linear transform (64->32).
//
// Conventions:
// - Reflected CRC32 (LSB-first), poly = 0xEDB88320.
// - init = 0xFFFFFFFF, xorout = 0xFFFFFFFF.
// - First byte in data_in[7:0].
//
// Behavior:
// - When data_en=1, consumes data_in and updates running CRC.
// - crc_valid is asserted one cycle after data_en (crc_out registered).
// - crc_out is the "final" CRC (xorout applied) corresponding to that input word.

module crc32 (
  input  logic        clk,
  input  logic        reset,
  input  logic        data_en,    // qualify data_in
  input  logic [31:0] data_in,

  output logic [31:0] crc_out,    // crc (xorout applied)
  output logic        crc_valid
);

  localparam logic [31:0] CRC_INIT   = 32'hFFFF_FFFF;
  localparam logic [31:0] CRC_XOROUT = 32'hFFFF_FFFF;

  // Precomputed linear masks for CRC32/IEEE (reflected), advancing by 32 data bits.
  // Input vector v = {data_in[31:0], crc_state[31:0]} with v[0]=crc_state[0].
  localparam logic [63:0] MASK [0:31] = '{
    64'h04d101df04d101df,
    64'h09a203be09a203be,
    64'h1344077d1344077d,
    64'h26880efa26880efa,
    64'h4d101df44d101df4,
    64'h9a203be99a203be9,
    64'h3091760d3091760d,
    64'h6122ec1a6122ec1a,
    64'hc245d835c245d835,
    64'h805ab1b5805ab1b5,
    64'h046462b5046462b5,
    64'h08c8c56a08c8c56a,
    64'h11918ad411918ad4,
    64'h232315a9232315a9,
    64'h46462b5346462b53,
    64'h8c8c56a68c8c56a6,
    64'h1dc9ac921dc9ac92,
    64'h3b9359243b935924,
    64'h7726b2497726b249,
    64'hee4d6493ee4d6493,
    64'hd84bc8f9d84bc8f9,
    64'hb446902db446902d,
    64'h6c5c21846c5c2184,
    64'hd8b84309d8b84309,
    64'hb5a187ccb5a187cc,
    64'h6f920e466f920e46,
    64'hdf241c8cdf241c8c,
    64'hba9938c7ba9938c7,
    64'h71e3705171e37051,
    64'he3c6e0a3e3c6e0a3,
    64'hc35cc098c35cc098,
    64'h826880ef826880ef
  };

  logic [31:0] crc_state;
  logic [31:0] crc_next;
  logic [63:0] v;

  // Combinational next-state computation (pure XOR)
  integer k;
  always_comb begin
    v = {data_in, crc_state};
    crc_next = 32'h0;
    for (k = 0; k < 32; k++) begin
      crc_next[k] = ^(v & MASK[k]);   // reduction XOR (parity)
    end
  end

  // State update + registered output
  always_ff @(posedge clk or posedge reset) begin
    if (reset) begin
      crc_state <= CRC_INIT;
      crc_out   <= 32'h0;
      crc_valid <= 1'b0;
    end else begin
      crc_valid <= data_en;
      if (data_en) begin
        crc_state <= crc_next;
        crc_out   <= crc_next ^ CRC_XOROUT; // "final" CRC
      end
    end
  end

endmodule
