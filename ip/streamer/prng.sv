// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// https://prng.di.unimi.it/: A PRNG Shootout

`default_nettype none

module prng64_xorshift (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        en,          // advance when 1
    input  wire        reseed,      // load seed when 1
    input  wire [63:0] seed,        // must be non-zero
    output reg [63:0] rnd
);
    logic [63:0] x;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            x <= 64'h1;  // safe non-zero default
        end else if (reseed) begin
            x <= (seed == 64'h0) ? 64'h1 : seed;
        end else if (en) begin
            x <= x ^ (x << 13);
            x <= x ^ (x >> 7);
            x <= x ^ (x << 17);
        end
    end

    assign rnd = x;
endmodule

module prng_xoroshiro128plus (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        en,          // advance when 1
    input  wire        reseed,      // load new seed when 1
    input  wire [127:0] seed,       // must be non-zero
    output wire [63:0] rnd
);
    // Internal state: two 64-bit registers
    logic [63:0] s0, s1;

    // rotate-left function
    function automatic logic [63:0] rotl64 (
        input logic [63:0] x,
        input int unsigned k
    );
        rotl64 = (x << k) | (x >> (64 - k));
    endfunction

    assign rnd = s0 + s1;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            s0 <= 64'h0123456789ABCDEF;
            s1 <= 64'hFEDCBA9876543210;
        end else if (reseed) begin
            // split 128-bit seed into two 64-bit words
            s0 <= (seed[63:0]  == 64'h0) ? 64'h1 : seed[63:0];
            s1 <= (seed[127:64] == 64'h0) ? 64'h2 : seed[127:64];
        end else if (en) begin
            logic [63:0] t0, t1;
            t0 = s0;
            t1 = s1;

            t1 ^= t0;
            s0 <= rotl64(t0, 55) ^ t1 ^ (t1 << 14);
            s1 <= rotl64(t1, 36);
        end
    end

endmodule
