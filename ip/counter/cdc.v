// Two-stage CDC synchronizer (per-bit)
module cdc_twoff #(
    parameter integer WIDTH = 1
) (
    input  wire                   clk_dst,   // destination clock
    input  wire [WIDTH-1:0]       d_async,   // asynchronous / other-domain input
    output wire [WIDTH-1:0]       q_sync     // synchronized output in clk_dst domain
);

    reg [WIDTH-1:0] s1;
    reg [WIDTH-1:0] s2;

    always @(posedge clk_dst) begin
        s1 <= d_async;
        s2 <= s1;
    end

    assign q_sync = s2;

endmodule
