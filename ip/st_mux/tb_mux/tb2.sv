// Public-handshake conservation/order scoreboard with continuous dual-source
// traffic, randomized stalls/switch requests and live register-count checks.
`timescale 1ns/1ps
module tb_mux_reference;
  logic clk = 0, reset = 1;
  always #0.5 clk = ~clk;
  logic [31:0] asi_data1 = 0, asi_data2 = 0;
  logic asi_valid1 = 0, asi_valid2 = 0, aso_ready = 0;
  wire asi_ready1, asi_ready2, aso_valid, aso_channel;
  wire [31:0] aso_data, avs_s0_readdata;
  logic [2:0] avs_s0_address = 0;
  logic avs_s0_read = 0, avs_s0_write = 0;
  logic [31:0] avs_s0_writedata = 0;
  st_mux_if dut(.clk, .reset, .asi_data1, .asi_data2, .asi_valid1, .asi_valid2,
    .asi_channel1(1'b1), .asi_channel2(1'b0), .asi_ready1, .asi_ready2,
    .aso_data, .aso_valid, .aso_channel, .aso_ready, .avs_s0_address,
    .avs_s0_read, .avs_s0_write, .avs_s0_writedata, .avs_s0_readdata);
  localparam int N = 256;
  int input_count [0:1], output_count [0:1];
  int stalled = 0, switches = 0, register_reads = 0;
  int busy_switches = 0, stalled_switch_requests = 0;
  bit active = 0, was_stalled = 0, previous_channel = 0;
  logic [32:0] held;
  logic [31:0] expected_register;
  function automatic logic [31:0] payload(input int channel, n);
    payload = (32'h9e3779b9 * n) ^ (channel ? 32'hcafef00d : 32'h12345678);
  endfunction
  always @(negedge clk) begin
    asi_valid1 = active && input_count[0] < N;
    asi_valid2 = active && input_count[1] < N;
    asi_data1 = payload(0, input_count[0]); asi_data2 = payload(1, input_count[1]);
  end
  always @(posedge clk) if (!reset) begin
    if (was_stalled) assert(aso_valid && {aso_channel, aso_data} === held) else $fatal(1, "mux changed a stalled transfer");
    was_stalled = aso_valid && !aso_ready;
    held = {aso_channel, aso_data};
    if (was_stalled) stalled++;
    if (aso_channel != previous_channel) switches++;
    if (aso_channel != previous_channel && asi_valid1 && asi_valid2) busy_switches++;
    if (aso_valid && !aso_ready && avs_s0_write && avs_s0_address == 0 && avs_s0_writedata[0] != aso_channel)
      stalled_switch_requests++;
    previous_channel = aso_channel;
    expected_register = 0;
    if (avs_s0_address == 0) expected_register = input_count[0];
    if (avs_s0_address == 2) expected_register = input_count[1];
    assert(!(asi_ready1 && asi_ready2)) else $fatal(1, "both sources accepted simultaneously");
    assert((aso_valid && aso_ready) == ((asi_valid1 && asi_ready1) || (asi_valid2 && asi_ready2))) else $fatal(1, "mux lost/created a handshake");
    if (asi_valid1 && asi_ready1) input_count[0]++;
    if (asi_valid2 && asi_ready2) input_count[1]++;
    if (aso_valid && aso_ready) begin
      assert(aso_channel === (asi_valid2 && asi_ready2)) else $fatal(1, "mux sideband identifies the wrong accepted source");
      assert(aso_data === payload(aso_channel, output_count[aso_channel])) else $fatal(1, "mux payload/order mismatch");
      output_count[aso_channel]++;
    end
    #1ps;
    if (avs_s0_read) begin
      assert(avs_s0_readdata === expected_register) else $fatal(1, "live mux count register mismatch");
      register_reads++;
    end
  end
  task automatic select_channel(input bit channel);
    @(negedge clk); avs_s0_address = 0; avs_s0_write = 1; avs_s0_writedata = channel; avs_s0_read = 0;
    @(negedge clk); avs_s0_write = 0;
  endtask
  integer fh;
  initial begin
    input_count[0] = 0; input_count[1] = 0; output_count[0] = 0; output_count[1] = 0;
    repeat (4) @(negedge clk); reset = 0; active = 1;
    repeat (2) @(negedge clk);
    select_channel(1); // deterministic switch request with both sources stalled
    repeat (4) @(negedge clk); aso_ready = 1;
    for (int n = 0; n < 1000; n++) begin
      @(negedge clk);
      aso_ready = ($urandom_range(0, 3) != 0);
      avs_s0_read = (n % 13 == 0);
      avs_s0_address = avs_s0_read ? $urandom_range(0, 3) : 0;
      avs_s0_write = !avs_s0_read && ($urandom_range(0, 3) == 0);
      avs_s0_writedata = $urandom_range(0, 1);
    end
    @(negedge clk); aso_ready = 1; avs_s0_read = 0; avs_s0_write = 0;
    select_channel(0); while (input_count[0] != N) @(negedge clk);
    select_channel(1); while (input_count[1] != N) @(negedge clk);
    repeat (4) @(negedge clk);
    for (int a = 0; a < 8; a++) begin
      @(negedge clk); avs_s0_address = a; avs_s0_read = 1;
      @(negedge clk); avs_s0_read = 0;
    end
    assert(output_count[0] == N && output_count[1] == N && stalled > 0 && switches > 10 && register_reads > 20) else
      $fatal(1, "incomplete mux coverage/delivery");
    tb_require_scenario("channel0_checked_transfers", output_count[0], N);
    tb_require_scenario("channel1_checked_transfers", output_count[1], N);
    tb_require_scenario("downstream_stalls", stalled);
    tb_require_scenario("switches_with_both_sources_valid", busy_switches);
    tb_require_scenario("switch_requests_while_stalled", stalled_switch_requests);
    tb_require_scenario("counter_readbacks_checked", register_reads, 20);
    $display("SUCCESS: 512 ordered mux transfers, stalls/switches and live count registers");
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #10000 $fatal(1, "mux reference watchdog");
  `include "../../testbench/scenario_coverage.svh"
endmodule
