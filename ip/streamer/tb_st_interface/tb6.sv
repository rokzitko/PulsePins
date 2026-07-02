// Purpose: split-clock CDC regression for `st_interface` static configuration updates.
//
// Writes qout override configuration while playback is active. The override must not affect
// qout_valid samples from the active run; it may commit only after the streamer returns idle.

`timescale 1ns/1ps

`include "../config.vh"

`default_nettype none

module tb_st_if_6;

logic clk = 1'b1;
logic streamer_clk = 1'b1;
always #0.5 clk = ~clk;
always #0.7 streamer_clk = ~streamer_clk;

logic reset;
initial begin
  reset <= 1;
  #5 reset <= 0;
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

task avmm_read(input logic [4:0] addr);
  begin
    @(negedge clk);
    avs_s0_address = addr;
    avs_s0_read = 1'b1;
    @(posedge clk);
    @(negedge clk);
    avs_s0_read = 1'b0;
  end
endtask

task automatic wait_qout_readback(input logic [31:0] expected);
  integer i;
  bit matched;
  begin
    matched = 1'b0;
    i = 0;
    while ((i < 40) && !matched) begin
      avmm_read(QOUT);
      matched = (avs_s0_readdata == expected);
      if (!matched)
        @(posedge clk);
      i = i + 1;
    end
    assert(matched) else $fatal(1, "QOUT readback did not snapshot override");
  end
endtask

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

  wait(!reset);
  src_bfm.init();

  //                                  control   counter   data
  src_bfm.set_transaction_data(96'h00000000_14000000_11000000);
  src_bfm.set_transaction_sop(1);
  src_bfm.set_transaction_eop(1);
  src_bfm.push_transaction();

  src_bfm.set_transaction_data(96'h00000000_14000000_22000000);
  src_bfm.set_transaction_sop(1);
  src_bfm.set_transaction_eop(1);
  src_bfm.push_transaction();

  src_bfm.set_transaction_data(96'h04000000_01000000_33000000);
  src_bfm.set_transaction_sop(1);
  src_bfm.set_transaction_eop(1);
  src_bfm.push_transaction();

  repeat (8) @(posedge streamer_clk);
  trigger_force_ext <= 1'b1;

  wait(qout_valid);
  avmm_write(QOUT_OVERRIDE, 32'ha5a5_a5a5);
  avmm_write(IF_CTRL, 32'h20); // request qout_select while playback is active
end

integer sample_idx = 0;

always @(posedge streamer_clk) begin
  if (qout_valid) begin
    assert(qout != 32'ha5a5_a5a5) else $fatal(1, "override affected active playback");
    if (sample_idx < 20)
      assert(qout == 32'h00000011) else $fatal(1, "first payload mismatch");
    else if (sample_idx < 40)
      assert(qout == 32'h00000022) else $fatal(1, "second payload mismatch");
    else
      $fatal(1, "too many qout_valid samples");
    sample_idx <= sample_idx + 1;
  end
end

integer fh;

initial begin
  wait(done);
  repeat (20) @(posedge streamer_clk);
  assert(sample_idx == 40) else $fatal(1, "sample count mismatch");
  assert(buffer_error == 0) else $fatal(1, "unexpected buffer_error");
  assert(qout == 32'ha5a5_a5a5) else $fatal(1, "pending override did not commit when idle");

  repeat (20) @(posedge clk);
  wait_qout_readback(32'ha5a5_a5a5);

  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
`ifndef VERILATOR
  $set_coverage_db_name("run_st_if_6.ucdb");
`endif
  $finish;
end

initial begin
  #1000 $fatal(1, "timeout");
end

endmodule: tb_st_if_6

`default_nettype wire
