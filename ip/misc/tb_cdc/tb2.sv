// Encoded payloads distinguish torn buses, unsent updates, stale reset epochs
// and lost latest values. Oracle and coverage use only the public interfaces.
`timescale 1ns/1ps
module tb_cdc_coherence;
  logic src_clk = 0, dst_clk = 0, src_reset = 1, dst_reset = 1;
  always #0.7 src_clk = ~src_clk;
  initial begin #0.17; forever #1.1 dst_clk = ~dst_clk; end
  logic [63:0] src_data = 0;
  logic src_update = 0, dst_accept = 0;
  wire [63:0] dst_data, snap_data;
  wire dst_valid, dst_pending, src_busy, snap_valid;
  cdc_bus_update #(.WIDTH(64)) update_dut(.*);
  cdc_snapshot #(.WIDTH(64)) snapshot_dut(.src_clk, .src_reset, .src_data,
    .dst_clk, .dst_reset, .dst_data(snap_data), .dst_valid(snap_valid));
  int epoch = 1, serial = 0, commits = 0, snapshots = 0, pending_cases = 0;
  bit drive = 0, allowed [0:255];
  logic [63:0] held = 0;
  function automatic logic [63:0] encoded(input int e, n);
    logic [31:0] word;
    word = {8'(e), 8'(n), 8'(37*n), ~8'(n)};
    encoded = {word, ~word};
  endfunction
  always @(negedge src_clk) if (drive && !src_reset) begin
    serial = (serial + 1) % 240;
    src_data = encoded(epoch, serial); src_update = serial % 3 != 0;
  end
  always @(posedge src_clk) begin
    if (src_reset) for (int n = 0; n < 256; n++) allowed[n] = 0;
    else if (src_update) allowed[src_data[55:48]] = 1;
  end
  always @(posedge dst_clk) begin
    #1ps;
    if (!dst_reset) begin
      if (!dst_accept) assert(!dst_valid && dst_data === held) else $fatal(1, "CDC committed while acceptance was blocked");
      if (dst_valid) begin
        assert(dst_data === encoded(epoch, dst_data[55:48]) && allowed[dst_data[55:48]]) else $fatal(1, "torn, unsent, or stale-epoch update: %h", dst_data);
        commits++;
      end
      if (snap_valid && snap_data != 0) begin
        assert(snap_data === encoded(epoch, snap_data[55:48])) else $fatal(1, "torn/stale snapshot: %h", snap_data);
        snapshots++;
      end
      held = dst_data;
    end else held = 0;
  end
  integer fh;
  initial begin
    for (int phase = 1; phase <= 3; phase++) begin
      @(negedge src_clk); src_reset = 1; dst_reset = 1; drive = 0; src_update = 0; src_data = 0; epoch = phase;
      repeat (8) @(negedge dst_clk);
      @(negedge src_clk); src_reset = 0; drive = 1;
      repeat (4) @(negedge dst_clk); dst_reset = 0; dst_accept = 0;
      repeat (20) @(negedge dst_clk);
      assert(dst_pending && dst_data == 0) else $fatal(1, "pending CDC scenario missing");
      pending_cases++;
      dst_accept = 1;
      repeat (80) @(negedge src_clk);
      drive = 0;
      @(negedge src_clk); src_data = encoded(epoch, 250); src_update = 1;
      @(negedge src_clk); src_update = 0;
      wait(!src_busy);
      repeat (20) @(negedge dst_clk);
      assert(dst_data === encoded(epoch, 250) && snap_data === encoded(epoch, 250)) else $fatal(1, "latest CDC value did not converge");
      assert(!dst_valid && !dst_pending && !src_busy) else $fatal(1, "update handshake did not return to idle");
      @(negedge src_clk); src_data = encoded(epoch, 251); src_update = 1;
      @(negedge src_clk); src_update = 0;
      assert(src_busy) else $fatal(1, "reset-in-flight scenario missing");
    end
    @(negedge src_clk); src_reset = 1; dst_reset = 1; src_update = 0; drive = 0;
    repeat (8) @(negedge dst_clk);
    assert(dst_data == 0 && snap_data == 0 && !src_busy && !dst_pending && !dst_valid) else $fatal(1, "final in-flight reset did not drain");
    assert(commits > 10 && snapshots > 30 && pending_cases == 3) else $fatal(1, "insufficient CDC coverage");
    $display("SUCCESS: CDC coherence, blocked/latest-value delivery and reset-in-flight epochs");
    fh = $fopen("SUCCESS", "w"); $fclose(fh); $finish;
  end
  initial #20000 $fatal(1, "CDC coherence watchdog");
endmodule
