// Purpose: randomized sanity test for the full `st_interface` wrapper path.
//
// Repeats wrapper-level transactions under fuzzed inputs to catch integration regressions that
// directed wrapper tests might miss.
// Fuzzing test of the whole design
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

st_interface dut (
  .clk,
  .reset,
  .asi_data,
  .asi_valid,
  .asi_ready,
  .trigger_force_ext,
  .trigger_armed,
  .trigger_activated,
  .qout,
  .qout_valid,
  .streamer_clk(clk),
  .strobe_enable,
  .done,
  .buffer_error
);

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

reg [95:0] rnd;
reg [31:0] control;
int i;

// Stimulus
initial begin
    #3;

    // Initialize the BFM
    src_bfm.init();

   for (i = 1; i <= 1000; i++) begin
      //      control   counter   data
      // rnd = { 32'b0, $urandom, $urandom };

      control = $urandom;
      control = control & (32'hF0); // random bit mode

      rnd = { control, $urandom, $urandom };
      $display("%h", rnd);

      src_bfm.set_transaction_data(rnd);
      src_bfm.set_transaction_sop(1);
      src_bfm.set_transaction_eop(1);
      src_bfm.push_transaction();
    end
end

int dly;
// Reset counter periodically
initial begin
  forever begin
     dly = $urandom_range(100, 200);
     #dly;
     if (dut.st0.fifo_i.used2 > 0) begin
       force dut.st0.rl0.curr_cnt = 10;
       #1;
       release dut.st0.rl0.curr_cnt;
     end
  end
end

initial begin
  trigger_force_ext <= 0;
  #1500;
  trigger_force_ext <= 1;
end

integer fh;

initial begin
  #2000;
  wait(dut.st0.fifo_i.used2 == 0);
  #2000;
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_if_3.ucdb");
  $finish;
end

// Terminate forcefully if it gets stuck
initial begin
  #1000000;
  $fatal;
end

endmodule: tb_st_if_3
