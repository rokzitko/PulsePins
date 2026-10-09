// Decoder reference regression: all operations, shifts at 0/31/32, zero-length
// runs, PRNG/relative-operation composition, randomized stalls, reset/reload in flight.
`timescale 1ns/1ps
`include "../config.vh"
module tb_decoder_reference;
  logic clk = 0, reset = 1, reload_initial = 0;
  always #0.5 clk = ~clk;
  logic in_valid = 0, out_almost_full = 0;
  logic [31:0] in_data = 0, in_control = 0, in_counter = 0;
  logic [31:0] initial_data = 32'h12345678;
  wire in_rdreq, out_wrreq;
  wire [31:0] out_data, out_control;
  rl_decoder dut(.clk, .reset, .reload_initial, .in_valid, .in_data, .in_control,
    .in_counter, .in_opmode(in_control[7:4]), .in_rdreq, .out_almost_full,
    .out_data, .out_control, .out_wrreq, .initial_data);
  `include "../decoder_reference.svh"
  int received = 0, stalls = 0, resets_in_flight = 0, zero_runs = 0;
  int observed_modes [0:10];
  int prng_outputs = 0, reset_cancels = 0, reload_cancels = 0, resumed_outputs = 0;
  always @(posedge clk) begin
    if (dut.curr_cnt != 0 && reset) reset_cancels++;
    if (dut.curr_cnt != 0 && reload_initial) reload_cancels++;
  end
  bit random_stalls = 1;
  always @(negedge clk) out_almost_full = random_stalls && ($urandom_range(0, 3) == 0);
  always @(posedge clk) begin
    #1ps;
    if (!reset && !reload_initial) begin
      if (out_almost_full) begin
        assert(!out_wrreq) else $fatal(1, "decoder emitted while stalled");
        if (dut.curr_cnt != 0) stalls++;
      end
      if (out_wrreq) begin
        assert(received < reference_count) else $fatal(1, "unexpected decoder output");
        assert({out_control, out_data} === reference_words[received]) else
          $fatal(1, "decoder word %0d got=%h expected=%h", received, {out_control, out_data}, reference_words[received]);
        if (out_control[7:4] <= 10) observed_modes[out_control[7:4]]++;
        if (out_control[BIT_PRNG]) prng_outputs++;
        if (stalls > 0) resumed_outputs++;
        received++;
      end
    end
  end
  task automatic send(input logic [31:0] ctrl, count, data);
    reference_run(ctrl, count, data);
    @(negedge clk); in_control = ctrl; in_counter = count; in_data = data; in_valid = 1;
    do @(posedge clk); while (!in_rdreq);
    if (count == 0) zero_runs++;
    @(negedge clk); in_valid = 0;
  endtask
  task automatic drain;
    wait(received == reference_count);
    repeat (8) @(negedge clk);
  endtask
  logic [31:0] ctrl, data;
  int count, old_received;
  integer fh;
  initial begin
    for (int op = 0; op <= 10; op++) observed_modes[op] = 0;
    reference_reset(initial_data);
    repeat (5) @(negedge clk); reset = 0;
    for (int op = 0; op <= 10; op++) send(op << 4, 3, 32'h80000001);
    for (int shift = 0; shift <= 32; shift++) begin
      send(0, 1, 32'h80000001); send(9 << 4, 1, shift);
      send(0, 1, 32'h80000001); send(10 << 4, 1, shift);
    end
    for (int n = 0; n < 400; n++) begin
      ctrl = $urandom_range(0, 10) << 4;
      if (n % 11 == 0) ctrl[BIT_PRNG] = 1;
      count = $urandom_range(0, 5); data = $urandom;
      if (ctrl[7:4] >= 9) data = $urandom_range(0, 33);
      send(ctrl, count, data);
    end
    drain();
    assert(reference_modes == '1 && reference_random_samples > 0 && stalls > 0 && zero_runs > 0) else
      $fatal(1, "missing decoder operation/random/stall/zero-run coverage");
    for (int reload = 0; reload < 2; reload++) begin
      old_received = received;
      send(0, 100, 32'hdeadbeef);
      wait(received >= old_received + 5);
      @(negedge clk);
      if (reload) reload_initial = 1; else reset = 1;
      initial_data = 32'haaaa5500;
      reference_reset(initial_data); received = 0;
      repeat (4) @(negedge clk);
      reset = 0; reload_initial = 0;
      send(1 << 4, 2, 32'h0f);
      drain();
      assert(received == 2) else $fatal(1, "cancelled run survived reset/reload");
      resets_in_flight++;
    end
    assert(resets_in_flight == 2) else $fatal(1, "missing in-flight cancellation");
    for (int op = 0; op <= 10; op++) tb_require_scenario($sformatf("output_mode_%0d", op), observed_modes[op]);
    tb_require_scenario("prng_outputs_checked", prng_outputs);
    tb_require_scenario("zero_length_accepts", zero_runs);
    tb_require_scenario("stalled_active_run", stalls);
    tb_require_scenario("post_stall_outputs", resumed_outputs);
    tb_require_scenario("reset_active_run", reset_cancels);
    tb_require_scenario("reload_active_run", reload_cancels);
    $display("SUCCESS: all decoder operations, PRNG reference, zero runs, stalls and reset/reload in flight");
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #30000 $fatal(1, "decoder reference watchdog: %0d/%0d", received, reference_count);
  `include "../../testbench/scenario_coverage.svh"
endmodule
