// Purpose: independent reference checking of legal randomized streamer programs.
//
// Bounded run lengths replace internal counter forcing. Store/replay and every
// decoder operation are expanded offline, then checked under asynchronous gating.
// Rok Zitko, 2025

`timescale 1ns/1ps   // 1ns time unit, 1ps resolution

`include "../config.vh"

`default_nettype none

module tb_st_if_3;

reg clk;
reg reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3;
  reset <= 0;
end

// DUT ports
wire [95:0] asi_data;
wire        asi_valid;
wire        asi_ready;
reg         trigger_force_ext;
wire        trigger_armed;
wire        trigger_activated;
wire [31:0] qout;
wire        qout_valid;
wire        strobe_enable;
wire done;
wire buffer_error;
logic streamer_clk = 0;
always #0.7 streamer_clk = ~streamer_clk;
logic [4:0] avs_s0_address = 0;
logic avs_s0_write = 0;
logic [31:0] avs_s0_writedata = 0;
logic gate_in = 1;
bit random_gate = 0;
int gated_cycles = 0;
always @(negedge streamer_clk) if (random_gate) begin
  gate_in = ($urandom_range(0, 3) != 0);
  if (!gate_in) gated_cycles++;
end

st_interface dut (
  .clk,
  .reset,
  .asi_data,
  .asi_valid,
  .asi_channel(1'b0),
  .asi_ready,
  .trigger_force_ext,
  .trigger_armed,
  .trigger_activated,
  .qout,
  .qout_valid,
  .streamer_clk,
  .strobe_enable,
  .done,
  .buffer_error,
  .avs_s0_address, .avs_s0_read(1'b0), .avs_s0_write, .avs_s0_writedata,
  .trigger_in('0), .trigger_enable_ext(1'b0), .trigger_reset_ext(1'b0),
  .gate_in
);

`include "check_inputs.svh"

// Instantiate Avalon-ST Source BFM
avalon_st_source_bfm #(
  .AVALON_ST_DATA_WIDTH(96)
) src_bfm (
  .clk,
  .reset,
  .src_data(asi_data),
  .src_valid(asi_valid),
  .src_ready(asi_ready)
);

initial begin
  forever begin
    #1000;
    $strobe("t=%8.3f fifo_i=%d c=%d qout=%h qout_valid=%b st_en=%b done=%b buffer_error=%b trig_act=%b",
      $realtime, dut.st0.fifo_i.used2, dut.st0.rl0.curr_cnt, qout, qout_valid, strobe_enable,
      done, buffer_error, trigger_activated
    );
  end
end

`include "../decoder_reference.svh"
logic [95:0] stored [0:7];
int received = 0, store_commands = 0, replay_commands = 0, discard_commands = 0;
bit producer_done = 0;
function automatic logic [31:0] wire_word(input logic [31:0] word);
  wire_word = {word[7:0], word[15:8], word[23:16], word[31:24]};
endfunction
task automatic send(input logic [31:0] ctrl, count, data);
  src_bfm.set_transaction_data({wire_word(ctrl), wire_word(count), wire_word(data)});
  src_bfm.set_transaction_sop(1);
  src_bfm.set_transaction_eop(1);
  src_bfm.push_transaction();
endtask
task automatic put_store(input int position);
  logic [31:0] ctrl, count, data;
  ctrl = (1 << BIT_NOPASS) | (1 << BIT_STORE) | (position << BIT_POSITIONS_LO) | ($urandom_range(0, 10) << 4);
  count = $urandom_range(1, 3); data = $urandom;
  stored[position] = {ctrl, count, data};
  send(ctrl, count, data); store_commands++;
endtask
always @(posedge streamer_clk) if (qout_valid) begin
  assert(received < reference_count) else $fatal(1, "extra wrapper sample");
  assert(qout === reference_words[received][31:0]) else
    $fatal(1, "wrapper sample %0d got=%h expected=%h", received, qout, reference_words[received][31:0]);
  received++;
end

initial begin
  logic [31:0] ctrl, count, data;
  int length, repetitions;
  trigger_force_ext <= 0;
  reference_reset(0);
  #5;
  src_bfm.init();
  @(negedge clk); avs_s0_address = GATING_W; avs_s0_writedata = 3; avs_s0_write = 1;
  @(negedge clk); avs_s0_write = 0;
  repeat (20) @(negedge clk);
  for (int op = 0; op <= 10; op++) begin
    reference_run(op << 4, 2, 32'h80000001);
    send(op << 4, 2, 32'h80000001);
  end
  reference_run(1 << BIT_PRNG, 20, 0); send(1 << BIT_PRNG, 20, 0);
  for (int n = 0; n < 8; n++) put_store(n);
  for (int n = 0; n < 96; n++) begin
    case (n % 4)
      0: put_store($urandom_range(0, 7));
      1: begin
        ctrl = $urandom_range(0, 10) << 4;
        count = $urandom_range(0, 4); data = $urandom;
        reference_run(ctrl, count, data); send(ctrl, count, data);
      end
      2: begin send(1 << BIT_NOPASS, 0, $urandom); discard_commands++; end
      3: begin
        length = $urandom_range(1, 8); repetitions = $urandom_range(1, 3);
        for (int r = 0; r < repetitions; r++)
          for (int p = 0; p < length; p++) reference_run(stored[p][95:64], stored[p][63:32], stored[p][31:0]);
        send((1 << BIT_NOPASS) | (1 << BIT_REPLAY), repetitions, length);
        replay_commands++;
      end
    endcase
  end
  send(4, 1, 32'hcafebabe);
  assert(reference_count < 3500) else $fatal(1, "reference exceeds prefill capacity");
  repeat (reference_count + 64) @(negedge clk);
  producer_done = 1;
  random_gate = 1;
  trigger_force_ext <= 1;
end

integer fh;

initial begin
  wait(producer_done && done);
  repeat (8) @(negedge streamer_clk);
  assert(received == reference_count && qout === 32'hcafebabe && !buffer_error && !qout_valid) else
    $fatal(1, "incomplete wrapper delivery: %0d/%0d", received, reference_count);
  assert(reference_modes == '1 && reference_random_samples >= 20 && gated_cycles > 0 &&
         store_commands > 8 && replay_commands > 0 && discard_commands > 0) else $fatal(1, "missing randomized program scenario");
  $display("SUCCESS: %0d reference-checked wrapper samples, %0d gated cycles", received, gated_cycles);
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_if_3.ucdb");
  $finish;
end

// Terminate forcefully if it gets stuck
initial begin
  #100000;
  $fatal(1, "randomized wrapper watchdog: %0d/%0d", received, reference_count);
end

endmodule: tb_st_if_3
