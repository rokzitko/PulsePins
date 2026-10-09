// Public event schedule predicts exact timestamps (first sampled edge + two
// cycles), including modulo wrap. Only saturation setup seeds internal state;
// all event, acceptance, reset, and overflow verdicts use the public interface.
`timescale 1ns/1ps
module tb_timestamp_reference;
  logic clk = 0, reset = 1;
  always #1 clk = ~clk;
  logic [1:0] sig = 0, ready = 0;
  wire [1:0] valid;
  wire [7:0] data [0:1];
  logic [1:0] avs_s0_address = 0;
  logic avs_s0_read = 0, avs_s0_write = 0;
  logic [31:0] avs_s0_writedata = 0;
  wire [31:0] avs_s0_readdata;
  ts_core #(.CTR_W(8)) dut(.clk, .reset, .sig(sig[0]), .sigA(sig[1]),
    .aso_ready(ready[0]), .asoA_ready(ready[1]), .aso_valid(valid[0]), .asoA_valid(valid[1]),
    .aso_data(data[0]), .asoA_data(data[1]), .avs_s0_address, .avs_s0_read,
    .avs_s0_write, .avs_s0_writedata, .avs_s0_readdata);
  int cycle = 0, epoch = 0, due [0:1];
  bit pending [0:1], overflow_flag [0:1];
  logic [7:0] timestamp [0:1];
  logic [31:0] dropped [0:1], expected_register;
  int delivered = 0, simultaneous = 0, coincident = 0, overflows = 0;
  int held_channels [0:1], timestamp_wraps [0:1];
  int simultaneous_transfers = 0, pending_resets = 0, saturation_checks = 0, clear_checks = 0;
  bit have_timestamp [0:1];
  logic [7:0] last_timestamp [0:1];
  always @(posedge reset) if (|valid) pending_resets++;
  always @(posedge sig[0]) if (!reset) due[0] = cycle + 3;
  always @(posedge sig[1]) if (!reset) due[1] = cycle + 3;
  always @(posedge clk) begin
    if (!reset) begin
      if (valid == 3 && ready == 3) simultaneous_transfers++;
      for (int ch = 0; ch < 2; ch++) begin
        if (valid[ch] && !ready[ch] && ready[1-ch]) held_channels[ch]++;
        if (valid[ch] && ready[ch]) begin
          if (have_timestamp[ch] && data[ch] < last_timestamp[ch]) timestamp_wraps[ch]++;
          last_timestamp[ch] = data[ch]; have_timestamp[ch] = 1;
        end
      end
    end else begin
      have_timestamp[0] = 0; have_timestamp[1] = 0;
    end
    cycle++;
    expected_register = 0;
    if (reset) begin
      epoch = cycle;
      for (int ch = 0; ch < 2; ch++) begin
        due[ch] = -1; pending[ch] = 0; timestamp[ch] = 0; dropped[ch] = 0; overflow_flag[ch] = 0;
      end
    end else begin
      case (avs_s0_address)
        0: expected_register = (int'(overflow_flag[1]) << 9) | (int'(overflow_flag[0]) << 8) | (int'(pending[1]) << 1) | pending[0];
        2: expected_register = dropped[0];
        3: expected_register = dropped[1];
        default: expected_register = 0;
      endcase
      if (due[0] == cycle && due[1] == cycle) simultaneous++;
      for (int ch = 0; ch < 2; ch++) begin
        if (pending[ch] && ready[ch]) delivered++;
        if (avs_s0_write && avs_s0_address == 1 && avs_s0_writedata[ch]) begin
          dropped[ch] = 0; overflow_flag[ch] = 0;
        end
        if (due[ch] == cycle) begin
          if (pending[ch] && !ready[ch]) begin
            if (dropped[ch] != 32'hffffffff) dropped[ch]++;
            overflow_flag[ch] = 1; overflows++;
          end else begin
            if (pending[ch] && ready[ch]) coincident++;
            timestamp[ch] = 8'(cycle - epoch - 1);
            pending[ch] = 1;
          end
        end else if (pending[ch] && ready[ch]) pending[ch] = 0;
      end
    end
    #1ps;
    for (int ch = 0; ch < 2; ch++) begin
      assert(valid[ch] === pending[ch]) else $fatal(1, "timestamp channel %0d validity mismatch", ch);
      if (pending[ch]) assert(data[ch] === timestamp[ch]) else
        $fatal(1, "timestamp channel %0d got=%0d expected=%0d cycle=%0d", ch, data[ch], timestamp[ch], cycle);
    end
    if (!reset && avs_s0_read) assert(avs_s0_readdata === expected_register) else $fatal(1, "timestamp status/overflow readback mismatch");
  end
  task automatic pulse(input logic [1:0] channels);
    @(negedge clk); #0.13; sig = channels;
    repeat (2) @(negedge clk); sig = 0;
  endtask
  task automatic read_reg(input logic [1:0] address, input logic [31:0] expected);
    @(negedge clk); avs_s0_address = address; avs_s0_read = 1;
    @(posedge clk); #2ps;
    assert(avs_s0_readdata === expected) else $fatal(1, "timestamp register %0d got=%h expected=%h", address, avs_s0_readdata, expected);
    if (address >= 2 && expected == 32'hffffffff) saturation_checks++;
    if (address >= 2 && expected == 0) clear_checks++;
    @(negedge clk); avs_s0_read = 0;
  endtask
  initial begin
    for (int ch = 0; ch < 2; ch++) begin held_channels[ch] = 0; timestamp_wraps[ch] = 0; have_timestamp[ch] = 0; end
    repeat (5) @(negedge clk); reset = 0; ready = 3;
    repeat (8) begin pulse(3); repeat (6) @(negedge clk); end
    // Each sink stalls independently; activity on its peer must not consume or
    // overwrite the held record.
    ready = 1; pulse(3); repeat (6) @(negedge clk); read_reg(0, 2);
    pulse(1); repeat (6) @(negedge clk); read_reg(0, 2);
    ready = 2; repeat (4) @(negedge clk);
    pulse(3); repeat (6) @(negedge clk); read_reg(0, 1);
    pulse(2); repeat (6) @(negedge clk); read_reg(0, 1);
    ready = 3; repeat (4) @(negedge clk);
    ready = 0; pulse(3); repeat (6) @(negedge clk);
    pulse(3);
    // pulse() returns on the edge immediately before its scheduled capture.
    ready = 3;
    repeat (6) @(negedge clk);
    assert(coincident == 2) else $fatal(1, "capture-with-consumption was not exercised");
    ready = 0; pulse(3); repeat (6) @(negedge clk);
    // A 32-bit saturation boundary cannot be reached by a bounded event stream.
    // Force/release only the initial count, then use real input events and CSR
    // reads. Force avoids adding a second procedural driver to always_ff state.
    force dut.overflow_count_r = 32'hfffffffe;
    force dut.overflowA_count_r = 32'hfffffffe;
    dropped[0] = 32'hfffffffe; dropped[1] = 32'hfffffffe;
    @(negedge clk);
    release dut.overflow_count_r;
    release dut.overflowA_count_r;
    repeat (2) begin pulse(3); repeat (6) @(negedge clk); end
    read_reg(2, 32'hffffffff); read_reg(3, 32'hffffffff);
    @(negedge clk); avs_s0_address = 1; avs_s0_writedata = 3; avs_s0_write = 1;
    @(negedge clk); avs_s0_write = 0;
    read_reg(2, 0); read_reg(3, 0);
    assert(valid == 3) else $fatal(1, "pending-reset case was not reached");
    @(negedge clk); reset = 1;
    repeat (4) @(negedge clk); reset = 0; ready = 3;
    // More than 256 cycles guarantees natural timestamp-counter wrap.
    for (int n = 0; n < 64; n++) begin
      pulse(n % 3 == 0 ? 2'b11 : (n % 2 ? 2'b01 : 2'b10));
      repeat (4) @(negedge clk);
      read_reg(0, 0);
    end
    assert(delivered > 80 && simultaneous >= 8 && overflows >= 4 && coincident == 2) else $fatal(1, "missing timestamp coverage");
    tb_require_scenario("timestamp_transfers_checked", delivered, 81);
    tb_require_scenario("simultaneous_output_transfers", simultaneous_transfers, 8);
    tb_require_scenario("capture_with_consumption", coincident, 2);
    tb_require_scenario("reset_pending_capture", pending_resets);
    tb_require_scenario("saturated_overflow_readbacks", saturation_checks, 2);
    tb_require_scenario("cleared_overflow_readbacks", clear_checks, 2);
    for (int ch = 0; ch < 2; ch++) begin
      tb_require_scenario($sformatf("channel_%0d_independent_stall", ch), held_channels[ch]);
      tb_require_scenario($sformatf("channel_%0d_timestamp_wrap", ch), timestamp_wraps[ch]);
    end
    $display("SUCCESS: exact dual timestamps, spacing/wrap, simultaneous acceptance, reset pending, overflow saturation");
    $finish;
  end
  initial #20000 $fatal(1, "timestamp reference watchdog");
  `include "../../testbench/scenario_coverage.svh"
endmodule
