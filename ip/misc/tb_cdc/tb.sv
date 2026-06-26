// Purpose: reusable CDC helper testbench.
//
// Exercises latest-value bus updates, destination-side accept gating, and continuous snapshots
// with unrelated source/destination clocks.

`timescale 1ns/1ps
`default_nettype none

module tb_cdc;

logic src_clk = 0;
logic dst_clk = 0;
always #0.7 src_clk = ~src_clk;
always #1.1 dst_clk = ~dst_clk;

logic src_reset;
logic dst_reset;

initial begin
  src_reset = 1;
  dst_reset = 1;
  repeat (4) @(posedge src_clk);
  src_reset = 0;
  repeat (4) @(posedge dst_clk);
  dst_reset = 0;
end

logic [7:0] src_data;
logic src_update;
logic dst_accept;
logic [7:0] dst_data;
logic dst_valid;
logic dst_pending;

cdc_bus_update #(.WIDTH(8)) update_dut (
  .src_clk,
  .src_reset,
  .src_data,
  .src_update,
  .src_busy(),
  .dst_clk,
  .dst_reset,
  .dst_accept,
  .dst_data,
  .dst_valid,
  .dst_pending
);

logic [7:0] snap_src;
logic [7:0] snap_dst;
logic snap_valid;

always_ff @(posedge src_clk) begin
  if (src_reset)
    snap_src <= 8'h00;
  else
    snap_src <= snap_src + 8'h01;
end

cdc_snapshot #(.WIDTH(8)) snapshot_dut (
  .src_clk,
  .src_reset,
  .src_data(snap_src),
  .dst_clk,
  .dst_reset,
  .dst_data(snap_dst),
  .dst_valid(snap_valid)
);

task automatic send_update(input logic [7:0] value);
  begin
    @(posedge src_clk);
    src_data = value;
    src_update = 1'b1;
    @(posedge src_clk);
    src_update = 1'b0;
  end
endtask

integer fh;

initial begin
  src_data = 8'h00;
  src_update = 1'b0;
  dst_accept = 1'b1;

  wait(!src_reset && !dst_reset);

  send_update(8'h11);
  wait(dst_valid && dst_data == 8'h11);

  dst_accept = 1'b0;
  send_update(8'h22);
  send_update(8'h33);
  repeat (20) @(posedge dst_clk);
  assert(dst_data == 8'h11) else $fatal(1, "dst_data changed while accept was low");
  assert(dst_pending) else $fatal(1, "dst_pending was not set while accept was low");

  dst_accept = 1'b1;
  wait(dst_data == 8'h33);

  wait(snap_valid);
  repeat (20) @(posedge dst_clk);
  assert(snap_dst != 8'h00) else $fatal(1, "snapshot did not update");

  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $finish;
end

initial begin
  #10000 $fatal(1, "timeout");
end

endmodule

`default_nettype wire
