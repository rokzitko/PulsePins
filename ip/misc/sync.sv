`default_nettype none

module sync_bit_2stage (
    input  wire clk_dest,     // destination clock
    input  wire async_in,     // signal from source domain
    output wire sync_out      // synchronized signal
);

(*
altera_attribute = {
"-name SYNCHRONIZER_IDENTIFICATION \"FORCED IF ASYNCHRONOUS\"; ",
"-name SYNCHRONIZATION_REGISTER_CHAIN_LENGTH 2; ",
"-name DONT_MERGE_REGISTER ON; ",
"-name PRESERVE_REGISTER ON" }
*) reg sync_ff1, sync_ff2;

    always @(posedge clk_dest) begin
        sync_ff1 <= async_in;
        sync_ff2 <= sync_ff1;
    end

    assign sync_out = sync_ff2;
endmodule

module sync_bit_3stage (
    input  wire clk_dest,     // destination clock
    input  wire async_in,     // signal from source domain
    output wire sync_out      // synchronized signal
);

(*
altera_attribute = {
"-name SYNCHRONIZER_IDENTIFICATION \"FORCED IF ASYNCHRONOUS\"; ",
"-name SYNCHRONIZATION_REGISTER_CHAIN_LENGTH 3; ",
"-name DONT_MERGE_REGISTER ON; ",
"-name PRESERVE_REGISTER ON" }
*) reg sync_ff1, sync_ff2, sync_ff3;

    always @(posedge clk_dest) begin
        sync_ff1 <= async_in;
        sync_ff2 <= sync_ff1;
        sync_ff3 <= sync_ff2;
    end

    assign sync_out = sync_ff3;
endmodule

`default_nettype wire
