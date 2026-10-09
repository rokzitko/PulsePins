// Trigger-to-playback integration with unrelated control/output clocks.
// All eight trigger pins, both polarities, internal/external enable and force,
// reset priority, gate-closed activation, retrigger re-arming, and reset-in-flight.
`timescale 1ns/1ps
`include "../config.vh"
`default_nettype none

module tb_st_if_triggers;
  logic clk = 0, streamer_clk = 0;
`ifdef STREAMER_FASTER
  always #1.3 clk = ~clk;
  initial begin #0.17; forever #0.7 streamer_clk = ~streamer_clk; end
`else
  always #1 clk = ~clk;
  initial begin #0.17; forever #1.3 streamer_clk = ~streamer_clk; end
`endif
  logic reset = 1;
  logic [95:0] asi_data = 0;
  logic asi_valid = 0;
  wire asi_ready;
  logic [4:0] avs_s0_address = 0;
  logic avs_s0_read = 0, avs_s0_write = 0;
  logic [31:0] avs_s0_writedata = 0;
  wire [31:0] avs_s0_readdata;
  logic [WIDTH_TRIGGER-1:0] trigger_in = 0;
  logic trigger_enable_ext = 0, trigger_force_ext = 0, trigger_reset_ext = 0, gate_in = 0;
  wire [31:0] qout;
  wire qout_valid, qout_strobe, strobe_enable, done, buffer_error, trigger_armed, trigger_activated;
  st_interface dut(.clk, .reset, .asi_data, .asi_valid, .asi_channel(1'b0), .asi_ready,
    .avs_s0_address, .avs_s0_read, .avs_s0_write, .avs_s0_readdata, .avs_s0_writedata,
    .streamer_clk, .qout, .qout_valid, .qout_strobe, .strobe_enable, .done, .buffer_error,
    .trigger_in, .trigger_enable_ext, .trigger_force_ext, .trigger_reset_ext,
    .trigger_armed, .trigger_activated, .gate_in);

  function automatic logic [31:0] wire_word(input logic [31:0] word);
    wire_word = {word[7:0], word[15:8], word[23:16], word[31:24]};
  endfunction
  task automatic send(input logic [31:0] ctrl, count, data);
    @(negedge clk);
    asi_data = {wire_word(ctrl), wire_word(count), wire_word(data)}; asi_valid = 1;
    do @(posedge clk); while (!asi_ready);
    @(negedge clk); asi_valid = 0;
  endtask
  task automatic mm_write(input logic [4:0] address, input logic [31:0] data);
    @(negedge clk);
    avs_s0_address = address; avs_s0_writedata = data; avs_s0_write = 1;
    @(negedge clk); avs_s0_write = 0;
  endtask
  task automatic mm_read(input logic [4:0] address, output logic [31:0] data);
    @(negedge clk); avs_s0_address = address; avs_s0_read = 1;
    @(posedge clk); #1ps; data = avs_s0_readdata;
    @(negedge clk); avs_s0_read = 0;
  endtask

  logic [31:0] expected [0:31];
  int expected_count = 0, samples = 0;
  bit checking = 0;
  int pin_cases = 0, priority_cases = 0;
  logic [31:0] pin_cases_checked = 0;
  logic [3:0] priorities_checked = 0;
  bit retrigger_checked = 0, reset_pending_checked = 0, reset_active_checked = 0;
  always @(posedge streamer_clk) begin
    if (checking) begin
      assert(!$isunknown({qout_valid, done, buffer_error, trigger_armed, trigger_activated})) else
        $fatal(1, "unknown public trigger/playback status");
      if (qout_valid) begin
        assert(samples < expected_count) else $fatal(1, "unexpected output sample");
        assert(qout === expected[samples]) else $fatal(1, "sample %0d: got %h expected %h", samples, qout, expected[samples]);
        samples++;
      end
    end
  end

  task automatic clear_all;
    @(negedge clk); checking = 0; reset = 1; asi_valid = 0;
    trigger_in = 0; trigger_enable_ext = 0; trigger_force_ext = 0; trigger_reset_ext = 0; gate_in = 0;
    avs_s0_write = 0; avs_s0_read = 0;
    repeat (8) @(negedge streamer_clk);
    @(negedge clk); reset = 0;
    repeat (20) @(negedge streamer_clk);
    samples = 0; expected_count = 0; checking = 1;
    assert(qout === 0 && !done && !qout_valid && !trigger_activated && !trigger_armed) else
      $fatal(1, "reset left stale trigger/output state");
  endtask
  task automatic await_armed;
    int n;
    n = 0;
    while (!trigger_armed && n < 200) begin @(negedge streamer_clk); n++; end
    assert(trigger_armed && !trigger_activated) else $fatal(1, "trigger program did not arm");
  endtask
  task automatic await_active;
    int n;
    n = 0;
    while (!trigger_activated && n < 200) begin @(negedge streamer_clk); n++; end
    assert(trigger_activated) else $fatal(1, "matching/forced trigger did not activate");
  endtask
  task automatic complete(input logic [31:0] final_value);
    int n;
    n = 0;
    while (!done && n < 300) begin @(negedge streamer_clk); n++; end
    assert(done) else $fatal(1, "triggered playback did not complete");
    repeat (8) @(negedge streamer_clk);
    assert(samples == expected_count && !buffer_error && !qout_valid && qout === final_value) else
      $fatal(1, "incorrect completed trigger transaction: samples=%0d/%0d qout=%h", samples, expected_count, qout);
  endtask
  task automatic load_single(input logic [7:0] p, m, input logic [31:0] payload);
    expected_count = 3;
    for (int n = 0; n < 3; n++) expected[n] = payload;
    send(3, 0, {16'b0, m, p});
    send(0, 3, payload);
    send(4, 1, ~payload);
    await_armed();
    repeat (12) @(negedge clk); // prefill output even when playback is the faster clock
  endtask

  logic [31:0] payload, status, ctrl;
  logic [7:0] bit_mask, match_value;
  integer fh;
  initial begin
    for (int b = 0; b < 8; b++) begin
      for (int polarity = 0; polarity < 2; polarity++) begin
        for (int source = 0; source < 2; source++) begin
          clear_all();
          bit_mask = 8'(1 << b);
          match_value = polarity ? bit_mask : 8'b0;
          trigger_in = match_value ^ bit_mask;
          // Commit gating before arming; static configuration is intentionally
          // deferred while a trigger program is armed or playback is active.
          mm_write(GATING_W, 3);
          repeat (20) @(negedge clk);
          payload = 32'h12340000 | (b << 8) | (polarity << 4) | source;
          load_single(match_value, bit_mask, payload);
          trigger_in = match_value;
          repeat (12) @(negedge streamer_clk);
          assert(trigger_armed && !trigger_activated && samples == 0) else $fatal(1, "disabled matching input triggered");
          if (source == 0) mm_write(IF_CTRL, 4); else trigger_enable_ext = 1;
          await_active();
          assert(samples == 0 && qout === 0 && !done) else $fatal(1, "closed gate allowed output");
          // Activation is latched even when enable is subsequently removed.
          if (source == 0) mm_write(IF_CTRL, 0); else trigger_enable_ext = 0;
          repeat (8) @(negedge streamer_clk);
          assert(trigger_activated && samples == 0) else $fatal(1, "enable removal cleared activation");
          gate_in = 1;
          complete(~payload);
          repeat (24) @(negedge clk);
          mm_read(IF_STATUS, status);
          assert(status[3:0] === 4'b0010) else $fatal(1, "trigger completion status readback=%h", status);
          mm_read(EXT_TRIG_IN, status);
          assert(status[7:0] === match_value) else $fatal(1, "trigger pin readback mismatch");
          pin_cases++;
          pin_cases_checked[4*b + 2*polarity + source] = 1;
        end
      end
    end

    // Both force sources must work with enable low, and both reset sources
    // must suppress a held force. Assert reset first to account for CDC latency.
    for (int force_source = 0; force_source < 2; force_source++) begin
      for (int reset_source = 0; reset_source < 2; reset_source++) begin
        clear_all();
        payload = 32'h55660000 | (force_source << 4) | reset_source;
        load_single(8'h80, 8'h80, payload);
        ctrl = 0;
        if (reset_source == 0) begin ctrl = 16; mm_write(IF_CTRL, ctrl); end
        else trigger_reset_ext = 1;
        repeat (12) @(negedge streamer_clk);
        if (force_source == 0) begin ctrl |= 2; mm_write(IF_CTRL, ctrl); end
        else trigger_force_ext = 1;
        repeat (12) @(negedge streamer_clk);
        assert(!trigger_activated && samples == 0 && !done) else $fatal(1, "force defeated held trigger reset");
        if (reset_source == 0) begin ctrl &= ~32'd16; mm_write(IF_CTRL, ctrl); end
        else trigger_reset_ext = 0;
        complete(~payload);
        priority_cases++;
        priorities_checked[2*force_source + reset_source] = 1;
      end
    end

    clear_all();
    expected_count = 5;
    for (int n = 0; n < 5; n++) expected[n] = n < 3 ? 32'h11112222 : 32'h33334444;
    send(3, 0, 32'h0404); send(0, 3, 32'h11112222);
    send(32'h10000, 1, 0);
    send(3, 0, 32'h8080); send(0, 2, 32'h33334444); send(4, 1, 32'habcdef01);
    trigger_enable_ext = 1;
    await_armed();
    trigger_in = 4;
    wait(samples == 3);
    await_armed();
    trigger_reset_ext = 1;
    repeat (12) @(negedge streamer_clk);
    trigger_reset_ext = 0;
    await_armed();
    repeat (12) begin
      @(negedge streamer_clk);
      assert(samples == 3 && !qout_valid && !done && qout === 32'h11112222) else $fatal(1, "retrigger pause/reset failed");
    end
    trigger_in = 8'h80;
    complete(32'habcdef01);
    retrigger_checked = 1;

    clear_all();
    load_single(1, 1, 32'hdeadbeef);
    clear_all(); // discard an armed program with queued payload
    repeat (12) @(negedge streamer_clk);
    assert(!qout_valid && !trigger_armed && !trigger_activated) else $fatal(1, "pending program survived reset");
    reset_pending_checked = 1;
    expected_count = 12;
    for (int n = 0; n < 12; n++) expected[n] = 32'h12345678;
    send(3, 0, 32'h0101); send(0, 12, 32'h12345678); send(4, 1, 0);
    await_armed(); repeat (24) @(negedge clk);
    trigger_enable_ext = 1; trigger_in = 1;
    wait(samples == 1);
    assert(!done) else $fatal(1, "active-reset scenario did not interrupt playback");
    clear_all();
    load_single(2, 2, 32'h98765432);
    trigger_enable_ext = 1; trigger_in = 2;
    complete(~32'h98765432);
    reset_active_checked = 1;

    assert(pin_cases == 32 && priority_cases == 4 && retrigger_checked && reset_pending_checked && reset_active_checked)
      else $fatal(1, "missing trigger integration scenario");
    for (int b = 0; b < 8; b++)
      for (int polarity = 0; polarity < 2; polarity++)
        for (int source = 0; source < 2; source++)
          tb_require_scenario($sformatf("pin_%0d_polarity_%0d_enable_%0d", b, polarity, source), pin_cases_checked[4*b+2*polarity+source]);
    for (int p = 0; p < 4; p++) tb_require_scenario($sformatf("force_reset_priority_%0d", p), priorities_checked[p]);
    tb_require_scenario("retrigger_pause_and_reset", retrigger_checked);
    tb_require_scenario("reset_armed_program", reset_pending_checked);
    tb_require_scenario("reset_active_playback", reset_active_checked);
    $display("SUCCESS: 32 pin/polarity/enable cases, four force/reset priorities, retrigger and in-flight resets");
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #200000 $fatal(1, "trigger integration watchdog: pin=%0d priority=%0d samples=%0d", pin_cases, priority_cases, samples);
  `include "../../testbench/scenario_coverage.svh"
endmodule
