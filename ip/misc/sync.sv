module sync_bit (
    input  wire clk_dest,     // destination clock
    input  wire async_in,     // signal from source domain
    output wire sync_out      // synchronized signal
);
    reg sync_ff1, sync_ff2;

    always @(posedge clk_dest) begin
        sync_ff1 <= async_in;
        sync_ff2 <= sync_ff1;
    end

    assign sync_out = sync_ff2;
endmodule
