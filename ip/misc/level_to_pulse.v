// Misc functions (level to pulse)
// Rok Zitko, June 2020

`default_nettype none // turn off implicit data types
module level_to_pulse(input wire clk, input wire reset, input wire i, output wire o);

reg r;
assign o = i & ~r;

always @(posedge clk or posedge reset)
 if (reset)
   r <= 1'b0;
 else
   r <= i;

endmodule


module level_to_pulse_delay1(input wire clk, input wire reset, input wire i, output wire o);

reg r, q1;
assign o = q1 & ~r;

always @(posedge clk or posedge reset)
 if (reset) begin
   q1 <= 1'b0;
   r <= 1'b0;
 end else begin
   q1 <= i;
   r <= q1;
 end

endmodule



module level_to_pulse_with_synchronizer2(input wire clk, input wire reset, input wire i, output wire o);

reg r, q1, q2;
assign o = q2 & ~r;

always @(posedge clk or posedge reset)
 if (reset) begin
   q1 <= 1'b0;
   q2 <= 1'b0;
   r  <= 1'b0;
 end else begin
   q1 <= i;
   q2 <= q1;
   r  <= q2;
 end
endmodule



module level_to_pulse_with_synchronizer3(input wire clk, input wire reset, input wire i, output wire o);

reg r, q1, q2, q3;
assign o = q3 & ~r;

always @(posedge clk or posedge reset)
 if (reset) begin
	q1 <= 1'b0;
	q2 <= 1'b0;
	q3 <= 1'b0;
   r  <= 1'b0;
 end else begin
   q1 <= i;
   q2 <= q1;
	q3 <= q2;
   r  <= q3;
  end
endmodule

`default_nettype wire // turn implicit nets on again to avoid side-effects
