// Rok Zitko, 2025

`default_nettype none // turn off implicit data types

module tik #(
  PERIOD = 10,
  WIDTH = $clog2(PERIOD)
)(
  input wire clk,
  input wire reset,
  output wire tik
);

reg [WIDTH-1:0] counter;

assign tik = (counter == PERIOD-1);

always @(posedge clk)
begin
  if (reset) begin
    counter <= '0;
  end else if (tik) begin
    counter <= '0;
  end else begin
    counter <= counter + 1;
  end
end

endmodule: tik

`default_nettype wire
