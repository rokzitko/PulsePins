// Canonical RLE scoreboard at the public FIFO output. Small count width forces
// natural run splitting; unrelated clocks/stalls, overflow and pending reset
// are exercised on the same owning DUT. WEIRD_CLOCK additionally tests strobe mode.
`timescale 1ns/1ps
`include "../rl_config.vh"
module tb_encoder_reference;
  logic clk = 0, data_clk = 0;
  always #0.4 clk = ~clk;
  always #0.7 data_clk = ~data_clk;
  logic reset = 1, mode = 1, valid = 0, strobe = 0;
  logic [31:0] data = 0;
  wire [35:0] element;
  wire empty, overflow;
  logic read_enable = 0, ready = 0;
  wire rdreq = read_enable && ready && !empty;
  rl_encoder #(.width_counter(4), .fifo_p(5), .fifo_length(32)) dut(
    .clk, .reset, .mode, .data, .valid, .strobe, .data_clk, .element, .empty, .rdreq, .overflow);
  logic [35:0] expected [0:4095];
  int expected_count = 0, received = 0, run_length = 0, sample_count = 0, decoded_count = 0;
  logic [31:0] run_value;
  bit checking = 0;
  int stalled = 0, split_runs = 0, modes_checked = 0;
  always @(negedge clk) ready = ($urandom_range(0, 3) != 0);
  always @(posedge clk) begin
    if (checking && !empty && !rdreq) stalled++;
    if (checking && rdreq) begin
      assert(received < expected_count) else $fatal(1, "extra encoded run");
      assert(element === expected[received]) else $fatal(1, "encoded run %0d got=%h expected=%h", received, element, expected[received]);
      decoded_count += element[35:32]; received++;
    end
  end
  task automatic append_run;
    if (run_length != 0) begin
      expected[expected_count] = {4'(run_length), run_value}; expected_count++;
      if (run_length == 15) split_runs++;
      run_length = 0;
    end
  endtask
  task automatic clear_all(input bit selected_mode);
    checking = 0; read_enable = 0; valid = 0; strobe = 0; reset = 1; mode = selected_mode;
    repeat (12) @(negedge data_clk);
    reset = 0;
    repeat (12) @(negedge data_clk);
    expected_count = 0; received = 0; run_length = 0; sample_count = 0; decoded_count = 0;
    checking = 1;
    assert(empty && !overflow) else $fatal(1, "encoder reset did not clear pending/overflow state");
  endtask
  task automatic begin_burst;
    // For strobe mode, select the stopped strobe clock while both possible
    // clock sources are low, avoiding a synthetic edge at the clock mux.
    if (!mode) begin @(posedge data_clk); #1ps; valid = 1; end
  endtask
  task automatic sample(input logic [31:0] value);
    if (run_length != 0 && (value != run_value || run_length == 15)) append_run();
    run_value = value; run_length++; sample_count++;
    if (mode) begin
      @(negedge data_clk); data = value; valid = 1;
      @(posedge data_clk); #1ps;
    end else begin
      data = value; #0.3; strobe = 1; #0.3; strobe = 0; #0.4;
    end
  endtask
  task automatic end_burst;
    append_run();
    if (mode) @(negedge data_clk);
    else begin @(posedge data_clk); #1ps; end
    valid = 0;
    repeat (6) @(negedge data_clk);
  endtask
  task automatic drain;
    @(negedge clk); read_enable = 1;
    wait(received == expected_count);
    repeat (20) @(negedge clk);
    assert(empty && !overflow && decoded_count == sample_count) else $fatal(1, "encoder drain/sample count mismatch");
  endtask
  logic [31:0] value;
  integer fh;
  initial begin
    for (int selected = 1; selected >= 0; selected--) begin
`ifndef WEIRD_CLOCK
      if (selected == 0) break;
`endif
      clear_all(selected);
      begin_burst(); repeat (65) sample(32'h80000001); end_burst(); drain();
      for (int burst = 0; burst < 20; burst++) begin
        begin_burst(); value = $urandom;
        for (int n = 0; n < 20; n++) begin
          if ($urandom_range(0, 2) == 0) value = $urandom;
          sample(value);
        end
        end_burst();
      end
      drain();
      // Reset while encoded runs are waiting for downstream acceptance.
      read_enable = 0; begin_burst(); repeat (40) sample(32'hdeadbeef); end_burst();
      assert(!empty) else $fatal(1, "pending-reset scenario was not reached");
      clear_all(selected);
      // Deliberately exceed FIFO capacity. Delivery checking resumes after reset.
      checking = 0; begin_burst();
      for (int n = 0; n < 96; n++) sample(n);
      end_burst();
      assert(overflow && !empty) else $fatal(1, "encoder overflow was not reported");
      clear_all(selected);
      begin_burst(); repeat (3) sample(32'hfedcba98); end_burst(); drain();
      assert(received == 1) else $fatal(1, "old encoded runs survived reset");
      modes_checked++;
    end
`ifdef WEIRD_CLOCK
    assert(modes_checked == 2) else $fatal(1, "both sampling modes were not checked");
`else
    assert(modes_checked == 1) else $fatal(1, "normal sampling mode was not checked");
`endif
    assert(stalled > 0 && split_runs >= 4) else $fatal(1, "missing stalls/count saturation");
    $display("SUCCESS: encoder modes=%0d, natural count splitting, randomized stalls, overflow and reset", modes_checked);
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #30000 $fatal(1, "encoder reference watchdog: %0d/%0d", received, expected_count);
endmodule
