// Purpose: trigger FIFO backpressure integration test.
//
// The first trigger condition is held unmatched until the trigger FIFO is full. The streamer
// must then stop popping trigger elements from the input FIFO until the trigger FIFO has write
// capacity. After releasing the first condition, every trigger element must be consumed before
// activation, proving that no trigger-program elements were dropped while backpressured.

`timescale 1ns/1ps

`include "../config.vh"

`default_nettype none

module tb_st_12;

localparam int TRIGGER_FIFO_LENGTH = 2**P_FIFO_TRIGGER;
localparam int TOTAL_TRIGGERS = TRIGGER_FIFO_LENGTH + 8;

localparam logic [WIDTH_TRIGGER-1:0] MATCH_PATTERN = WIDTH_TRIGGER'(8'h01);
localparam logic [WIDTH_TRIGGER-1:0] MATCH_MASK    = WIDTH_TRIGGER'(8'h01);
localparam logic [WIDTH_DATA-1:0] TRIGGER_DATA =
  (WIDTH_DATA'(MATCH_MASK) << WIDTH_TRIGGER) | WIDTH_DATA'(MATCH_PATTERN);
localparam logic [WIDTH_CONTROL-1:0] CONTROL_TERMINATE = WIDTH_CONTROL'(1 << BIT_TERMINATE);
localparam logic [WIDTH_DATA-1:0] FINAL_VALUE = WIDTH_DATA'(32'h0000005a);

logic clk;
logic reset;

initial clk = 1;
always #0.5 clk = ~clk;

initial begin
  reset <= 1;
  #3 reset <= 0;
end

logic [WIDTH_TOTAL-1:0] input_data;
logic input_valid;
wire input_ready;

logic [WIDTH_TRIGGER-1:0] trigger_in;

wire [WIDTH_DATA-1:0] qout;
wire qout_valid;
wire strobe;
wire strobe_enable;
wire buffer_error;
wire done;
wire trigger_armed;
wire trigger_activated;

streamer dut(
  .clk,
  .reset,
  .input_data,
  .input_valid,
  .input_ready,
  .gate_enable(1'b1),
  .initial_value('0),
  .initial_reload(1'b0),
  .initial_value_streamer('0),
  .streamer_clk(clk),
  .qout,
  .qout_valid,
  .strobe,
  .strobe_enable,
  .buffer_error,
  .done,
  .trigger_in,
  .trigger_enable(1'b1),
  .trigger_force(1'b0),
  .trigger_reset(1'b0),
  .trigger_armed,
  .trigger_activated,
  .stop(1'b0),
  .stop_on_buffer_error(1'b0)
);

function automatic logic [WIDTH_CONTROL-1:0] trigger_control(input int idx);
  logic [WIDTH_CONTROL-1:0] control;
  begin
    control = '0;
    control[BIT_TRIGGER] = 1'b1;
    if (idx == TOTAL_TRIGGERS - 1)
      control[BIT_TRIGGER_FINAL] = 1'b1;
    trigger_control = control;
  end
endfunction

task automatic write_stream_word(input logic [WIDTH_CONTROL-1:0] control,
                                 input logic [WIDTH_COUNTER-1:0] counter,
                                 input logic [WIDTH_DATA-1:0] value);
  begin
    @(negedge clk);
    assert(input_ready == 1) else $fatal;
    input_data <= {control, counter, value};
    input_valid <= 1;
  end
endtask

integer idx;

initial begin
  input_data <= '0;
  input_valid <= 0;
  trigger_in <= '0;

  wait(reset == 0);
  repeat (8) @(negedge clk);

  for (idx = 0; idx < TOTAL_TRIGGERS; idx = idx + 1)
    write_stream_word(trigger_control(idx), '0, TRIGGER_DATA);

  write_stream_word(CONTROL_TERMINATE, WIDTH_COUNTER'(1), FINAL_VALUE);

  @(negedge clk);
  input_valid <= 0;
  input_data <= '0;
end

integer trigger_read_count = 0;
integer activation_read_count = -1;
integer backpressure_seen = 0;

always @(posedge clk) begin
  if (reset) begin
    trigger_read_count <= 0;
    activation_read_count <= -1;
  end else begin
    if (dut.ct0.rdreq)
      trigger_read_count <= trigger_read_count + 1;
    if (trigger_activated && activation_read_count < 0)
      activation_read_count <= trigger_read_count;
  end
end

always @(posedge clk) begin
  if (!reset && (dut.ct0.wrfull || dut.ct0.rdreq || trigger_activated || done)) begin
    $strobe("t=%8.3f full=%b in_chain=%b rdchain=%b ct_wr=%b ct_rd=%b reads=%0d armed=%b act=%b qout=%h done=%b",
      $realtime,
      dut.ct0.wrfull,
      dut.in_valid_chain,
      dut.rdreq_chain_trigger,
      dut.ct0.wrreq,
      dut.ct0.rdreq,
      trigger_read_count,
      trigger_armed,
      trigger_activated,
      qout,
      done
    );
  end
end

initial begin
  wait(reset == 0);
  wait(dut.ct0.wrfull && dut.in_valid_chain);
  repeat (2) @(posedge clk);

  assert(dut.ct0.wrfull == 1) else $fatal;
  assert(dut.in_valid_chain == 1) else $fatal;
  assert(dut.rdreq_chain_trigger == 0) else $fatal;
  assert(dut.ct0.wrreq == 0) else $fatal;
  assert(trigger_read_count == 1) else $fatal;
  assert(trigger_activated == 0) else $fatal;

  backpressure_seen = 1;
  trigger_in <= MATCH_PATTERN;
end

integer fh;

initial begin
  wait(done == 1);
  #1;
  assert(backpressure_seen == 1) else $fatal;
  assert(activation_read_count == TOTAL_TRIGGERS) else $fatal;
  assert(qout == FINAL_VALUE) else $fatal;
  assert(buffer_error == 0) else $fatal;
  $display("SUCCESS");
  fh = $fopen("SUCCESS", "w");
  $fclose(fh);
  $set_coverage_db_name("run_st_12.ucdb");
  $finish;
end

initial begin
  #2000 $fatal;
end

endmodule: tb_st_12

`default_nettype wire
