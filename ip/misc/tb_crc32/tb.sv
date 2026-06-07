// Purpose: CRC32 helper testbench.
//
// Confirms that the CRC block updates deterministically for known input data sequences.
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
  logic [31:0] exp_crc_out_pipe [0:15];
  logic        exp_valid_pipe   [0:15];

  localparam int DUT_LAT = 1; // crc_valid asserted one cycle after data_en

  task automatic pipe_shift(input logic v, input logic [31:0] crc_state_after_word);
    int k;
    logic [31:0] outval;
    begin
      outval = crc_state_after_word ^ CRC_XOROUT;

      // shift pipeline
      for (k = 15; k > 0; k--) begin
        exp_valid_pipe[k]   = exp_valid_pipe[k-1];
        exp_crc_out_pipe[k] = exp_crc_out_pipe[k-1];
      end
      exp_valid_pipe[0]   = v;
      exp_crc_out_pipe[0] = outval;
    end
  endtask

  // --------------------------------------------------------------------------
  // Drive a single word
  // --------------------------------------------------------------------------
  task automatic send_word(input logic [31:0] w, input logic en = 1'b1);
    begin
      @(negedge clk);
      data_in <= w;
      data_en <= en;

      // update expected state when enabled
      if (en) begin
        exp_crc_state <= crc32_update_word_reflected(exp_crc_state, w);
        // expected output corresponds to crc AFTER consuming this word
        pipe_shift(1'b1, crc32_update_word_reflected(exp_crc_state, w));
      end else begin
        pipe_shift(1'b0, exp_crc_state);
      end

      @(negedge clk);
      data_en <= 1'b0;
      data_in <= '0;
      // pipeline advances each cycle regardless
      pipe_shift(1'b0, exp_crc_state);
    end
  endtask

  // --------------------------------------------------------------------------
  // Checker
  // --------------------------------------------------------------------------
  always_ff @(posedge clk) begin
    if (rst_n) begin
      if (crc_valid) begin
        if (!exp_valid_pipe[DUT_LAT]) begin
          $fatal(1, "DUT asserted crc_valid unexpectedly.");
        end
        if (crc_out !== exp_crc_out_pipe[DUT_LAT]) begin
          $display("Mismatch @%0t: got 0x%08x exp 0x%08x",
                   $time, crc_out, exp_crc_out_pipe[DUT_LAT]);
          $fatal(1, "CRC mismatch.");
        end
      end
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

    for (n = 0; n < 16; n++) begin
      exp_valid_pipe[n]   = 1'b0;
      exp_crc_out_pipe[n] = '0;
    end

    // reset
    rst_n = 1'b0;
    exp_crc_state = CRC_INIT;
    repeat (5) @(posedge clk);
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
    @(negedge clk);
    data_en <= 1'b0;
    data_in <= '0;
    pipe_shift(1'b0, exp_crc_state);
    @(negedge clk);
    pipe_shift(1'b0, exp_crc_state);

    // ----------------------------------------------------------------------
    // Random regression
    // ----------------------------------------------------------------------
    for (n = 0; n < 2000; n++) begin
      rnd = $urandom;
      send_word(rnd, 1'b1);
      // occasional disabled cycles
      if ((n % 37) == 0) begin
        @(negedge clk);
        data_en <= 1'b0;
        data_in <= '0;
        pipe_shift(1'b0, exp_crc_state);
      end
    end

    // Drain a few cycles
    repeat (10) begin
      @(negedge clk);
      data_en <= 1'b0;
      data_in <= '0;
      pipe_shift(1'b0, exp_crc_state);
    end

    $display("All tests PASSED.");
    fh = $fopen("SUCCESS", "w");
    $fclose(fh);
    $finish;
  end

initial begin
  #1000000 $fatal(1, "timeout waiting for CRC32 test completion");
end


endmodule
