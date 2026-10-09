// Small counters wrap naturally. Independent fast control clock stresses rapid
// configuration updates; ideal input periods provide independent count bounds.
`timescale 1ns/1ps
module tb_frequency_boundaries;
  logic avs_clk = 0, cnt_clk = 0, avs_reset = 1, cnt_reset = 1;
  realtime avs_half_period = 0.7;
  always #(avs_half_period) avs_clk = ~avs_clk;
  always #5 cnt_clk = ~cnt_clk;
  logic [2:0] sig_in = 0, running = 7;
  initial begin #0.23; forever begin #2; sig_in[0] = running[0] ? ~sig_in[0] : 0; end end
  initial begin #1.13; forever begin #3; sig_in[1] = running[1] ? ~sig_in[1] : 0; end end
  initial begin #0.41; forever begin #4; sig_in[2] = running[2] ? ~sig_in[2] : 0; end end
  logic [3:0] avs_address = 0;
  logic avs_read = 0, avs_write = 0;
  logic [31:0] avs_writedata = 0;
  wire [31:0] avs_readdata;
  wire avs_waitrequest;
  freq_meter_avalon_gray #(.N_CH(3), .COUNTER_W(8)) dut(.*);
  int edges [0:2];
  for (genvar ch = 0; ch < 3; ch++) begin : edge_counts
    always @(posedge sig_in[ch]) if (!cnt_reset) begin
      edges[ch]++;
      #1ps;
      assert(dut.gray_in[ch] === 8'((edges[ch] % 256) ^ ((edges[ch] % 256) >> 1))) else
        $fatal(1, "Gray count incoherent at input wrap: channel=%0d edges=%0d gray=%h", ch, edges[ch], dut.gray_in[ch]);
    end
  end
  task automatic write_reg(input logic [3:0] address, input logic [31:0] value);
    @(negedge avs_clk); avs_address = address; avs_writedata = value; avs_write = 1;
    @(negedge avs_clk); avs_write = 0;
  endtask
  task automatic read_reg(input logic [3:0] address, output logic [31:0] value);
    @(negedge avs_clk); avs_address = address; avs_read = 1;
    @(posedge avs_clk); #1ps; value = avs_readdata;
    @(negedge avs_clk); avs_read = 0;
  endtask
  task automatic check_results(input int gate_length);
    logic [31:0] value;
    int lo, hi, period;
    for (int ch = 0; ch < 3; ch++) begin
      period = 4 + 2*ch;
      lo = running[ch] ? (10*gate_length)/period : 0;
      hi = running[ch] ? (10*gate_length + period-1)/period : 0;
      read_reg(4+ch, value);
      assert(value >= lo && value <= hi) else
        $fatal(1, "frequency gate=%0d channel=%0d got=%0d expected=%0d..%0d", gate_length, ch, value, lo, hi);
    end
  endtask
  task automatic configure(input int requested_gate);
    write_reg(0, 0); repeat (12) @(negedge cnt_clk);
    write_reg(1, requested_gate); repeat (12) @(negedge cnt_clk);
    write_reg(0, 2); repeat (12) @(negedge cnt_clk);
    write_reg(0, 1);
    repeat (4*(requested_gate ? requested_gate : 1) + 40) @(negedge cnt_clk);
  endtask
  logic [31:0] value, held;
  integer fh;
  initial begin
    for (int ch = 0; ch < 3; ch++) edges[ch] = 0;
    repeat (8) @(negedge cnt_clk);
    avs_reset = 0; cnt_reset = 0; running = 3;
    configure(0); check_results(1); read_reg(1, value);
    assert(value == 0) else $fatal(1, "gate register did not retain requested zero");
    configure(1); check_results(1);
    configure(24); check_results(24);
    // Two writes entirely between reference-clock sampling edges must still
    // deliver the latest gate length, rather than cancelling a toggle request.
    @(posedge cnt_clk); #0.1;
    write_reg(1, 7); write_reg(1, 17);
    repeat (120) @(negedge cnt_clk);
    read_reg(1, value); assert(value == 17) else $fatal(1, "rapid write readback mismatch");
    check_results(17);
    write_reg(0, 0); repeat (20) @(negedge cnt_clk); read_reg(4, held);
    running = 0;
    repeat (80) @(negedge cnt_clk); read_reg(4, value);
    assert(value == held) else $fatal(1, "disabled meter changed its published result");
    write_reg(0, 1); repeat (100) @(negedge cnt_clk); check_results(17);
    for (int phase = 0; phase < 4; phase++) begin
      running = phase == 0 ? 1 : phase == 1 ? 2 : phase == 2 ? 4 : 7;
      repeat (120) @(negedge cnt_clk); check_results(17);
    end
    configure(96); check_results(96);
    @(posedge cnt_clk); #0.1;
    write_reg(0, 3); write_reg(0, 3);
    repeat (20) @(negedge cnt_clk);
    for (int ch = 0; ch < 3; ch++) begin
      read_reg(4+ch, value);
      assert(value == 0) else $fatal(1, "rapid clear requests cancelled each other");
    end
    // A one-cycle gate also has to publish fresh results when the bus clock
    // is slower than the reference clock (a raw toggle can alias indefinitely).
    avs_half_period = 10;
    running = 7;
    configure(1); check_results(1);
    running = 0;
    repeat (100) @(negedge cnt_clk); check_results(1);
    assert(edges[0] > 256 && edges[1] > 256 && edges[2] > 256) else $fatal(1, "input counter wrap not exercised");
    $display("SUCCESS: stopped/restarted phases, gate 0/1, rapid writes, disable/re-enable and natural wraps");
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #50000 $fatal(1, "frequency boundary watchdog");
endmodule
