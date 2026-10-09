// Rok Zitko, 2025

`default_nettype none // turn off implicit data types

module tik #(
  PERIOD = 10,
  WIDTH = (PERIOD > 1) ? $clog2(PERIOD) : 1
)(
  input wire clk,
  input wire reset,
  output wire tik
);

reg [WIDTH-1:0] counter;

initial assert (PERIOD >= 1 && WIDTH >= ((PERIOD > 1) ? $clog2(PERIOD) : 1))
  else $fatal(1, "tik requires a positive period and sufficient counter width");

assign tik = (counter == PERIOD-1);

always @(posedge clk)
begin
  if (reset) begin
    counter <= '0;
  end else if (tik) begin
    counter <= '0;
  end else begin
    counter <= WIDTH'(counter + 1);
  end
end

endmodule: tik

`default_nettype wire
