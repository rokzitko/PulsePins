// Independent command interpreter with randomized stalls and cancellation of
// finite/infinite replay. Expectations come only from accepted input commands.
`timescale 1ns/1ps
`include "../config.vh"
module tb_pre_reference;
  logic clk = 0, reset = 1;
  always #0.5 clk = ~clk;
  logic [95:0] din = 0;
  logic din_valid = 0, dout_ready = 0;
  wire din_ready, dout_valid;
  wire [95:0] dout;
  preprocessor dut(.clk, .reset, .din, .din_valid, .din_ready, .dout, .dout_valid, .dout_ready);
  logic [95:0] memory_ref [0:7], expected [0:16383];
  int sent = 0, received = 0, stalls = 0, stores = 0, replays = 0, passes = 0, discards = 0;
  bit consume = 0, randomized = 1;
  always @(negedge clk) dout_ready = consume && (!randomized || $urandom_range(0, 3) != 0);
  always @(posedge clk) if (!reset) begin
    if (din_valid && !din_ready) stalls++;
    if (dout_valid) begin
      assert(dout_ready && received < sent) else $fatal(1, "unexpected preprocessor output");
      assert(dout === expected[received]) else $fatal(1, "preprocessor word %0d got=%h expected=%h", received, dout, expected[received]);
      received++;
    end
  end
  task automatic append(input logic [95:0] word);
    assert(sent < 16384) else $fatal(1, "reference capacity exceeded");
    expected[sent] = word; sent++;
  endtask
  task automatic send(input logic [31:0] ctrl, count, data);
    @(negedge clk); din = {ctrl, count, data}; din_valid = 1;
    do @(posedge clk); while (!din_ready);
    @(negedge clk); din_valid = 0;
  endtask
  task automatic store_word(input int position);
    logic [95:0] word;
    word = {32'((1 << BIT_NOPASS) | (1 << BIT_STORE) | (position << BIT_POSITIONS_LO)), $urandom(), $urandom()};
    memory_ref[position] = word;
    send(word[95:64], word[63:32], word[31:0]); stores++;
  endtask
  task automatic replay_words(input int length, repetitions);
    for (int r = 0; r < repetitions; r++)
      for (int n = 0; n < length; n++) append(memory_ref[n]);
    send((1 << BIT_NOPASS) | (1 << BIT_REPLAY), repetitions, length); replays++;
  endtask
  logic [95:0] word;
  int before_reset, length;
  integer fh;
  initial begin
    repeat (4) @(negedge clk); reset = 0; consume = 1;
    for (int n = 0; n < 8; n++) store_word(n);
    for (int n = 0; n < 240; n++) begin
      case (n % 4)
        0: store_word($urandom_range(0, 7));
        1: begin
          word = {32'b0, $urandom(), $urandom()}; append(word);
          send(word[95:64], word[63:32], word[31:0]); passes++;
        end
        2: begin send(1 << BIT_NOPASS, $urandom, $urandom); discards++; end
        3: replay_words($urandom_range(1, 8), $urandom_range(1, 4));
      endcase
    end
    wait(received == sent);
    repeat (8) @(negedge clk);
    assert(stores > 8 && passes > 0 && discards > 0 && replays > 0 && stalls > 0) else $fatal(1, "missing preprocessor scenario");
    // Infinite replay has a bounded checked prefix; reset cancels the rest.
    for (int infinite = 0; infinite < 2; infinite++) begin
      consume = 0;
      before_reset = received;
      for (int n = 0; n < 16; n++) append(memory_ref[n % 8]);
      send((1 << BIT_NOPASS) | (1 << BIT_REPLAY), infinite ? 0 : 100, 8);
      randomized = 0; consume = 1;
      wait(received >= before_reset + 5);
      consume = 0;
      @(negedge clk); reset = 1; sent = 0; received = 0;
      repeat (4) @(negedge clk); reset = 0;
      word = {32'b0, 32'd1, 32'hfeed0000 | 32'(infinite)};
      append(word); consume = 1;
      send(word[95:64], word[63:32], word[31:0]);
      wait(received == 1);
      repeat (8) @(negedge clk);
      assert(!dout_valid) else $fatal(1, "stale replay survived reset");
      for (int n = 0; n < 8; n++) store_word(n);
    end
    $display("SUCCESS: independent store/discard/pass/replay model, stalls, finite/infinite replay reset");
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #100000 $fatal(1, "preprocessor reference watchdog: %0d/%0d", received, sent);
endmodule
