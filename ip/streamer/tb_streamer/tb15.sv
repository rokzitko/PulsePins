// Both buffer-error policies: late terminator recovery versus stop-on-error,
// followed by reset and a clean complete playback on the same instances.
`timescale 1ns/1ps
`include "../config.vh"
module tb_buffer_policies;
  logic clk = 0, streamer_clk = 0, reset = 1;
  always #0.5 clk = ~clk;
  initial begin #0.17; forever #0.7 streamer_clk = ~streamer_clk; end
  logic [95:0] input_data = 0;
  logic input_valid = 0, trigger_force = 0;
  wire [1:0] input_ready, qout_valid, buffer_error, done, active, triggered;
  wire [31:0] qout [0:1];
  int samples [0:1];
  for (genvar p = 0; p < 2; p++) begin : policies
    streamer dut(.clk, .reset, .input_data, .input_valid, .input_ready(input_ready[p]),
      .streamer_clk, .gate_enable(1'b1), .initial_value(32'b0), .initial_reload(1'b0),
      .initial_value_streamer(32'b0), .trigger_in(8'b0), .trigger_enable(1'b0),
      .trigger_force, .trigger_reset(1'b0), .stop(1'b0), .stop_on_buffer_error(1'(p)),
      .qout(qout[p]), .qout_valid(qout_valid[p]), .buffer_error(buffer_error[p]),
      .done(done[p]), .stream_active(active[p]), .trigger_activated(triggered[p]));
    always @(posedge streamer_clk) if (qout_valid[p]) begin
      assert(samples[p] < 8 && qout[p] === 32'h12345678) else $fatal(1, "policy %0d output mismatch", p);
      samples[p]++;
    end
  end
  task automatic send(input logic [31:0] ctrl, count, data);
    @(negedge clk); input_data = {ctrl, count, data}; input_valid = 1;
    @(posedge clk); assert(input_ready == 2'b11) else $fatal(1, "input unexpectedly full");
    @(negedge clk); input_valid = 0;
  endtask
  task automatic clear_all;
    @(negedge clk); reset = 1; trigger_force = 0;
    repeat (10) @(negedge streamer_clk);
    samples[0] = 0; samples[1] = 0;
    reset = 0;
    repeat (10) @(negedge streamer_clk);
    assert(buffer_error == 0 && done == 0 && active == 0) else $fatal(1, "policy reset failed");
  endtask
  integer fh;
  initial begin
    samples[0] = 0; samples[1] = 0;
    clear_all(); send(0, 8, 32'h12345678);
    repeat (30) @(negedge clk); trigger_force = 1;
    wait(buffer_error == 2'b11);
    @(negedge streamer_clk);
    assert(samples[0] == 8 && samples[1] == 8 && done == 0 && triggered == 2'b01) else
      $fatal(1, "error policies not distinguished: samples=%0d/%0d done=%b triggered=%b", samples[0], samples[1], done, triggered);
    send(4, 1, 32'habcdef01);
    repeat (40) @(negedge streamer_clk);
    assert(active == 2'b10 && done == 0 && qout[0] === 32'habcdef01 && qout[1] === 32'h12345678) else
      $fatal(1, "late terminator did not obey buffer-error policy");
    clear_all(); send(0, 8, 32'h12345678); send(4, 1, 32'habcdef01);
    repeat (30) @(negedge clk); trigger_force = 1;
    wait(done == 2'b11);
    repeat (8) @(negedge streamer_clk);
    assert(samples[0] == 8 && samples[1] == 8 && buffer_error == 0 && qout_valid == 0 && active == 0) else $fatal(1, "clean recovery failed");
    $display("SUCCESS: both underflow policies, late termination and clean post-reset playback");
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #3000 $fatal(1, "buffer policy watchdog");
endmodule
