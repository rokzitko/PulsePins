// Purpose: clock-domain crossing mailbox testbench.
//
// Verifies that the mailbox-style CDC helper safely transfers data/events between unrelated clock
// domains.
`timescale 1ns/1ps

module tb;

  localparam int WIDTH = 32;

  // Timeouts in out_clk cycles
  localparam int TMO_VALID  = 1000;
  localparam int TMO_MATCH  = 2000;
  localparam int TMO_UPDATE = 2000;

  // DUT I/O
  logic               in_clk, in_reset;
  logic [WIDTH-1:0]   in_data;
  logic               in_valid;

  logic               out_clk, out_reset;
  logic [WIDTH-1:0]   out_data;
  logic               out_req;
  logic               out_valid;

  cdc_mailbox #(.WIDTH(WIDTH)) dut (
    .in_clk, .in_reset, .in_data, .in_valid,
    .out_clk, .out_reset, .out_data, .out_req, .out_valid
  );

  // Clocks
  initial begin in_clk = 1'b0;  forever #5 in_clk  = ~in_clk;  end
  initial begin out_clk = 1'b0; forever #7 out_clk = ~out_clk; end

  // Resets
  initial begin
    in_reset  = 1'b1;
    out_reset = 1'b1;
    in_valid  = 1'b0;
    in_data   = '0;
    out_req   = 1'b0;

    repeat (5) @(posedge in_clk);
    in_reset <= 1'b0;

    repeat (5) @(posedge out_clk);
    out_reset <= 1'b0;
  end

  // Waves (optional)
  initial begin
    $dumpfile("tb.vcd");
    $dumpvars(0, tb);
  end

  // Drive one input word for N in_clk cycles (N>=2 recommended for first word)
  task automatic send_word_n(input logic [WIDTH-1:0] w, input int ncycles);
    @(posedge in_clk);
    in_data  <= w;
    in_valid <= 1'b1;
    repeat (ncycles-1) @(posedge in_clk);
    in_valid <= 1'b0;
  endtask

  // Wait for out_valid with timeout
  task automatic wait_out_valid(input int max_cycles, output bit ok);
    ok = 0;
    for (int i = 0; i < max_cycles; i++) begin
      @(posedge out_clk);
      if (out_valid) begin ok = 1; return; end
    end
  endtask

  // Wait for out_data == value with timeout
  task automatic wait_out_data_eq(input logic [WIDTH-1:0] w,
                                  input int max_cycles,
                                  output bit ok);
    ok = 0;
    for (int i = 0; i < max_cycles; i++) begin
      @(posedge out_clk);
      if (out_data === w) begin ok = 1; return; end
    end
  endtask

  // Wait for out_data to change from prev with timeout
  task automatic wait_out_data_changes(input logic [WIDTH-1:0] prev,
                                       input int max_cycles,
                                       output bit ok);
    ok = 0;
    for (int i = 0; i < max_cycles; i++) begin
      @(posedge out_clk);
      if (out_data !== prev) begin ok = 1; return; end
    end
  endtask

  // Main test
  logic [WIDTH-1:0] held;
  bit ok;
  integer fh;

  initial begin
    // Wait for both resets to be low
    wait (!in_reset && !out_reset);

    // Let both domains run a bit so the 2FF synchronizers settle deterministically
    repeat (10) @(posedge in_clk);
    repeat (10) @(posedge out_clk);

    // Ensure hold is deasserted (explicit)
    @(posedge out_clk);
    out_req <= 1'b0;

    // 1) Send a one-cycle word and verify it appears
    send_word_n(32'h1111_0001, 1);

    wait_out_valid(TMO_VALID, ok);
    if (!ok) $fatal(1, "[TB] TIMEOUT: out_valid did not assert within %0d out_clk cycles", TMO_VALID);

    wait_out_data_eq(32'h1111_0001, TMO_MATCH, ok);
    if (!ok) $fatal(1, "[TB] TIMEOUT: out_data did not match 0x11110001 within %0d out_clk cycles (out_data=0x%08x)",
                    TMO_MATCH, out_data);

    // 2) Assert hold and confirm data doesn't change while held
    @(posedge out_clk);
    out_req <= 1'b1;
    held = out_data;

    // Try to send while held (DUT should apply backpressure and/or output must not change)
    send_word_n(32'h2222_0002, 3);

    repeat (50) begin
      @(posedge out_clk);
      if (out_data !== held)
        $fatal(1, "[TB] FAIL: out_data changed while out_req=1 (held=0x%08x now=0x%08x t=%0t)",
               held, out_data, $time);
    end

    // 3) Release hold and verify the sample captured during hold is committed
    @(posedge out_clk);
    out_req <= 1'b0;

    wait_out_data_eq(32'h2222_0002, TMO_UPDATE, ok);
    if (!ok) $fatal(1, "[TB] TIMEOUT: pending sample did not commit after releasing hold");

    // 4) Send another one-cycle word and verify the exact value, not just any change
    send_word_n(32'h3333_0003, 1);

    wait_out_data_eq(32'h3333_0003, TMO_UPDATE, ok);
    if (!ok) $fatal(1, "[TB] TIMEOUT: one-cycle update did not reach out_data");

    $display("[TB] PASS (out_data=0x%08x out_valid=%0b)", out_data, out_valid);
    fh = $fopen("SUCCESS", "w");
    $fclose(fh);
    $finish;
  end

endmodule
