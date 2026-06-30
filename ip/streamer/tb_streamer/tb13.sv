// Purpose: PRNG output participates in decoder state tracking.
//
// A relative operation following a PRNG element must use the actual random value that was
// emitted, not the placeholder data carried by the PRNG sequence record.

`timescale 1ns/1ps

`include "../config.vh"

`default_nettype none

module tb_st_13;

localparam logic [WIDTH_CONTROL-1:0] CONTROL_PRNG = WIDTH_CONTROL'(1) << BIT_PRNG;
localparam logic [WIDTH_CONTROL-1:0] CONTROL_BITOR = WIDTH_CONTROL'(BITOR) << BIT_MODE_LO;
localparam logic [WIDTH_CONTROL-1:0] CONTROL_FINAL_NOMOD =
  (WIDTH_CONTROL'(1) << BIT_TERMINATE) | (WIDTH_CONTROL'(BITOR) << BIT_MODE_LO);
localparam logic [WIDTH_DATA-1:0] EXPECTED_FIRST_RANDOM = WIDTH_DATA'(32'hffffffff);
localparam logic [WIDTH_DATA-1:0] BITOR_MASK = WIDTH_DATA'(32'h00000001);
localparam logic [WIDTH_DATA-1:0] EXPECTED_AFTER_BITOR = EXPECTED_FIRST_RANDOM | BITOR_MASK;

logic clk;
logic reset;
logic reload_initial;
logic in_valid;
logic [WIDTH_DATA-1:0] in_data;
logic [WIDTH_CONTROL-1:0] in_control;
logic [WIDTH_COUNTER-1:0] in_counter;
logic [3:0] in_opmode;
wire in_rdreq;
wire [WIDTH_DATA-1:0] out_data;
wire [WIDTH_CONTROL-1:0] out_control;
wire out_wrreq;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

assign in_opmode = in_control[BIT_MODE_HI:BIT_MODE_LO];

rl_decoder dut(
  .clk,
  .reset,
  .reload_initial,
  .in_valid,
  .in_data,
  .in_control,
  .in_counter,
  .in_opmode,
  .in_rdreq,
  .out_almost_full(1'b0),
  .out_data,
  .out_control,
  .out_wrreq,
  .initial_data('0)
);

task automatic send_word(input logic [WIDTH_CONTROL-1:0] control,
                         input logic [WIDTH_COUNTER-1:0] counter,
                         input logic [WIDTH_DATA-1:0] value);
  begin
    @(negedge clk);
    in_control <= control;
    in_counter <= counter;
    in_data <= value;
    in_valid <= 1;
    @(posedge clk);
    assert(in_rdreq == 1) else $fatal(1, "Decoder did not accept input word");
    @(negedge clk);
    in_valid <= 0;
    in_control <= '0;
    in_counter <= '0;
    in_data <= '0;
  end
endtask

initial begin
  reload_initial <= 0;
  in_valid <= 0;
  in_data <= '0;
  in_control <= '0;
  in_counter <= '0;

  wait(reset == 0);
  repeat (4) @(negedge clk);

  send_word(CONTROL_PRNG, WIDTH_COUNTER'(1), '0);
  send_word(CONTROL_BITOR, WIDTH_COUNTER'(1), BITOR_MASK);
  send_word(CONTROL_FINAL_NOMOD, WIDTH_COUNTER'(1), '0);
end

integer out_count = 0;

always @(posedge clk) begin
  #0.1;
  if (!reset && out_wrreq) begin
    unique case (out_count)
      0: begin
        assert(out_control[BIT_PRNG] == 1) else $fatal(1, "First output is not marked PRNG");
        assert(out_data == EXPECTED_FIRST_RANDOM)
          else $fatal(1, "PRNG output mismatch: got %h expected %h", out_data, EXPECTED_FIRST_RANDOM);
      end
      1: begin
        assert(out_control == CONTROL_BITOR) else $fatal(1, "Second output control mismatch");
        assert(out_data == EXPECTED_AFTER_BITOR)
          else $fatal(1, "Relative BITOR used stale state: got %h expected %h", out_data, EXPECTED_AFTER_BITOR);
      end
      2: begin
        assert(out_control == CONTROL_FINAL_NOMOD) else $fatal(1, "Final output control mismatch");
        assert(out_data == EXPECTED_AFTER_BITOR)
          else $fatal(1, "Final no-modify used stale state: got %h expected %h", out_data, EXPECTED_AFTER_BITOR);
      end
      default: $fatal(1, "Too many decoder outputs");
    endcase
    out_count = out_count + 1;
  end
end

always @(posedge clk) begin
  $strobe("t=%8.3f in_valid=%b rdreq=%b out_wr=%b out_ctrl=%h out_data=%h prev=%h curr=%h",
    $realtime, in_valid, in_rdreq, out_wrreq, out_control, out_data,
    dut.prev_value, dut.curr_value);
end

integer fh;

initial begin
  wait(out_count == 3);
  repeat (2) @(posedge clk);
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
`ifndef VERILATOR
  $set_coverage_db_name("run_st_13.ucdb");
`endif
  $finish;
end

initial begin
  #100 $fatal;
end

endmodule: tb_st_13

`default_nettype wire
