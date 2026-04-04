// Shared initialization, tracing, and symbolic control encodings for the combined
// input-FIFO + preprocessor testbench family.
//
// These tests focus on how ingress buffering and the store/replay preprocessor interact, so the
// common file provides the repeated clock/reset setup, DUT wiring, and debug visibility.
reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #1 reset <= 0;
end

// Interface
reg [WIDTH_TOTAL-1:0] data; // din
reg wrreq;                   // din_valid
wire full;                   // ~din_ready
wire din_ready = ~full;

wire [WIDTH_TOTAL-1:0] q;   // dout
wire empty;                  // ~dout_valid
reg rdreq;                   // dout_ready

always @(posedge clk) begin
  $strobe("t=%8.3f d=%4h wr=%b | co=%h ctr=%h d=%h din_valid=%b din_ready=%b | pass=%b di=%b store=%b replay=%b | i=%d j=%d dout=%4h valid=%h | used2=%d q=%4h rdreq=%b empty=%b", $realtime,
    data, wrreq,
    dut.proc.control, dut.proc.counter, dut.proc.data, dut.proc.din_valid, dut.proc.din_ready,
    dut.proc.pass, dut.proc.discard, dut.proc.store, dut.proc.replay,
    dut.proc.i, dut.proc.j,
    dut.proc.dout, dut.proc.dout_valid,
    dut.used2,
    q, rdreq, empty
   );
end

input_fifo dut(
.clk,
.reset,

.data,
.wrreq,
.full,

.q,
.empty,
.rdreq
);

localparam [31:0] PASS = 0;
localparam [31:0] NOPASS = 1 << BIT_NOPASS;
localparam [31:0] TERMINATE = 1 << BIT_TERMINATE;
localparam [31:0] STORE0 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (0 << BIT_POSITIONS_LO);
localparam [31:0] STORE1 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (1 << BIT_POSITIONS_LO);
localparam [31:0] STORE2 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (2 << BIT_POSITIONS_LO);
localparam [31:0] STORE3 = (1 << BIT_NOPASS) + (1 << BIT_STORE) + (3 << BIT_POSITIONS_LO);
localparam [31:0] REPLAY = (1 << BIT_NOPASS) + (1 << BIT_REPLAY);
