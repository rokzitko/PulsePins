// Shared behavioral reference for all three combiners. Configuration uses an
// unrelated clock; after bounded CDC settling, every output cycle is checked.
`timescale 1ns/1ps
module tb_combiner_reference;
`ifdef TRIGGER_COMBINER
  localparam int W = 11;
`else
  localparam int W = 32;
`endif
  logic clk = 0, bus_clk = 0, reset = 1;
  always #1.3 clk = ~clk;
  initial begin #0.17; forever #2.1 bus_clk = ~bus_clk; end
  logic [W-1:0] in1 = 0, in2 = 0, in3 = 0, in4 = 0;
  wire [W-1:0] o;
  logic [3:0] avs_s0_address = 0;
  logic avs_s0_read = 0, avs_s0_write = 0;
  logic [31:0] avs_s0_writedata = 0;
  wire [31:0] avs_s0_readdata;
`ifdef TRIGGER_COMBINER
  combiner_trig #(.WIDTH(W)) dut(
`elsif COMBINATIONAL_COMBINER
  combiner_comb #(.WIDTH(W)) dut(
`else
  combiner #(.WIDTH(W)) dut(
`endif
    .clock_clk(bus_clk), .reset_reset(reset),
`ifndef COMBINATIONAL_COMBINER
    .clk, .clk_reset(reset),
`endif
    .in1, .in2, .in3, .in4, .o, .avs_s0_address, .avs_s0_read,
    .avs_s0_write, .avs_s0_writedata, .avs_s0_readdata);

  logic [31:0] registers [0:15];
  bit checking = 0, drive = 0;
  int cycles = 0, checked [0:15];
  logic [4:0] forced_seen = 0;
  localparam logic [W-1:0] MAXIMUM = '1;
  localparam logic [W-1:0] SIGN_BIT = W'(1) << (W-1);
  always @(negedge clk) if (drive) begin
    case (cycles % 8)
      0: begin in1 = 0; in2 = 0; in3 = 0; in4 = 0; end
      1: begin in1 = MAXIMUM; in2 = 1; in3 = MAXIMUM; in4 = 1; end
      2: begin in1 = SIGN_BIT; in2 = SIGN_BIT-1; in3 = 1; in4 = MAXIMUM; end
      3: begin in1 = 0; in2 = 1; in3 = SIGN_BIT; in4 = SIGN_BIT; end
      default: begin in1 = $urandom; in2 = $urandom; in3 = $urandom; in4 = $urandom; end
    endcase
    cycles++;
  end

  function automatic logic [W-1:0] reference(input logic [W-1:0] a, b, c, d);
    logic [W-1:0] v [0:3];
    logic [W-1:0] combined;
    int votes;
    begin
      v[0] = a; v[1] = b; v[2] = c; v[3] = d;
      for (int k = 0; k < 4; k++) begin
        if (registers[0][17+k]) v[k] = registers[12+k];
        v[k] = (v[k] ^ registers[4+k]) & registers[8+k];
      end
      combined = 0;
      case (registers[0][3:0])
        0, 1, 2, 3: combined = v[registers[0][1:0]];
        4, 5, 6, 7, 8: begin
          // A per-bit population count is independent of the RTL's Boolean
          // expressions for AND/OR/parity/majority.
          for (int bit_idx = 0; bit_idx < W; bit_idx++) begin
            votes = int'(v[0][bit_idx]) + int'(v[1][bit_idx]) + int'(v[2][bit_idx]) + int'(v[3][bit_idx]);
            case (registers[0][3:0])
              4: combined[bit_idx] = votes == 4;
              5: combined[bit_idx] = votes != 0;
              6: combined[bit_idx] = votes % 2;
              7: combined[bit_idx] = !(votes % 2);
`ifndef TRIGGER_COMBINER
              8: combined[bit_idx] = votes >= 3;
`endif
              default: combined[bit_idx] = 0;
            endcase
          end
        end
`ifndef TRIGGER_COMBINER
        9: combined = (v[0] & 255) | ((v[1] & 255) << 8) | ((v[2] & 255) << 16) | ((v[3] & 255) << 24);
        10: combined = (v[0] & 65535) | (v[1] << 16);
        11: combined = 64'(v[0]) + 64'(v[1]);
        12: combined = 64'(v[0]) + 64'(v[1]) + 64'(v[2]) + 64'(v[3]);
        13: combined = 64'(v[0]) - 64'(v[1]);
`endif
        default: combined = 0;
      endcase
      reference = registers[0][16] ? registers[3] : ((combined ^ registers[1]) & registers[2]);
    end
  endfunction

  logic [W-1:0] previous_result = 0, expected, current_result;
  always @(posedge clk) begin
    current_result = reference(in1, in2, in3, in4);
`ifdef COMBINATIONAL_COMBINER
    expected = current_result;
`else
    expected = previous_result; // input register, then output register
`endif
    previous_result = current_result;
    #1ps;
    if (checking) begin
      assert(o === expected) else $fatal(1, "mode=%0d pipeline output got=%h expected=%h", registers[0][3:0], o, expected);
      checked[registers[0][3:0]]++;
      if (registers[0][16]) forced_seen[0] = 1;
      else if (registers[0][3:0] <= 3 && registers[0][17+registers[0][3:0]])
        forced_seen[1+registers[0][3:0]] = 1;
    end
  end
  task automatic write_reg(input int a, input logic [31:0] value);
    @(negedge bus_clk); avs_s0_address = a; avs_s0_writedata = value; avs_s0_write = 1;
    registers[a] = a == 0 ? value : W'(value);
    @(negedge bus_clk); avs_s0_write = 0;
  endtask
  task automatic read_reg(input int a, input logic [31:0] value);
    @(negedge bus_clk); avs_s0_address = a; avs_s0_read = 1;
    @(posedge bus_clk); #1ps;
    assert(avs_s0_readdata === value) else $fatal(1, "combiner register %0d got=%h expected=%h", a, avs_s0_readdata, value);
    @(negedge bus_clk); avs_s0_read = 0;
  endtask

  logic [31:0] value, cfg;
  integer fh;
  initial begin
    for (int a = 0; a < 16; a++) begin registers[a] = 0; checked[a] = 0; end
    registers[2] = MAXIMUM;
    for (int a = 8; a < 12; a++) registers[a] = MAXIMUM;
    repeat (8) @(negedge bus_clk); reset = 0; drive = 1;
    for (int pass = 0; pass < 2; pass++) begin
      for (int mode = 0; mode < 16; mode++) begin
        checking = 0;
        // Rapid asynchronous register updates exercise the latest-value CDC.
        for (int a = 1; a < 16; a++) begin
          if (pass) value = $urandom;
          else if (a == 2 || (a >= 8 && a <= 11)) value = MAXIMUM;
          else value = 0;
          write_reg(a, value);
        end
        cfg = mode | (pass ? (32'(1 << (mode % 5)) << 16) : 0);
        write_reg(0, cfg);
        for (int a = 0; a < 16; a++) read_reg(a, registers[a]);
        repeat (24) @(negedge clk);
        checking = 1;
        repeat (64) @(negedge clk);
        checking = 0;
      end
    end
    // Each input force must affect a checked selection, rather than merely
    // being programmed while an unrelated mode ignores that input.
    write_reg(1, 0); write_reg(2, MAXIMUM); write_reg(3, W'(32'h13572468));
    for (int k = 0; k < 4; k++) begin
      write_reg(4+k, 0); write_reg(8+k, MAXIMUM); write_reg(12+k, W'(32'h13572468 ^ k));
      write_reg(0, k | (1 << (17+k)));
      repeat (24) @(negedge clk);
      checking = 1; repeat (8) @(negedge clk); checking = 0;
    end
    write_reg(1, MAXIMUM); write_reg(2, 0); write_reg(0, 1 << 16);
    repeat (24) @(negedge clk);
    checking = 1; repeat (8) @(negedge clk); checking = 0;
    // Live-port readback must select the external port rather than the stored
    // override value. Hold inputs stable while their snapshots cross domains.
    @(negedge clk); drive = 0;
    write_reg(0, registers[0] | 32'h1f000000);
    repeat (40) @(negedge bus_clk);
    read_reg(3, o); read_reg(12, in1); read_reg(13, in2); read_reg(14, in3); read_reg(15, in4);
    for (int mode = 0; mode < 16; mode++)
      assert(checked[mode] >= 128) else $fatal(1, "mode %0d was not checked every cycle", mode);
    for (int mode = 0; mode < 16; mode++) tb_require_scenario($sformatf("mode_%0d_output_checks", mode), checked[mode], 128);
    for (int path = 0; path < 5; path++) tb_require_scenario($sformatf("force_path_%0d_checked", path), forced_seen[path]);
    $display("SUCCESS: 16 modes, cycle-varying inputs, arithmetic boundaries, all register/force paths and live readback");
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #100000 $fatal(1, "combiner reference watchdog");
  `include "../../testbench/scenario_coverage.svh"
endmodule
