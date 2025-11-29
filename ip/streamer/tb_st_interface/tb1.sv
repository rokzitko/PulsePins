// Endianness test
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_if_1;

reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #1;
  reset <= 0;
end

// DUT ports
wire [95:0] asi_data;
wire        asi_valid;
wire        asi_ready;

st_interface dut (
  .clk,
  .reset,
  .asi_data,
  .asi_valid,
  .asi_ready
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
  $strobe("t=%8.3f asi_valid=%b asi_ready=%b fifo wrreq=%b empty_i=%b y=%h c=%h v=%h",
    $realtime, asi_valid, asi_ready, dut.st0.fifo_i.wrreq,
    dut.st0.empty_i, dut.st0.control, dut.st0.counter, dut.st0.data
  );
end

// Stimulus
initial begin
    #3;

    // Initialize the BFM
    src_bfm.init();

//                                  control   counter  data
    src_bfm.set_transaction_data(96'h12345678_aabbccdd_abcdefaa);
    src_bfm.set_transaction_sop(1);
    src_bfm.set_transaction_eop(1);
    src_bfm.push_transaction();

    src_bfm.set_transaction_data(96'h11223344_ffffffff_eeeeeeee);
    src_bfm.set_transaction_sop(1);
    src_bfm.set_transaction_eop(1);
    src_bfm.push_transaction();
end

// Tests
initial begin
  wait(dut.st0.empty_i == 0);
  assert(dut.st0.data[7:0] == 8'hAB) else $fatal;
  assert(dut.st0.counter[7:0] == 8'hAA) else $fatal;
  assert(dut.st0.control[7:0] == 8'h12) else $fatal;
end

integer fh;

initial begin
  #10 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_if_1.ucdb");
  $finish;
end

endmodule: tb_st_if_1
