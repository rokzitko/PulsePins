module delay_or4 (
 input  wire clk,
 input  wire in1,
 input  wire in2,
 input  wire in3,
 input  wire in4,
 output reg  dout
);

// first stage registers
reg r1, r2, r3, r4;

always @(posedge clk) begin
 // stage 1: register inputs
 r1 <= in1;
 r2 <= in2;
 r3 <= in3;
 r4 <= in4;

 // stage 2: OR together and register the result
 dout <= r1 | r2 | r3 | r4;
end

endmodule
