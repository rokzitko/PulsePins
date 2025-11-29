`default_nettype none

module sig_mux #(
    parameter int unsigned INPUTS = 4
) (
    input  logic [INPUTS-1:0]              i,   // input vector
    input  logic [$clog2(INPUTS)-1:0]      sel, // selects one of INPUTS
    output logic                           o    // selected bit
);

    always_comb begin
        // Safe default
        o = 1'b0;

        // Only select if sel is within range (handles non-power-of-two INPUTS)
        if (sel < INPUTS)
            o = i[sel];
    end

endmodule

`default_nettype wire

