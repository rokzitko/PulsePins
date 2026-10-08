// Purpose: additional combined FIFO + preprocessor integration test.
//
// Extends coverage of combined-path corner cases beyond the earlier directed and backpressure
// scenarios.
// Storage of elements, etc.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_input_pre_6;

`include "common_init.sv"

// look around used2=2032

parameter logic [31:0] NR = 'd2100;

initial begin
  wrreq <= 0;
  data <= 96'b0;
  #4
  wrreq <= 1;
  data <= { STORE0, 32'h1234, 32'h04ff }; // store (do not pass)
  #1
  wrreq <= 1;
  data <= { STORE1, 32'h1234, 32'h05ee }; // store (do not pass)
  #1
  wrreq <= 1;
  data <= { STORE2, 32'h1234, 32'h06dd }; // store (do not pass)
  #1
  wrreq <= 1;
  data <= { STORE3, 32'h1234, 32'h07cc }; // store (do not pass)
  #1
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h08ff }; // pass
  #1
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h09ff }; // pass
  #1
  wrreq <= 1;
  data <= { REPLAY, NR, 32'h04 }; // replay 4 elements many (NR) times
  #1
  wrreq <= 1;
  data <= { PASS, 32'h00ff, 32'h01aa }; // store (do not pass)
  #1;
  wrreq <= 1;
  data <= { TERMINATE, 32'd0, 32'd0 };
  #1;
  wrreq <= 0;
end

localparam int EXPECTED_WORDS = 4 * NR + 4;
int received = 0;
bit reader_done = 0;
bit checker_done = 0;
int j;
initial begin
  rdreq <= 0;
  #2060;
  for (j = 0; j < 2000 && received < EXPECTED_WORDS; j++) begin
    rdreq <= 1;
    #10;
    rdreq <= 0;
    #2;
  end
  reader_done = 1;
end

// The FIFO is show-ahead: check the word at every accepted read, including
// the throttled reads above, before the clock edge advances q.
logic [WIDTH_TOTAL-1:0] expected;
always @(posedge clk) begin
  if (!reset && rdreq && !empty) begin
    case (received)
      0: expected = { PASS, 32'h00ff, 32'h08ff };
      1: expected = { PASS, 32'h00ff, 32'h09ff };
      4*NR+2: expected = { PASS, 32'h00ff, 32'h01aa };
      4*NR+3: expected = { TERMINATE, 32'd0, 32'd0 };
      default: begin
        assert(received < 4*NR+2) else $fatal(1, "unexpected extra replay output");
        case ((received-2) % 4)
          0: expected = { STORE0, 32'h1234, 32'h04ff };
          1: expected = { STORE1, 32'h1234, 32'h05ee };
          2: expected = { STORE2, 32'h1234, 32'h06dd };
          3: expected = { STORE3, 32'h1234, 32'h07cc };
        endcase
      end
    endcase
    assert(q === expected) else $fatal(1, "word %0d: got %h expected %h", received, q, expected);
    received = received + 1;
    checker_done = (received == EXPECTED_WORDS);
  end
end

initial begin
   #20;
   $strobe("%h", dut.proc.memory[0]);
   $strobe("%h", dut.proc.memory[1]);
   $strobe("%h", dut.proc.memory[2]);
   $strobe("%h", dut.proc.memory[3]);
   $strobe("%h", dut.proc.memory[4]);
end

integer fh;

initial begin
  wait(reader_done && checker_done);
  repeat (8) @(negedge clk);
  assert(empty && dut.rdempty && !dut.proc.active) else $fatal(1, "replay path did not drain");
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_input_pre_6.ucdb");
  $finish;
end

initial #40000 $fatal(1, "timeout: checked %0d/%0d replay words", received, EXPECTED_WORDS);

endmodule: tb_input_pre_6
