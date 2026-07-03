// Purpose: `stop` must not make an unfinished stream eligible for static config commits.
//
// A pending qout override/select update is written while playback is active, then `stop` is
// asserted. The update must remain pending while the stream is stopped but not drained, and may
// commit only after `stop` is released and the terminator is consumed.

`timescale 1ns/1ps

`include "../config.vh"

`default_nettype none

module tb_st_if_9;

logic clk = 1'b1;
logic streamer_clk = 1'b1;
always #0.5 clk = ~clk;
always #0.7 streamer_clk = ~streamer_clk;

logic reset;
initial begin
  reset <= 1'b1;
  #5 reset <= 1'b0;
end

wire [95:0] asi_data;
wire        asi_valid;
wire        asi_ready;
wire        src_sop;
wire        src_eop;

logic [4:0] avs_s0_address;
logic       avs_s0_read;
logic       avs_s0_write;
wire [31:0] avs_s0_readdata;
logic [31:0] avs_s0_writedata;

logic [WIDTH_TRIGGER-1:0] trigger_in;
logic trigger_enable_ext;
logic trigger_force_ext;
logic trigger_reset_ext;
logic gate_in;

wire trigger_armed;
wire trigger_activated;
wire [31:0] qout;
wire qout_valid;
wire qout_strobe;
wire strobe_enable;
wire done;
wire buffer_error;

st_interface dut (
  .clk,
  .reset,
  .asi_data,
  .asi_valid,
  .asi_channel(1'b0),
  .asi_ready,
  .avs_s0_address,
  .avs_s0_read,
  .avs_s0_write,
  .avs_s0_readdata,
  .avs_s0_writedata,
  .streamer_clk,
  .qout,
  .qout_valid,
  .qout_strobe,
  .strobe_enable,
  .done,
  .buffer_error,
  .trigger_in,
  .trigger_enable_ext,
  .trigger_force_ext,
  .trigger_reset_ext,
  .trigger_armed,
  .trigger_activated,
  .gate_in
);

avalon_st_source_bfm #(
  .AVALON_ST_DATA_WIDTH(96)
) src_bfm (
  .clk,
  .reset,
  .src_data(asi_data),
  .src_valid(asi_valid),
  .src_ready(asi_ready),
  .src_sop,
  .src_eop
);

task avmm_write(input logic [4:0] addr, input logic [31:0] data);
  begin
    @(negedge clk);
    avs_s0_address = addr;
    avs_s0_writedata = data;
    avs_s0_write = 1'b1;
    @(negedge clk);
    avs_s0_write = 1'b0;
  end
endtask

task push_word(input logic [31:0] control, input logic [31:0] counter, input logic [31:0] data);
  begin
    src_bfm.set_transaction_data({control, counter, data});
    src_bfm.set_transaction_sop(1);
    src_bfm.set_transaction_eop(1);
    src_bfm.push_transaction();
  end
endtask

integer sample_idx;
integer fh;

always @(posedge streamer_clk) begin
  if (qout_valid) begin
    assert(qout != 32'ha5a5_a5a5) else $fatal(1, "override affected stopped/active playback");
    sample_idx <= sample_idx + 1;
  end
end

initial begin
  avs_s0_address <= '0;
  avs_s0_read <= 1'b0;
  avs_s0_write <= 1'b0;
  avs_s0_writedata <= '0;
  trigger_in <= '0;
  trigger_enable_ext <= 1'b0;
  trigger_force_ext <= 1'b0;
  trigger_reset_ext <= 1'b0;
  gate_in <= 1'b0;
  sample_idx <= 0;

  wait(!reset);
  src_bfm.init();

  //                                  control   counter   data
  push_word(32'h00000000, 32'h80000000, 32'h11000000);
  push_word(32'h00000000, 32'h10000000, 32'h22000000);
  push_word(32'h04000000, 32'h01000000, 32'h33000000);

  repeat (8) @(posedge streamer_clk);
  trigger_force_ext <= 1'b1;

  wait(qout_valid);
  avmm_write(QOUT_OVERRIDE, 32'ha5a5_a5a5);
  avmm_write(IF_CTRL, 32'h00000021); // request qout_select and stop while playback is unfinished

  wait(dut.stop_streamer == 1'b1);
  repeat (80) @(posedge streamer_clk);
  assert(done == 1'b0) else $fatal(1, "stream completed while stop was asserted");
  assert(buffer_error == 1'b0) else $fatal(1, "unexpected buffer_error while stopped");
  assert(dut.streamer_idle == 1'b0) else $fatal(1, "stopped unfinished stream looked idle");
  assert(qout != 32'ha5a5_a5a5) else $fatal(1, "pending override committed while stream was stopped");

  avmm_write(IF_CTRL, 32'h00000020); // release stop, keep qout_select pending
  wait(dut.stop_streamer == 1'b0);
  wait(done);
  assert(buffer_error == 1'b0) else $fatal(1, "unexpected buffer_error after stop release");
  repeat (80) @(posedge streamer_clk);
  assert(qout == 32'ha5a5_a5a5) else $fatal(1, "pending override did not commit after drain");
  assert(sample_idx == 144) else $fatal(1, "sample count mismatch");

  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
`ifndef VERILATOR
  $set_coverage_db_name("run_st_if_9.ucdb");
`endif
  $finish;
end

initial begin
  #2000 $fatal(1, "timeout");
end

endmodule: tb_st_if_9

`default_nettype wire
