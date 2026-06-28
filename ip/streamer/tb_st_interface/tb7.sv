// Purpose: INIT_VAL commit must reload the decoder base state without requiring reset.

`timescale 1ns/1ps

`include "../config.vh"

`default_nettype none

module tb_st_if_7;

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

integer samples;
integer fh;

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
  samples <= 0;

  wait(!reset);
  src_bfm.init();

  avmm_write(INIT_VAL, 32'h000000f0);
  repeat (80) @(posedge clk);
  assert(qout == 32'h000000f0) else $fatal(1, "committed idle qout did not update");

  //                                  control   counter   data
  // BITSET 0x0f for four samples. If the decoder base was not reloaded, qout would be 0x0f.
  src_bfm.set_transaction_data(96'h10000000_04000000_0f000000);
  src_bfm.set_transaction_sop(1);
  src_bfm.set_transaction_eop(1);
  src_bfm.push_transaction();

  src_bfm.set_transaction_data(96'h04000000_01000000_ff000000);
  src_bfm.set_transaction_sop(1);
  src_bfm.set_transaction_eop(1);
  src_bfm.push_transaction();

  repeat (8) @(posedge streamer_clk);
  trigger_force_ext <= 1'b1;
end

always @(posedge streamer_clk) begin
  if (qout_valid) begin
    assert(qout == 32'h000000ff) else $fatal(1, "INIT_VAL was not used as decoder base");
    samples <= samples + 1;
  end
end

initial begin
  wait(done);
  assert(samples >= 4) else $fatal(1, "too few output samples");
  assert(buffer_error == 0) else $fatal(1, "unexpected buffer_error");
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
`ifndef VERILATOR
  $set_coverage_db_name("run_st_if_7.ucdb");
`endif
  $finish;
end

initial begin
  #1000 $fatal(1, "timeout");
end

endmodule: tb_st_if_7

`default_nettype wire
