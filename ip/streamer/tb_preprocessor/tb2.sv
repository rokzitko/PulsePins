// Storage of elements, etc.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_pre_2;

reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #1 reset <= 0;
end

// Interface
reg [`WIDTH_TOTAL-1:0] din;
reg din_valid;
wire din_ready;

wire [`WIDTH_TOTAL-1:0] dout;
wire dout_valid;
reg dout_ready;

always @(posedge clk) begin
  $strobe("t=%8.3f reset=%b control=%h counter=%h data=%h din_valid=%b din_ready=%b | pass=%b discard=%b store=%b replay=%b | i=%d j=%d | dout=%h dout_valid=%b", $realtime, reset,
    dut.control, dut.counter, dut.data, din_valid, din_ready,
    dut.pass, dut.discard, dut.store, dut.replay,
    dut.i, dut.j,
    dout, dout_valid
   );
end

preprocessor dut(
.clk,
.reset,

.din,
.din_valid,
.din_ready,

.dout,
.dout_valid,
.dout_ready
);

// FIFO accepts everything; we don't simulate output throttling in this test
initial begin
  dout_ready <= 1;
end

localparam [31:0] PASS = 0;
localparam [31:0] NOPASS = 1 << `BIT_NOPASS;
localparam [31:0] STORE0 = (1 << `BIT_NOPASS) + (1 << `BIT_STORE) + (0 << `BIT_POSITIONS_LO);
localparam [31:0] STORE1 = (1 << `BIT_NOPASS) + (1 << `BIT_STORE) + (1 << `BIT_POSITIONS_LO);
localparam [31:0] STORE2 = (1 << `BIT_NOPASS) + (1 << `BIT_STORE) + (2 << `BIT_POSITIONS_LO);
localparam [31:0] STORE3 = (1 << `BIT_NOPASS) + (1 << `BIT_STORE) + (3 << `BIT_POSITIONS_LO);
localparam [31:0] REPLAY = (1 << `BIT_NOPASS) + (1 << `BIT_REPLAY);

initial begin
  din_valid <= 0;
  din <= 96'b0;
  #4
  din_valid <= 1;
  din <= { PASS, 32'h00ff, 32'h01ff }; // pass [X1]
  #1
  din_valid <= 1;
  din <= { NOPASS, 32'h00ff, 32'h02ff }; // do not pass
  #1
  din_valid <= 1;
  din <= { PASS, 32'h00ff, 32'h03ff }; // pass [X2]
  #1
  din_valid <= 1;
  din <= { STORE0, 32'h1234, 32'h04ff }; // store (do not pass)
  #1
  din_valid <= 1;
  din <= { STORE1, 32'h1234, 32'h05ee }; // store (do not pass)
  #1
  din_valid <= 1;
  din <= { STORE2, 32'h1234, 32'h06dd }; // store (do not pass)
  #1
  din_valid <= 1;
  din <= { STORE3, 32'h1234, 32'h07cc }; // store (do not pass)
  #1
  din_valid <= 1;
  din <= { PASS, 32'h00ff, 32'h08ff }; // pass [X3]
  #1
  din_valid <= 1;
  din <= { PASS, 32'h00ff, 32'h09ff }; // pass [X4]
  #1

  din_valid <= 1;
  din <= { REPLAY, 32'h0001, 32'h04 }; // replay 1 time (do not pass) [A]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  @(posedge din_ready); // we need to wait!

  #1
  din_valid <= 1;
  din <= { PASS, 32'h00ff, 32'h0aff }; // pass [Y1]
  #1
  din_valid <= 1;
  din <= { PASS, 32'h00ff, 32'h0bff }; // pass [Y2]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  #1

  // corner cases
  din_valid <= 1;
  din <= { REPLAY, 32'h0001, 32'h0 }; // replay 1 time, length=0 [] -> no output!
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  // @(posedge din_ready); // NO NEED TO WAIT, because din_ready is not deasserted!

  #1
  din_valid <= 1;
  din <= { REPLAY, 32'h0001, 32'h1 }; // replay 1 time, length=1 [B]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  @(posedge din_ready); // we need to wait!

  #1
  din_valid <= 1;
  din <= { REPLAY, 32'h0001, 32'h2 }; // replay 1 time, length=2 [C]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  @(posedge din_ready); // we need to wait!

  #1
  din_valid <= 1;
  din <= { REPLAY, 32'h0002, 32'h0 }; // replay 2 times, length=0
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  // @(posedge din_ready); // NO NEED TO WAIT

  #1
  din_valid <= 1;
  din <= { REPLAY, 32'h0002, 32'h1 }; // replay 2 times, length=1 [D]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  @(posedge din_ready); // we need to wait!

  #1
  din_valid <= 1;
  din <= { REPLAY, 32'h0002, 32'h2 }; // replay 2 times, length=2 [E]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  @(posedge din_ready); // we need to wait!

  #1
  din_valid <= 1;
  din <= { REPLAY, 32'h0001, 32'(`MEMORY_POSITIONS) }; // replay 1 times, length=max [F]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  @(posedge din_ready); // we need to wait!

  #1
  din_valid <= 1;
  din <= { REPLAY, 32'h0002, 32'(`MEMORY_POSITIONS) }; // replay 2 times, length=max [G]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  @(posedge din_ready); // we need to wait!

  #1
  din_valid <= 1;
  din <= { REPLAY, 32'h0003, 32'(`MEMORY_POSITIONS) }; // replay 3 times, length=max [H]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  @(posedge din_ready); // we need to wait!

  #1
  din_valid <= 1;
  din <= { REPLAY, 32'h0, 32'h04 }; // infinite replay (never stops, must be the last case) [I]
  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
  @(posedge din_ready); // we need to wait!

  #1
  din_valid <= 0;
  din <= { PASS, 32'h0000, 32'hffffffff }; // zero/one
end

// checker for expected output
integer i;
initial begin
  // [X]
  @(posedge clk iff dout_valid);
  assert(dout == 'h00000000000000ff000001ff) else $fatal;
  @(posedge clk iff dout_valid);
  assert(dout == 'h00000000000000ff000003ff) else $fatal;
  @(posedge clk iff dout_valid);
  assert(dout == 'h00000000000000ff000008ff) else $fatal;
  @(posedge clk iff dout_valid);
  assert(dout == 'h00000000000000ff000009ff) else $fatal;

  // [A], once
  for (i = 0; i < 1; i++) begin
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000004ff) else $fatal;
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000005ee) else $fatal;
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000006dd) else $fatal;
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000007cc) else $fatal;
  end

  // [Y]
  @(posedge clk iff dout_valid);
  assert(dout == 'h00000000000000ff00000aff) else $fatal;
  @(posedge clk iff dout_valid);
  assert(dout == 'h00000000000000ff00000bff) else $fatal;

  // [B]
  @(posedge clk iff dout_valid);
  assert(dout[63:0] == 'h00001234000004ff) else $fatal;

  // [C]
  @(posedge clk iff dout_valid);
  assert(dout[63:0] == 'h00001234000004ff) else $fatal;
  @(posedge clk iff dout_valid);
  assert(dout[63:0] == 'h00001234000005ee) else $fatal;

  // [D]
  for (i=0; i<2; i++) begin
    @(posedge clk iff dout_valid);
    assert(dout[63:0] == 'h00001234000004ff) else $fatal;
  end

  // [E]
  for (i=0; i<2; i++) begin
    @(posedge clk iff dout_valid);
    assert(dout[63:0] == 'h00001234000004ff) else $fatal;
    @(posedge clk iff dout_valid);
    assert(dout[63:0] == 'h00001234000005ee) else $fatal;
  end

  // [F], [G], [H], 6 times in total
  for (i = 0; i < 6; i++) begin
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000004ff) else $fatal;
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000005ee) else $fatal;
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000006dd) else $fatal;
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000007cc) else $fatal;
   @(posedge clk iff dout_valid);
//   assert(dout[63:0] == 'h0) else $fatal;
   @(posedge clk iff dout_valid);
//   assert(dout[63:0] == 'h0) else $fatal;
   @(posedge clk iff dout_valid);
//   assert(dout[63:0] == 'h0) else $fatal;
   @(posedge clk iff dout_valid);
//   assert(dout[63:0] == 'h0) else $fatal;
  end

  // [I]
  for (i = 0; ; i++) begin
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000004ff) else $fatal;
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000005ee) else $fatal;
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000006dd) else $fatal;
   @(posedge clk iff dout_valid);
   assert(dout[63:0] == 'h00001234000007cc) else $fatal;
  end
end

initial begin
   #12;
   $strobe("%h", dut.memory[0]);
   $strobe("%h", dut.memory[1]);
   $strobe("%h", dut.memory[2]);
   $strobe("%h", dut.memory[3]);
   $strobe("%h", dut.memory[4]);
end

integer fh;

initial begin
  #200 $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_pre_2.ucdb");
  $finish;
end

endmodule: tb_pre_2
