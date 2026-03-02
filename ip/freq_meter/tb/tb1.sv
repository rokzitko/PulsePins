`timescale 1ns/1ps

module tb1;

  localparam int N_CH      = 3;
  localparam int COUNTER_W = 32;
  localparam int ADDR_W    = 4;

  // Clocks / resets
  logic avs_clk, avs_reset;
  logic cnt_clk, cnt_reset;

  // Inputs to DUT (three measured clocks)
  logic [N_CH-1:0] sig_in;

  // Avalon-MM
  logic [ADDR_W-1:0] avs_address;
  logic              avs_read, avs_write;
  logic [31:0]       avs_writedata;
  wire  [31:0]       avs_readdata;
  wire               avs_waitrequest;

  // DUT
  freq_meter_avalon_gray #(
    .N_CH(N_CH),
    .COUNTER_W(COUNTER_W)
  ) dut (
    .avs_clk(avs_clk),
    .avs_reset(avs_reset),
    .avs_address(avs_address),
    .avs_read(avs_read),
    .avs_write(avs_write),
    .avs_writedata(avs_writedata),
    .avs_readdata(avs_readdata),
    .avs_waitrequest(avs_waitrequest),
    .cnt_clk(cnt_clk),
    .cnt_reset(cnt_reset),
    .sig_in(sig_in)
  );

  // --------------------------------------------------------------------------
  // Clock generation
  // --------------------------------------------------------------------------
  // Pick a base period; ratios: 1x, 1/2x, 1/3x relative to cnt_clk frequency
  // So periods: T, 2T, 3T.
  localparam time CNT_HALF = 5ns;  // cnt_clk period = 10 ns (100 MHz)
  localparam time CNT_PER  = 2*CNT_HALF;

  initial begin
    cnt_clk = 1'b0;
    forever #(CNT_HALF) cnt_clk = ~cnt_clk;
  end

  // avs_clk can be independent; keep it slower for visibility (50 MHz)
  initial begin
    avs_clk = 1'b0;
    forever #10ns avs_clk = ~avs_clk; // 20 ns period
  end

  // Measured clocks
  // ch0: same as cnt_clk (period T)
  initial begin
    sig_in[0] = 1'b0;
    forever #(CNT_HALF) sig_in[0] = ~sig_in[0];
  end

  // ch1: half frequency (period 2T)
  initial begin
    sig_in[1] = 1'b0;
    forever #(CNT_PER) sig_in[1] = ~sig_in[1]; // half freq => double period, half-rate toggles => use full period/2 toggles
  end

  // ch2: one third frequency (period 3T)
  initial begin
    sig_in[2] = 1'b0;
    forever #(3*CNT_HALF) sig_in[2] = ~sig_in[2];
  end

  // --------------------------------------------------------------------------
  // Avalon helpers (word addressing)
  // --------------------------------------------------------------------------
  task automatic avs_write32(input [ADDR_W-1:0] addr, input [31:0] data);
    begin
      @(posedge avs_clk);
      avs_address   <= addr;
      avs_writedata <= data;
      avs_write     <= 1'b1;
      avs_read      <= 1'b0;
      @(posedge avs_clk);
      avs_write     <= 1'b0;
    end
  endtask

  task automatic avs_read32(input [ADDR_W-1:0] addr, output [31:0] data);
    begin
      @(posedge avs_clk);
      avs_address <= addr;
      avs_read    <= 1'b1;
      avs_write   <= 1'b0;
      @(posedge avs_clk);
      data        = avs_readdata;
      avs_read    <= 1'b0;
    end
  endtask

  // Address map (must match DUT)
  localparam [ADDR_W-1:0] A_CTRL     = 8'h00;
  localparam [ADDR_W-1:0] A_GATE_LEN = 8'h01;
  localparam [ADDR_W-1:0] A_NCH      = 8'h02;
  localparam [ADDR_W-1:0] A_RES_BASE = 8'h04;

  // --------------------------------------------------------------------------
  // Stimulus / checks
  // --------------------------------------------------------------------------
  int unsigned gate_len;
  int unsigned r0, r1, r2;
  int unsigned exp0, exp1, exp2;

  initial begin
    // Defaults
    avs_address   = '0;
    avs_writedata = '0;
    avs_read      = 1'b0;
    avs_write     = 1'b0;

    // Apply resets
    avs_reset = 1'b1;
    cnt_reset = 1'b1;
    repeat (5) @(posedge avs_clk);
    repeat (5) @(posedge cnt_clk);
    avs_reset = 1'b0;
    cnt_reset = 1'b0;

    // Program gate length in cnt_clk cycles
    // Choose multiple of 6 to make expected counts integers for 1, 1/2, 1/3 ratios.
    gate_len = 600; // cnt_clk cycles per gate
    avs_write32(A_GATE_LEN, gate_len);

    // Enable and clear (clear is W1P)
    avs_write32(A_CTRL, 32'h0000_0002); // clear
    avs_write32(A_CTRL, 32'h0000_0001); // enable

    // Wait for a couple of gate intervals to pass (in cnt_clk domain)
    // Gate interval time = gate_len * CNT_PER
    #(gate_len * CNT_PER * 2);

    // Read results
    avs_read32(A_RES_BASE + 0, r0);
    avs_read32(A_RES_BASE + 1, r1);
    avs_read32(A_RES_BASE + 2, r2);

    // Expected rising edges per gate:
    // - ch0 same as cnt_clk: one rising per cnt_clk period => ~gate_len edges
    // - ch1 half freq: edges per gate = gate_len/2
    // - ch2 one third freq: edges per gate = gate_len/3
    //
    // With this DUT, the gate is defined by cnt_clk cycles; for ideal aligned clocks,
    // these should match exactly. To allow for boundary effects, use small tolerance.
    exp0 = gate_len;
    exp1 = gate_len/2;
    exp2 = gate_len/3;

    $display("GateLen=%0d cycles", gate_len);
    $display("RESULT0=%0d (exp ~%0d)", r0, exp0);
    $display("RESULT1=%0d (exp ~%0d)", r1, exp1);
    $display("RESULT2=%0d (exp ~%0d)", r2, exp2);

    // Tolerance (boundary +/-1..2 possible due to async resets / phase)
    if ((r0 < exp0-2) || (r0 > exp0+2)) $fatal(1, "ch0 out of range");
    if ((r1 < exp1-2) || (r1 > exp1+2)) $fatal(1, "ch1 out of range");
    if ((r2 < exp2-2) || (r2 > exp2+2)) $fatal(1, "ch2 out of range");

    $display("PASS");
    #100ns;
    $finish;
  end

endmodule
