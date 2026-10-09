// End-to-end readback plus pulse-count/CRC/status registers under unrelated
// clocks and randomized downstream stalls. Expected RLE/CRC are built offline.
`timescale 1ns/1ps
`include "../rl_config.vh"
module tb_readback_registers;
  logic clk = 0, qin_clk = 0, reset = 1;
  always #0.5 clk = ~clk;
  initial begin #0.17; forever #0.7 qin_clk = ~qin_clk; end
  logic [31:0] qin = 0;
  logic qin_valid = 0, aso_ready = 0;
  wire [95:0] aso_data;
  wire aso_valid;
  logic [1:0] avs_s0_address = 0;
  logic avs_s0_read = 0, avs_s0_write = 0;
  logic [31:0] avs_s0_writedata = 0;
  wire [31:0] avs_s0_readdata;
  rl_encoder_if dut(.clk, .reset, .qin, .qin_valid, .qin_clk, .qin_strobe(1'b0),
    .aso_data, .aso_valid, .aso_ready, .avs_s0_address, .avs_s0_read,
    .avs_s0_write, .avs_s0_writedata, .avs_s0_readdata);
  logic [31:0] values [0:255];
  logic [95:0] expected [0:255];
  logic [31:0] crc, got;
  int expected_count = 0, received = 0, run_length, stalled = 0;
  int sampled_inputs = 0, checked_registers = 0, checked_epochs = 0;
  logic [1:0] readback_modes = 0;
  always @(posedge qin_clk) if (!reset && qin_valid && !dut.input_reset) sampled_inputs++;
  logic [31:0] run_value;
  always @(negedge clk) aso_ready = !reset && ($urandom_range(0, 3) != 0);
  always @(posedge clk) if (!reset) begin
    if (aso_valid && !aso_ready) stalled++;
    if (aso_valid && aso_ready) begin
      assert(received < expected_count && aso_data === expected[received]) else $fatal(1, "readback run %0d mismatch: %h", received, aso_data);
      received++;
    end
  end
  function automatic logic [31:0] crc_word(input logic [31:0] state, word);
    logic [31:0] c;
    c = state;
    for (int bit_idx = 0; bit_idx < 32; bit_idx++)
      c = (c >> 1) ^ ((c[0] ^ word[bit_idx]) ? 32'hedb88320 : 32'b0);
    crc_word = c;
  endfunction
  task automatic write_reg(input logic [1:0] address, input logic [31:0] value);
    @(negedge clk); avs_s0_address = address; avs_s0_writedata = value; avs_s0_write = 1;
    @(negedge clk); avs_s0_write = 0;
  endtask
  task automatic read_reg(input logic [1:0] address, input logic [31:0] value);
    @(negedge clk); avs_s0_address = address; avs_s0_read = 1;
    @(posedge clk); #1ps;
    assert(avs_s0_readdata === value) else $fatal(1, "readback register %0d got=%h expected=%h", address, avs_s0_readdata, value);
    checked_registers++;
    if (address == 0) readback_modes[avs_s0_readdata[2]] = 1;
    @(negedge clk); avs_s0_read = 0;
  endtask
  integer fh;
  initial begin
    repeat (10) @(negedge qin_clk); reset = 0;
    repeat (20) @(negedge qin_clk);
    for (int mode = 0; mode < 2; mode++) begin
      // In the normal build the stored mode bit must not change sampling;
      // the WEIRD_CLOCK branch is separately checked by the core bench.
      write_reg(1, mode);
      expected_count = 0; received = 0; crc = '1; run_length = 0;
      for (int n = 0; n < 256; n++) begin
        values[n] = n % 16 < 8 ? 32'h12340000 + n/16 : $urandom();
        crc = crc_word(crc, values[n]);
        if (run_length != 0 && values[n] != run_value) begin
          expected[expected_count] = {32'b0, 32'(run_length), run_value}; expected_count++; run_length = 0;
        end
        run_value = values[n]; run_length++;
      end
      expected[expected_count] = {32'b0, 32'(run_length), run_value}; expected_count++;
      for (int n = 0; n < 256; n++) begin
        @(negedge qin_clk); qin = values[n]; qin_valid = 1;
      end
      @(negedge qin_clk); qin_valid = 0;
      wait(received == expected_count);
      repeat (80) @(negedge clk);
      assert(!aso_valid) else $fatal(1, "readback did not drain");
      read_reg(1, 256); read_reg(2, crc ^ 32'hffffffff); read_reg(0, (mode << 2) | 1);
      write_reg(0, 1);
      repeat (40) @(negedge qin_clk);
      read_reg(1, 0); read_reg(2, 0);
      write_reg(0, 0);
      repeat (40) @(negedge qin_clk);
      checked_epochs++;
    end
    assert(stalled > 0) else $fatal(1, "readback stalls not exercised");
    tb_require_scenario("sampled_inputs", sampled_inputs, 512);
    tb_require_scenario("stalled_output", stalled);
    tb_require_scenario("capture_and_reset_epochs", checked_epochs, 2);
    tb_require_scenario("csr_results_checked", checked_registers, 10);
    tb_require_scenario("mode0_readback", readback_modes[0]);
    tb_require_scenario("mode1_readback", readback_modes[1]);
    $display("SUCCESS: public RLE stream, both normal-build mode values, pulse-count/CRC readback and reset");
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #20000 $fatal(1, "readback register watchdog: %0d/%0d", received, expected_count);
  `include "../../testbench/scenario_coverage.svh"
endmodule
