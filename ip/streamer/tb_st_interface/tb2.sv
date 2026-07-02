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
  .strobe_enable
);

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

// Tests
initial begin
  #44  assert(strobe_enable == 0);
  #0.1 assert(strobe_enable == 1);
end

initial begin
  #243 assert(strobe_enable == 1);
  #0.1 assert(strobe_enable == 0);
end

integer fh;

initial begin
  #300 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_if_2.ucdb");
  $finish;
end

endmodule: tb_st_if_2
