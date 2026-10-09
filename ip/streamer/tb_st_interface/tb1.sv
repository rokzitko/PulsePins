// Purpose: Avalon-ST ingress endianness/word-order test.
//
// Verifies that the software-visible 96-bit transport order is mapped into the internal
// `control/counter/data` layout expected by the streamer wrapper.
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
  .asi_channel(1'b0),
  .asi_ready,
  .streamer_clk(clk),
  .avs_s0_address('0), .avs_s0_read(1'b0), .avs_s0_write(1'b0), .avs_s0_writedata('0),
  .trigger_in('0), .trigger_enable_ext(1'b0), .trigger_force_ext(1'b0), .trigger_reset_ext(1'b0),
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
  $strobe("t=%8.3f asi_valid=%b asi_ready=%b fifo wrreq=%b empty_i=%b y=%h c=%h v=%h",
    $realtime, asi_valid, asi_ready, dut.st0.fifo_i.wrreq,
    dut.st0.empty_i, dut.st0.control, dut.st0.counter, dut.st0.data
  );
end

// Stimulus
bit stimulus_done = 0;
initial begin
    #3;

    // Initialize the BFM
    src_bfm.init();

//                                  control   counter  data
    // Short counters let both words leave the ingress FIFO within this test.
    src_bfm.set_transaction_data(96'h12345678_02000000_abcdefaa);
    src_bfm.set_transaction_sop(1);
    src_bfm.set_transaction_eop(1);
    src_bfm.push_transaction();

    src_bfm.set_transaction_data(96'h11223344_03000000_eeeeeeee);
    src_bfm.set_transaction_sop(1);
    src_bfm.set_transaction_eop(1);
    src_bfm.push_transaction();
    stimulus_done = 1;
end

// Check all mapped bits on acceptance and again when the ingress FIFO delivers
// each word. Correct mapping alone must not hide missing or duplicated delivery.
localparam logic [95:0] EXPECTED_FIRST = 96'h78563412_00000002_aaefcdab;
localparam logic [95:0] EXPECTED_SECOND = 96'h44332211_00000003_eeeeeeee;
int transfers = 0;
int delivered = 0;
always @(posedge clk) begin
  if (!reset && asi_valid && asi_ready) begin
    case (transfers)
      0: assert(dut.input_data === EXPECTED_FIRST) else
           $fatal(1, "first mapped word mismatch: %h", dut.input_data);
      1: assert(dut.input_data === EXPECTED_SECOND) else
           $fatal(1, "second mapped word mismatch: %h", dut.input_data);
      default: $fatal(1, "unexpected extra ingress transfer");
    endcase
    transfers++;
  end
  if (!reset && dut.st0.rdreq_i && !dut.st0.empty_i) begin
    case (delivered)
      0: assert(dut.st0.q_i === EXPECTED_FIRST) else $fatal(1, "first delivered word mismatch: %h", dut.st0.q_i);
      1: assert(dut.st0.q_i === EXPECTED_SECOND) else $fatal(1, "second delivered word mismatch: %h", dut.st0.q_i);
      default: $fatal(1, "unexpected extra FIFO delivery");
    endcase
    delivered++;
  end
end

integer fh;

initial begin
  wait(stimulus_done && transfers == 2 && delivered == 2);
  repeat (8) @(negedge clk);
  assert(dut.st0.empty_i === 1'b1) else $fatal(1, "ingress FIFO did not drain");
  tb_require_scenario("mapped_transfers_checked", transfers, 2);
  tb_require_scenario("fifo_delivery_checked", delivered, 2);
  tb_require_scenario("ingress_drained", dut.st0.empty_i === 1'b1);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_if_1.ucdb");
  $finish;
end

initial #100 $fatal(1, "timeout: accepted %0d/2, delivered %0d/2 transport words", transfers, delivered);

`include "../../testbench/scenario_coverage.svh"
endmodule: tb_st_if_1
