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
  repeat (4) @(negedge src_clk);
  src_reset = 0;
  repeat (4) @(negedge dst_clk);
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
    @(negedge src_clk);
    src_data = value;
    src_update = 1'b1;
    @(negedge src_clk);
    src_update = 1'b0;
  end
endtask

integer fh;
int committed_updates = 0, held_pending_cycles = 0, committed_snapshots = 0;
always @(posedge dst_clk) begin
  #1ps;
  if (!dst_reset) begin
    if (dst_valid) committed_updates++;
    if (!dst_accept && dst_pending) held_pending_cycles++;
    if (snap_valid) committed_snapshots++;
  end
end

initial begin
  src_data = 8'h00;
  src_update = 1'b0;
  dst_accept = 1'b1;

  wait(!src_reset && !dst_reset);

  send_update(8'h11);
  wait(dst_valid && dst_data == 8'h11);

  @(negedge dst_clk);
  dst_accept = 1'b0;
  send_update(8'h22);
  send_update(8'h33);
  repeat (20) @(negedge dst_clk);
  assert(dst_data == 8'h11) else $fatal(1, "dst_data changed while accept was low");
  assert(dst_pending) else $fatal(1, "dst_pending was not set while accept was low");

  dst_accept = 1'b1;
  wait(dst_data == 8'h33);

  wait(snap_valid);
  repeat (20) @(negedge dst_clk);
  assert(snap_dst != 8'h00) else $fatal(1, "snapshot did not update");
  tb_require_scenario("committed_updates", committed_updates, 2);
  tb_require_scenario("blocked_pending_update", held_pending_cycles);
  tb_require_scenario("snapshot_commits", committed_snapshots);
  tb_require_scenario("latest_value_delivered", dst_data === 8'h33);

  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $finish;
end

initial begin
  #10000 $fatal(1, "timeout");
end

`include "../../testbench/scenario_coverage.svh"
endmodule

`default_nettype wire
