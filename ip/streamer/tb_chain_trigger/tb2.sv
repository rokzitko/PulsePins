// Public-behavior oracle for asynchronous trigger programs. Internal rdreq is
// used only to place resets at the LOAD boundary, never to derive expected data.
`timescale 1ns/1ps
`include "../config.vh"
`default_nettype none

module tb_chain_async;
  logic clk = 0, wrclk = 0;
`ifdef READ_FASTER
  always #0.7 clk = ~clk;
  initial begin #0.23; forever #1.1 wrclk = ~wrclk; end
`else
  always #1.1 clk = ~clk;
`ifdef COINCIDENT_CLOCKS
  always #0.7 wrclk = ~wrclk;
`else
  initial begin #0.23; forever #0.7 wrclk = ~wrclk; end
`endif
`endif
  logic reset = 1, rst = 1;
  logic [WIDTH_TRIGGER-1:0] i = 0, pattern = 0, mask = 0;
  logic [WIDTH_TRIGGER_CONTROL-1:0] control = 0;
  logic wrreq = 0, trigger_enable = 0, trigger_force = 0, trigger_reset = 0, retrig = 0;
  wire armed, wrfull, o;
  chain_trigger dut(.clk, .wrclk, .reset, .rst, .i, .pattern, .mask, .control,
    .wrreq, .trigger_enable, .trigger_force, .trigger_reset, .retrig, .armed, .wrfull, .o);

  localparam int DEPTH = 1 << P_FIFO_TRIGGER;
  int writes = 0, consumed = 0, blocked = 0;
  int reset_cases = 0, retrig_cases = 0, random_programs = 0;
  bit saw_full = 0, saw_empty = 0;
  logic [3:0] trigger_reset_states = 0, retrig_states = 0, read_reset_states = 0;
  int full_resets = 0, read_edges = 0, write_edges = 0, coincident_edges = 0;
  longint last_write_ps = -1;
  always @(posedge wrclk) begin write_edges++; last_write_ps = longint'($realtime*1000.0); end
  always @(posedge clk) begin
    longint edge_ps;
    edge_ps = longint'($realtime*1000.0); read_edges++;
    if (!reset) begin
      if (rst) read_reset_states[dut.state] = 1;
      else if (retrig) retrig_states[dut.state] = 1;
      else if (trigger_reset) trigger_reset_states[dut.state] = 1;
    end
    #1ps;
    if (edge_ps == last_write_ps) coincident_edges++;
  end

  task automatic tick;
    @(posedge clk);
    #1ps;
    assert(!$isunknown({armed, o})) else $fatal(1, "unknown trigger status");
    assert(!(armed && o)) else $fatal(1, "trigger is both armed and active");
  endtask

  task automatic clear_all;
    bit was_full;
    @(negedge clk);
    was_full = wrfull;
    reset = 1; rst = 1;
    trigger_enable = 0; trigger_force = 0; trigger_reset = 0; retrig = 0; i = 0;
    repeat (8) tick();
    @(negedge wrclk); reset = 0;
    @(negedge clk); rst = 0;
    repeat (8) tick();
    assert(!armed && !o && !wrfull) else $fatal(1, "global reset did not empty/disarm trigger");
    if (was_full) full_resets++;
  endtask

  task automatic put(input logic [7:0] p, m, input bit last);
    @(negedge wrclk);
    while (wrfull) begin
      blocked++;
      @(negedge wrclk);
    end
    pattern = p; mask = m; control = last ? 16'h3 : 16'h1; wrreq = 1;
    @(posedge wrclk);
    assert(!wrfull) else $fatal(1, "producer wrote a full trigger queue");
    writes++;
    @(negedge wrclk); wrreq = 0;
  endtask

  task automatic await_armed;
    int cycles;
    cycles = 0;
    while (armed !== 1'b1 && cycles < 100) begin
      tick();
      assert(!o) else $fatal(1, "program triggered before expected stage %0d", consumed);
      cycles++;
    end
    assert(armed && !o) else $fatal(1, "stage %0d did not arm", consumed);
  endtask

  task automatic stage(input logic [7:0] p, m, input bit last);
    logic [7:0] matching, mismatch;
    await_armed();
    matching = p ^ ~m; // all ignored bits intentionally disagree
    mismatch = matching ^ (m & (8'b0 - m));
    @(negedge clk); trigger_enable = 0; i = matching;
    repeat (3) begin
      tick();
      assert(armed && !o) else $fatal(1, "disabled stage advanced");
    end
    if (m != 0) begin
      @(negedge clk); trigger_enable = 1; i = mismatch;
      repeat (3) begin
        tick();
        assert(armed && !o) else $fatal(1, "mismatch advanced stage: pattern=%h mask=%h input=%h", p, m, mismatch);
      end
    end
    @(negedge clk); trigger_enable = 1; i = matching;
    tick();
    assert(armed && !o) else $fatal(1, "match bypassed registered trigger latency");
    @(negedge clk); trigger_enable = 0; i = mismatch;
    tick();
    assert(!armed && o === last) else
      $fatal(1, "stage %0d pattern=%h mask=%h: active=%b expected final=%b", consumed, p, m, o, last);
    consumed++;
  endtask

  task automatic keep_stage;
    @(negedge clk); trigger_reset = 1; trigger_force = 1; trigger_enable = 0;
    tick();
    assert(!o && !armed) else $fatal(1, "trigger_reset lost priority over force");
    @(negedge clk); trigger_reset = 0; trigger_force = 0;
    reset_cases++;
    await_armed();
  endtask

  task automatic next_program;
    @(negedge clk); retrig = 1; trigger_force = 1; trigger_enable = 0;
    tick();
    assert(!armed && !o) else $fatal(1, "retrig lost priority over force");
    @(negedge clk); retrig = 0; trigger_force = 0;
    retrig_cases++;
  endtask

  task automatic empty_after_retrig;
    next_program();
    repeat (20) begin
      tick();
      assert(!armed && !o) else $fatal(1, "empty queue replayed an old/extra stage");
    end
    saw_empty = 1;
  endtask

  task automatic latched;
    repeat (4) begin
      @(negedge clk); i = $urandom; trigger_enable = $urandom_range(0, 1);
      tick();
      assert(o && !armed) else $fatal(1, "trigger did not remain latched");
    end
    @(negedge clk); trigger_enable = 0;
  endtask

  function automatic logic [7:0] sequence_pattern(input int n);
    // Each wrap uses different contents at the same physical FIFO address.
    sequence_pattern = 8'(37*n + 13 + (n / DEPTH)*71);
  endfunction

  logic [7:0] pats [0:7], masks_ref [0:7];
  int count, before_writes, before_consumed;
  integer fh;
  initial begin
    clear_all();
    @(negedge clk); rst = 1; trigger_force = 1;
    tick(); assert(!o && !armed) else $fatal(1, "idle read-reset/force priority");
    @(negedge clk); rst = 0; trigger_force = 0;
    @(negedge clk); retrig = 1; trigger_force = 1;
    tick(); assert(!o && !armed) else $fatal(1, "idle retrigger/force priority");
    @(negedge clk); retrig = 0; trigger_force = 0;
    // Empty queue: force ignores enable/matching, but every reset outranks it.
    @(negedge clk); trigger_reset = 1; trigger_force = 1;
    tick(); assert(!o) else $fatal(1, "idle reset/force priority");
    @(negedge clk); trigger_reset = 0;
    tick(); assert(o) else $fatal(1, "force did not activate an empty, disabled trigger");
    @(negedge clk); trigger_force = 0;
    latched();
    empty_after_retrig();

    // Explicit final records retain subsequent programs; trigger_reset keeps
    // the active record, whereas retrig moves to the next queued record.
    @(negedge clk); rst = 1;
    put(8'h11, 8'hff, 1); put(8'h22, 8'hff, 1); put(8'h44, 8'hff, 1);
    @(negedge clk); rst = 0;
    stage(8'h11, 8'hff, 1); latched();
    keep_stage(); stage(8'h11, 8'hff, 1);
    @(negedge clk); rst = 1; trigger_force = 1;
    tick(); assert(!o && !armed) else $fatal(1, "active read-reset/force priority");
    @(negedge clk); rst = 0; trigger_force = 0;
    reset_cases++;
    stage(8'h22, 8'hff, 1);
    next_program(); stage(8'h44, 8'hff, 1);
    empty_after_retrig();

    // Reset on the LOAD edge must preserve the record being loaded.
    clear_all();
    @(negedge clk); rst = 1;
    put(8'h12, 8'hff, 0); put(8'h34, 8'hff, 1);
    @(negedge clk); rst = 0;
    wait(dut.rdreq === 1'b1);
    keep_stage();
    stage(8'h12, 8'hff, 0);
    await_armed();
    // Reset after a match was sampled must cancel that pending activation.
    @(negedge clk); i = 8'h34; trigger_enable = 1;
    tick();
    keep_stage(); stage(8'h34, 8'hff, 1);
    empty_after_retrig();

    // retrig and read-domain reset must suppress a LOAD-edge FIFO pop.
    for (int which = 0; which < 2; which++) begin
      clear_all();
      @(negedge clk); rst = 1;
      put(8'h55, 8'hff, 0); put(8'haa, 8'hff, 1);
      @(negedge clk); rst = 0;
      wait(dut.rdreq === 1'b1);
      @(negedge clk);
      if (which == 0) retrig = 1; else rst = 1;
      tick(); assert(!o && !armed) else $fatal(1, "LOAD cancellation failed");
      @(negedge clk); retrig = 0; rst = 0;
      await_armed();
      // Discard the now-active first record; the second must still be queued.
      if (which == 0) next_program();
      else begin
        @(negedge clk); rst = 1; trigger_force = 1;
        tick(); assert(!o) else $fatal(1, "read reset lost priority over force");
        @(negedge clk); rst = 0; trigger_force = 0;
        reset_cases++;
      end
      stage(8'haa, 8'hff, 1);
      empty_after_retrig();
    end

    // Every physical trigger input, both polarities, zero/full masks, and
    // implicit final-on-empty behavior are exercised through the queue.
    for (int b = 0; b < 8; b++) begin
      for (int polarity = 0; polarity < 2; polarity++) begin
        put(polarity ? 8'(1 << b) : 8'b0, 8'(1 << b), 0);
        stage(polarity ? 8'(1 << b) : 8'b0, 8'(1 << b), 1);
        empty_after_retrig();
      end
    end
    put(8'ha5, 0, 1); stage(8'ha5, 0, 1); empty_after_retrig();

    // Conditions are levels, not edges: identical successive records and a
    // final zero mask must progress with input and enable continuously held.
    clear_all();
    @(negedge clk); rst = 1;
    put(8'h96, 8'hff, 0); put(8'h96, 8'hff, 0); put(8'ha5, 0, 1);
    @(negedge clk); rst = 0;
    await_armed();
    @(negedge clk); i = 8'h96; trigger_enable = 1;
    for (int edge_idx = 1; edge_idx <= 8; edge_idx++) begin
      tick();
      assert(o === (edge_idx == 8)) else $fatal(1, "held-level chain latency mismatch at edge %0d", edge_idx);
    end
    consumed += 3;
    empty_after_retrig();

    for (int program_idx = 0; program_idx < 64; program_idx++) begin
      @(negedge clk); rst = 1;
      count = $urandom_range(1, 8);
      for (int n = 0; n < count; n++) begin
        pats[n] = $urandom; masks_ref[n] = $urandom;
        put(pats[n], masks_ref[n], n == count-1);
      end
      @(negedge clk); rst = 0;
      for (int n = 0; n < count; n++) stage(pats[n], masks_ref[n], n == count-1);
      empty_after_retrig();
      random_programs++;
    end

    // Full queue cancellation, then three complete queue wraps with concurrent
    // unrelated-clock producer/consumer activity and observable backpressure.
    @(negedge clk); rst = 1;
    for (int n = 0; n < DEPTH; n++) put(sequence_pattern(n), 8'hff, 0);
    repeat (6) @(negedge wrclk);
    assert(wrfull) else $fatal(1, "trigger queue never reached full");
    saw_full = 1;
    clear_all();
    @(negedge clk); rst = 1;
    before_writes = writes; before_consumed = consumed;
    for (int n = 0; n < DEPTH; n++) put(sequence_pattern(n), 8'hff, 0);
    fork
      begin
        for (int n = DEPTH; n < 3*DEPTH; n++) begin
          put(sequence_pattern(n), 8'hff, n == 3*DEPTH-1);
          repeat ($urandom_range(0, 2)) @(negedge wrclk);
        end
      end
      begin
        @(negedge clk); rst = 0;
        for (int n = 0; n < 3*DEPTH; n++) stage(sequence_pattern(n), 8'hff, n == 3*DEPTH-1);
      end
    join
    assert(writes-before_writes == 3*DEPTH && consumed-before_consumed == 3*DEPTH) else
      $fatal(1, "trigger queue delivery count mismatch");
    empty_after_retrig();
    assert(saw_full && saw_empty && blocked > 0 && reset_cases >= 4 && retrig_cases > 0 && random_programs == 64)
      else $fatal(1, "required trigger scenarios were not exercised");
    tb_require_scenario("queue_full", saw_full);
    tb_require_scenario("queue_empty", saw_empty);
    tb_require_scenario("producer_waited_for_space", blocked);
    tb_require_scenario("full_queue_reset", full_resets);
    tb_require_scenario("queue_wrap_stages_checked", consumed-before_consumed, 3*DEPTH);
    tb_require_scenario("random_programs_checked", random_programs, 64);
    for (int state_idx = 0; state_idx < 4; state_idx++) begin
      tb_require_scenario($sformatf("trigger_reset_state_%0d", state_idx), trigger_reset_states[state_idx]);
      tb_require_scenario($sformatf("retrigger_state_%0d", state_idx), retrig_states[state_idx]);
      tb_require_scenario($sformatf("read_reset_state_%0d", state_idx), read_reset_states[state_idx]);
    end
`ifdef READ_FASTER
    tb_require_scenario("reader_clock_faster", read_edges > write_edges);
`else
    tb_require_scenario("writer_clock_faster", write_edges > read_edges);
`endif
`ifdef COINCIDENT_CLOCKS
    tb_require_scenario("coincident_clock_edges", coincident_edges);
`endif
    $display("SUCCESS: %0d checked trigger stages, %0d blocked writes, %0d randomized programs", consumed, blocked, random_programs);
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #200000 $fatal(1, "trigger program watchdog: writes=%0d consumed=%0d", writes, consumed);
  `include "../../testbench/scenario_coverage.svh"
endmodule
