// Purpose: rapid INIT_VAL writes must not wedge asi_ready after idle commit.
//
// This covers the parity-cancel failure where multiple INIT_VAL writes collapse through the
// latest-value static config CDC while the streamer is non-idle. The final committed initial
// value must be accepted, the decoder reload must complete, and input backpressure must clear.

`timescale 1ns/1ps

`include "../config.vh"

`default_nettype none

module tb_st_if_8;

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

task push_word(input logic [31:0] control,
               input logic [31:0] counter,
               input logic [31:0] data);
  begin
    src_bfm.set_transaction_data({control, counter, data});
    src_bfm.set_transaction_sop(1);
    src_bfm.set_transaction_eop(1);
    src_bfm.push_transaction();
  end
endtask

task wait_for_asi_ready(input int max_cycles);
  integer i;
  logic matched;
  begin
    matched = 1'b0;
    i = 0;
    while ((i < max_cycles) && !matched) begin
      matched = asi_ready;
      if (!matched)
        @(posedge clk);
      i = i + 1;
    end
    assert(matched) else $fatal(1, "asi_ready did not recover after rapid INIT_VAL writes");
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

  trigger_enable_ext <= 1'b1;

  // One trigger condition: wait for trigger_in[0] == 1. The trigger stays armed below
  // because trigger_in is held at zero, so static_cfg_cdc cannot commit immediately.
  //                                  control   counter   data {mask,pattern}
  push_word(32'h03000000, 32'h00000000, 32'h01010000);

  wait(trigger_armed);
  repeat (8) @(posedge clk);
  assert(trigger_activated == 1'b0) else $fatal(1, "trigger unexpectedly activated");

  avmm_write(INIT_VAL, 32'h00000010);
  avmm_write(INIT_VAL, 32'h000000f0);

  // Keep dst_accept false long enough that both update requests reach the streamer side
  // while armed. Broken RTL collapses the embedded toggle back to its original parity.
  repeat (80) @(posedge clk);
  assert(trigger_armed == 1'b1) else $fatal(1, "trigger did not remain armed");

  trigger_reset_ext <= 1'b1;
  repeat (8) @(posedge streamer_clk);
  trigger_reset_ext <= 1'b0;
  wait(!trigger_armed);

  repeat (80) @(posedge clk);
  assert(qout == 32'h000000f0) else $fatal(1, "final INIT_VAL was not committed to idle qout");
  wait_for_asi_ready(80);

  //                                  control   counter   data
  // BITSET 0x0f for four samples. If the decoder base was not reloaded, qout would be 0x0f.
  push_word(32'h10000000, 32'h04000000, 32'h0f000000);

  push_word(32'h04000000, 32'h01000000, 32'hff000000);

  repeat (8) @(posedge streamer_clk);
  trigger_force_ext <= 1'b1;
end

always @(posedge streamer_clk) begin
  if (qout_valid) begin
    assert(qout == 32'h000000ff) else $fatal(1, "INIT_VAL reload was not used as decoder base");
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
  $set_coverage_db_name("run_st_if_8.ucdb");
`endif
  $finish;
end

initial begin
  #2000 $fatal(1, "timeout");
end

endmodule: tb_st_if_8

`default_nettype wire
