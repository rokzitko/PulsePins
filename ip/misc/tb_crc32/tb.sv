// Purpose: CRC32 helper testbench.
//
// Checks CRC values and validity every cycle for fixed and randomized input data,
// including disabled cycles and back-to-back enabled words. Success requires
// exact result counts and complete pipeline drain.
`timescale 1ns/1ps

module tb_crc32;

  // --------------------------------------------------------------------------
  // Clock / Reset
  // --------------------------------------------------------------------------
  logic clk = 1'b0;
  always #5 clk = ~clk;

  logic rst_n;

  // --------------------------------------------------------------------------
  // DUT I/O
  // --------------------------------------------------------------------------
  logic        data_en;
  logic [31:0] data_in;

  logic [31:0] crc_out;
  logic        crc_valid;

  // --------------------------------------------------------------------------
  // Instantiate DUT
  // --------------------------------------------------------------------------
  // Change module name/ports here if needed.
  crc32 dut (
    .clk      (clk),
    .reset    (~rst_n),
    .data_en  (data_en),
    .data_in  (data_in),
    .crc_out  (crc_out),
    .crc_valid(crc_valid)
  );

  // --------------------------------------------------------------------------
  // Golden CRC model (Ethernet CRC-32, reflected)
  // --------------------------------------------------------------------------
  localparam logic [31:0] POLY_REF   = 32'hEDB88320;
  localparam logic [31:0] CRC_INIT   = 32'hFFFF_FFFF;
  localparam logic [31:0] CRC_XOROUT = 32'hFFFF_FFFF;

  function automatic logic [31:0] crc32_update_word_reflected(
    input logic [31:0] crc,
    input logic [31:0] data
  );
    logic [31:0] c;
    int i;
    begin
      // Reflected: XOR whole word, then shift-right 32 times using POLY_REF
      c = crc ^ data;
      for (i = 0; i < 32; i++) begin
        if (c[0]) c = (c >> 1) ^ POLY_REF;
        else      c = (c >> 1);
      end
      return c;
    end
  endfunction

  // --------------------------------------------------------------------------
  // Scoreboard: expected CRC stream with DUT latency handling
  // --------------------------------------------------------------------------
  logic [31:0] exp_crc_state;
  logic exp_valid;
  int expected_count;
  int received_count;
  int consecutive_count;
  int idle_valid_checks;
  int enabled_blocks;

  // --------------------------------------------------------------------------
  // Drive one cycle; adjacent calls deliberately exercise back-to-back words.
  // --------------------------------------------------------------------------
  task automatic send_word(input logic [31:0] w, input logic en = 1'b1);
    begin
      @(negedge clk);
      data_in <= w;
      data_en <= en;

    end
  endtask

  // --------------------------------------------------------------------------
  // Checker
  // --------------------------------------------------------------------------
  always @(posedge clk) begin
    if (!rst_n) begin
      exp_crc_state = CRC_INIT;
      exp_valid = 0;
      expected_count = 0;
      received_count = 0;
      consecutive_count = 0;
      idle_valid_checks = 0;
      enabled_blocks = 0;
    end else begin
      // Sample before NBA updates: outputs correspond to the previous edge.
      assert(crc_valid === exp_valid) else
        $fatal(1, "CRC validity mismatch: got %b expected %b, received=%0d expected=%0d",
          crc_valid, exp_valid, received_count, expected_count);
      if (exp_valid) begin
        assert(crc_out === (exp_crc_state ^ CRC_XOROUT)) else
          $fatal(1, "CRC result %0d: got %08x expected %08x",
            received_count, crc_out, exp_crc_state ^ CRC_XOROUT);
        received_count++;
      end else idle_valid_checks++;
      if (data_en) begin
        if (exp_valid) consecutive_count++; else enabled_blocks++;
        exp_crc_state = crc32_update_word_reflected(exp_crc_state, data_in);
        expected_count++;
      end
      exp_valid = data_en;
    end
  end

  // --------------------------------------------------------------------------
  // Main test sequence
  // --------------------------------------------------------------------------
  int n;
  logic [31:0] rnd;
  integer fh;

  initial begin
    // init
    data_en = 1'b0;
    data_in = '0;

    // reset
    rst_n = 1'b0;
    repeat (5) @(negedge clk);
    rst_n = 1'b1;
    repeat (2) @(posedge clk);

    // ----------------------------------------------------------------------
    // Deterministic pattern test
    // ----------------------------------------------------------------------
    // A few fixed words; checks streaming behavior and output alignment.
    send_word(32'h0000_0000, 1'b1);
    send_word(32'hFFFF_FFFF, 1'b1);
    send_word(32'h1234_5678, 1'b1);
    send_word(32'hDEAD_BEEF, 1'b1);

    // Insert an idle (data_en=0) gap
    send_word(32'hBAD0_DA7A, 1'b0);
    send_word('0, 1'b0);

    // ----------------------------------------------------------------------
    // Random regression
    // ----------------------------------------------------------------------
    for (n = 0; n < 2000; n++) begin
      rnd = $urandom;
      send_word(rnd, 1'b1);
      // occasional disabled cycles
      if ((n % 37) == 0) begin
        send_word($urandom, 1'b0);
      end
    end

    // Drain a few cycles
    repeat (10) begin
      send_word('0, 1'b0);
    end

    assert(expected_count == 2004 && received_count == expected_count) else
      $fatal(1, "incomplete CRC drain: received=%0d expected=%0d", received_count, expected_count);
    assert(!exp_valid && crc_valid === 1'b0) else $fatal(1, "CRC output did not go idle");
    assert(consecutive_count > 0) else $fatal(1, "consecutive enabled cycles were not exercised");
    tb_require_scenario("crc_results_checked", received_count, 2004);
    tb_require_scenario("consecutive_enabled_cycles", consecutive_count);
    tb_require_scenario("idle_validity_checks", idle_valid_checks, 10);
    tb_require_scenario("enabled_blocks_separated_by_gaps", enabled_blocks, 2);
    tb_require_scenario("pipeline_drained", !exp_valid && crc_valid === 1'b0);
    $display("All tests PASSED: %0d CRC results, %0d consecutive enabled cycles.", received_count, consecutive_count);
    fh = $fopen("SUCCESS", "w");
    $fclose(fh);
    $finish;
  end

initial begin
  #1000000 $fatal(1, "timeout waiting for CRC32 test completion");
end


`include "../../testbench/scenario_coverage.svh"
endmodule
