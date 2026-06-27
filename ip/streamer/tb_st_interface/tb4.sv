// Purpose: Avalon-MM gating configuration test for `st_interface`.
//
// Programs `GATING_W` to use the external `gate_in` signal as the gate source and verifies that
// the software-visible wrapper blocks playback while the gate is low, reports the correct live
// gating state through `GATING_R`, and resumes playback once the gate opens.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_if_4;

logic clk;
logic reset;

// Simple 1 ns clock and short reset pulse used only to establish deterministic initial state.
initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

// DUT ports
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

wire streamer_clk = clk;

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

// Lightweight Avalon-ST source used to feed a short deterministic element stream.
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

// Trace the software-visible gating state and the resulting output activity.
always @(posedge clk) begin
  $strobe("t=%8.3f gate_in=%b gating=%b gate_in_en=%b gate_signal=%b gate_enable=%b qout=%h valid=%b st_en=%b done=%b act=%b used=%0d",
    $realtime,
    gate_in,
    dut.gating,
    dut.gate_in_en,
    dut.gate_signal,
    dut.gate_enable,
    qout,
    qout_valid,
    strobe_enable,
    done,
    trigger_activated,
    dut.st0.fifo0.used
  );
end

// Helper modeling a single Avalon-MM register write.
task avmm_write(input logic [4:0] addr, input logic [31:0] data);
  @(negedge clk);
  avs_s0_address = addr;
  avs_s0_writedata = data;
  avs_s0_write = 1'b1;
  @(negedge clk);
  avs_s0_write = 1'b0;
endtask

// Helper modeling a single Avalon-MM register read.
task avmm_read(input logic [4:0] addr);
  @(negedge clk);
  avs_s0_address = addr;
  avs_s0_read = 1'b1;
  @(posedge clk);
  @(negedge clk);
  avs_s0_read = 1'b0;
endtask

task automatic wait_gating_readback(input logic [31:0] expected, input logic [31:0] mask);
  integer i;
  bit matched;
  begin
    matched = 1'b0;
    i = 0;
    while ((i < 40) && !matched) begin
      avmm_read(GATING_R);
      matched = ((avs_s0_readdata & mask) == expected);
      if (!matched)
        @(posedge clk);
      i = i + 1;
    end
    assert(matched) else $fatal(1, "GATING_R did not reach expected value");
  end
endtask

// Program gate_in-based gating and feed a short deterministic stream.
initial begin
  avs_s0_address <= '0;
  avs_s0_read <= 0;
  avs_s0_write <= 0;
  avs_s0_writedata <= '0;
  trigger_in <= '0;
  trigger_enable_ext <= 0;
  trigger_force_ext <= 0;
  trigger_reset_ext <= 0;
  gate_in <= 0;

  #5;
  src_bfm.init();

  // Enable gating and select gate_in as the gating source.
  avmm_write(GATING_W, 32'h00000003);

  // Load a short deterministic stream through the software-visible transport path.
  //                                  control   counter   data
  src_bfm.set_transaction_data(96'h00000000_03000000_11000000);
  src_bfm.set_transaction_sop(1);
  src_bfm.set_transaction_eop(1);
  src_bfm.push_transaction();

  src_bfm.set_transaction_data(96'h00000000_02000000_22000000);
  src_bfm.set_transaction_sop(1);
  src_bfm.set_transaction_eop(1);
  src_bfm.push_transaction();

  src_bfm.set_transaction_data(96'h04000000_01000000_33000000);
  src_bfm.set_transaction_sop(1);
  src_bfm.set_transaction_eop(1);
  src_bfm.push_transaction();

  // Force triggering while the gate is still low.
  #5;
  trigger_force_ext <= 1;

  // Open the gate later so playback can resume.
  #20;
  gate_in <= 1;
end

integer sample_idx = 0;
logic gate_closed_checked = 1'b0;
logic gate_open_checked = 1'b0;
logic done_checked = 1'b0;

// Check that no payload advances while gate_in keeps gate_enable low.
initial begin
  wait(dut.gating_streamer == 1);
  wait_gating_readback(32'h00000003, 32'h00000003);

  #18;
  assert(trigger_activated == 1) else $fatal;
  assert(dut.gate_enable == 0) else $fatal;
  assert(qout_valid == 0) else $fatal;
  assert(strobe_enable == 0) else $fatal;
  assert(done == 0) else $fatal;
  gate_closed_checked = 1'b1;
end

// Once the gate opens, wrapper-level status and output progression should change together.
initial begin
  wait(gate_in == 1);
  wait(dut.gate_enable == 1);
  assert(dut.gate_enable == 1) else $fatal;
  wait_gating_readback(32'h00001c00, 32'h00001c00);
  gate_open_checked = 1'b1;
end

always @(posedge clk) begin
  if (qout_valid) begin
    case (sample_idx)
      0, 1, 2: assert(qout == 32'h00000011) else $fatal;
      3, 4:    assert(qout == 32'h00000022) else $fatal;
      default: $fatal;
    endcase
    sample_idx = sample_idx + 1;
  end
end

initial begin
  wait(done == 1);
  #1;
  assert(sample_idx == 5) else $fatal;
  assert(qout == 32'h00000033) else $fatal;
  assert(buffer_error == 0) else $fatal;
  done_checked = 1'b1;
end

integer fh;

initial begin
  wait(gate_closed_checked && gate_open_checked && done_checked);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
`ifndef VERILATOR
  $set_coverage_db_name("run_st_if_4.ucdb");
`endif
  $finish;
end

initial begin
  #300 $fatal(1, "timeout");
end

endmodule: tb_st_if_4
