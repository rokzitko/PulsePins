// Purpose: no-strobe output FIFO data-bus update regression.
//
// A BIT_NO_STROBE data element must still update qout while suppressing only qout_valid
// and therefore qout_strobe. This test exercises output_fifo directly so the data-bus
// contract is not hidden by readback, which intentionally follows qout_valid.

`timescale 1ns/1ps

`include "../config.vh"

`default_nettype none

module tb_st_11;

reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

localparam logic [WIDTH_CONTROL-1:0] CONTROL_REGULAR  = '0;
localparam logic [WIDTH_CONTROL-1:0] CONTROL_NOSTROBE = {{(WIDTH_CONTROL-1){1'b0}}, 1'b1} << BIT_NO_STROBE;
localparam logic [WIDTH_CONTROL-1:0] CONTROL_FINAL    = {{(WIDTH_CONTROL-1){1'b0}}, 1'b1} << BIT_TERMINATE;
localparam logic [WIDTH_DATA-1:0] NO_STROBE_VALUE = 32'h00000055;
localparam logic [WIDTH_DATA-1:0] STROBE_VALUE    = 32'h000000aa;
localparam logic [WIDTH_DATA-1:0] FINAL_VALUE     = 32'h000000cc;

reg [WIDTH_DATACTRL-1:0] data;
reg wrreq;
reg rdreq;

wire [WIDTH_DATA-1:0] qout;
wire qout_valid;
wire qout_written;
wire strobe;
wire strobe_enable;
wire almost_full;
wire done;
wire terminal_seen;
wire buffer_error;
wire retrig_requested;
wire [WIDTH_STAT-1:0] ctr_in;
wire [WIDTH_STAT-1:0] ctr_out;

output_fifo dut (
  .wrclk(clk),
  .rdclk(clk),
  .reset(reset),
  .rdrst(reset),
  .data(data),
  .wrreq(wrreq),
  .rdreq(rdreq),
  .qout(qout),
  .qout_valid(qout_valid),
  .qout_written(qout_written),
  .strobe(strobe),
  .strobe_enable(strobe_enable),
  .almost_full(almost_full),
  .done(done),
  .terminal_seen(terminal_seen),
  .buffer_error(buffer_error),
  .retrig_requested(retrig_requested),
  .ctr_in(ctr_in),
  .ctr_out(ctr_out)
);

always @(posedge clk) begin
  $strobe("t=%8.3f wr=%b rd=%b empty=%b read_fire=%b control=%h q=%h qout=%h valid=%b done=%b ctr_out=%0d",
    $realtime, wrreq, rdreq, dut.empty, dut.read_fire, dut.control, dut.q,
    qout, qout_valid, done, ctr_out);
end

task write_word(input logic [WIDTH_CONTROL-1:0] control,
                input logic [WIDTH_DATA-1:0] value);
  begin
    @(negedge clk);
    data = {control, value};
    wrreq = 1;
    @(negedge clk);
    wrreq = 0;
    data = '0;
  end
endtask

initial begin
  data <= '0;
  wrreq <= 0;
  rdreq <= 0;

  wait(reset == 0);
  repeat (4) @(negedge clk);
  write_word(CONTROL_NOSTROBE, NO_STROBE_VALUE);
  write_word(CONTROL_REGULAR, STROBE_VALUE);
  write_word(CONTROL_FINAL, FINAL_VALUE);

  repeat (4) @(negedge clk);
  rdreq <= 1;
end

integer saw_no_strobe_qout = 0;
integer saw_strobed_qout = 0;

always @(posedge clk) begin
  if (!reset) begin
    if (qout == NO_STROBE_VALUE) begin
      assert(qout_valid == 0) else $fatal;
      saw_no_strobe_qout = 1;
    end
    if (qout_valid && qout == STROBE_VALUE)
      saw_strobed_qout = 1;
    assert(!(qout_valid && qout == NO_STROBE_VALUE)) else $fatal;
  end
end

integer fh;

initial begin
  wait(done == 1);
  #1;
  assert(saw_no_strobe_qout == 1) else $fatal;
  assert(saw_strobed_qout == 1) else $fatal;
  assert(qout == FINAL_VALUE) else $fatal;
  assert(buffer_error == 0) else $fatal;
  assert(ctr_out == 3) else $fatal;
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
`ifndef VERILATOR
  $set_coverage_db_name("run_st_11.ucdb");
`endif
  $finish;
end

initial begin
  #100 $fatal;
end

endmodule: tb_st_11

`default_nettype wire
