// Purpose: small end-to-end wrapper test for `st_interface`.
//
// Drives a short two-element transaction sequence through the wrapper to validate that the
// software-visible transport path and the underlying streamer integration work together.
// Two-element sequence full simulation
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_if_2;

reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3;
  reset <= 0;
end

// DUT ports
wire [95:0] asi_data;
wire        asi_valid;
wire        asi_ready;
reg         trigger_force_ext;
wire        trigger_armed;
wire        trigger_activated;
wire [31:0] qout;
wire        qout_valid;
wire        streamer_clk;
wire        strobe_enable;

assign streamer_clk = clk;

st_interface dut (
  .clk,
  .reset,
  .asi_data,
  .asi_valid,
  .asi_channel(1'b0),
  .asi_ready,
  .trigger_force_ext,
  .trigger_armed,
  .trigger_activated,
  .qout,
  .qout_valid,
  .streamer_clk,
  .strobe_enable,
  .avs_s0_address('0), .avs_s0_read(1'b0), .avs_s0_write(1'b0), .avs_s0_writedata('0),
  .trigger_in('0), .trigger_enable_ext(1'b0), .trigger_reset_ext(1'b0),
  .gate_in(1'b0)
);

`include "check_inputs.svh"

// Instantiate Avalon-ST Source BFM
avalon_st_source_bfm #(
  .AVALON_ST_DATA_WIDTH(96)
) src_bfm (
  .clk,
  .reset,
  .src_data(asi_data),
  .src_valid(asi_valid),
  .src_ready(asi_ready)
);

always @(posedge clk) begin
  $strobe("t=%8.3f asi_valid=%b asi_ready=%b fifo wrreq=%b y=%h c=%h v=%h trigger force=%b armed=%b act=%b qout=%h qout_valid=%b used=%d st_en=%b",
    $realtime, asi_valid, asi_ready, dut.st0.fifo_i.wrreq, dut.st0.control,
    dut.st0.counter, dut.st0.data, trigger_force_ext, trigger_armed, trigger_activated,
    qout, qout_valid, dut.st0.fifo0.used, strobe_enable
  );
end

// Stimulus
initial begin
    #3;

    // Initialize the BFM
    src_bfm.init();

//                                   control   counter  data
    src_bfm.set_transaction_data(96'h00000000_05000000_12345678);
    src_bfm.set_transaction_sop(1);
    src_bfm.set_transaction_eop(1);
    src_bfm.push_transaction();

    src_bfm.set_transaction_data(96'h00000000_05000000_aabbccdd);
    src_bfm.set_transaction_sop(1);
    src_bfm.set_transaction_eop(1);
    src_bfm.push_transaction();
end

initial begin
  trigger_force_ext <= 0;
  #15
  trigger_force_ext <= 1;
end

// The two programmed five-cycle runs produce ten consecutive strobe cycles.
bit checker_done = 0;
int high_checks = 0, low_checks = 0;
initial begin
  wait(trigger_activated === 1'b1);
  @(negedge clk);
  assert(strobe_enable === 1'b0) else $fatal(1, "strobe enabled too early");
  low_checks++;
  @(negedge clk);
  assert(strobe_enable === 1'b1) else $fatal(1, "strobe did not start");
  high_checks++;
  repeat (9) begin
    @(negedge clk);
    assert(strobe_enable === 1'b1) else $fatal(1, "strobe ended too early");
    high_checks++;
  end
  repeat (4) begin
    @(negedge clk);
    assert(strobe_enable === 1'b0) else $fatal(1, "strobe did not stop");
    low_checks++;
  end
  checker_done = 1;
end

integer fh;

initial begin
  wait(checker_done);
  tb_require_scenario("enabled_strobe_cycles_checked", high_checks, 10);
  tb_require_scenario("disabled_strobe_cycles_checked", low_checks, 5);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_if_2.ucdb");
  $finish;
end

initial #300 $fatal(1, "timeout waiting for wrapper strobe checks");

`include "../../testbench/scenario_coverage.svh"
endmodule: tb_st_if_2
