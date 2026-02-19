// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Glue logic

`default_nettype none // turn off implicit data types
`include "config.vh"

module streamer
#(
 parameter int width_data     = `WIDTH_DATA,
 parameter int width_counter  = `WIDTH_COUNTER,
 parameter int width_control  = `WIDTH_CONTROL,
 parameter int width_datactrl = `WIDTH_DATACTRL,
 parameter int width_total    = `WIDTH_TOTAL, // {control, counter, data}
 parameter int width_trigger  = `WIDTH_TRIGGER,
 parameter int widthstat      = 64            // statistics counters bit width
)(
 input wire clk,                            // logic clock
 input wire reset,                          // reset FIFOs and logic

 input wire [width_total-1:0] input_data,   // input data = data+counter+control bits
 input wire input_valid,                    // enable for loading data into FIFO
 output wire input_ready,                   // FIFO ready (i.e., not full)

 input wire gate_enable,                    // enable signal
 input wire [width_data-1:0] initial_value, // initial value for qout

 input wire streamer_clk,                     // clock for data streaming
 output wire [width_data-1:0] qout,         // output stream
 output wire qout_valid,                    // valid/enable for qout
 output wire strobe,                        // strobe signal for output stream (out of phase relative to streamer_clk)
 output wire strobe_enable,                 // (for debugging)
 output wire buffer_error,                  // goes high in case of output buffer underflow
 output wire done,                          // goes high when streaming is complete

 input wire [width_trigger-1:0] trigger_in, // trigger inputs
 input wire trigger_enable,                 // enable trigger
 input wire trigger_force,                  // manual external trigger signal (overrides everything!)
 input wire trigger_reset,                  // manually resets the trigger logic
 output wire trigger_armed,                 // high if waiting for a trigger event
 output wire trigger_activated,             // high if trigger had been activated and we are streaming out

 input wire stop,
 input wire stop_on_buffer_error,

 // Statistics
 output wire [widthstat-1:0] input_fifo1_ctr_in,
 output wire [widthstat-1:0] input_fifo1_ctr_out,
 output wire [widthstat-1:0] input_fifo2_ctr_in,
 output wire [widthstat-1:0] input_fifo2_ctr_out,
 output wire [widthstat-1:0] output_fifo_ctr_in,
 output wire [widthstat-1:0] output_fifo_ctr_out,

 // Overflow detection
 output wire input_fifo_overflow_in,
 output wire input_fifo_overflow_out
);

logic full_i;                // throttling
assign input_ready = ~full_i;

logic [width_total-1:0] q_i; // output from input-FIFO
logic rdreq_i;               // request data from input-FIFO
logic empty_i;               // empty input-FIFO

input_fifo fifo_i (
    .clk(clk),
    .reset(reset),

    // input-side ports
    .data(input_data),
    .wrreq(input_valid),
    .full(full_i),

    // output-side ports
    .q(q_i),
    .rdreq(rdreq_i),
    .empty(empty_i),

    .ctr1_in(input_fifo1_ctr_in),
    .ctr1_out(input_fifo1_ctr_out),
    .ctr2_in(input_fifo2_ctr_in),
    .ctr2_out(input_fifo2_ctr_out),
    .overflow_in(input_fifo_overflow_in),
    .overflow_out(input_fifo_overflow_out)
    );

logic [width_control-1:0] control;
logic [width_counter-1:0] counter;
logic [width_data-1:0]    data;
assign {control, counter, data} = q_i; // note the order: control, counter, data

logic is_regular, is_trigger;
assign is_regular = ~control[`BIT_TRIGGER];
assign is_trigger = control[`BIT_TRIGGER];

logic in_valid_data, in_valid_chain;
assign in_valid_data  = ~empty_i && is_regular; // regular element available in FIFO
assign in_valid_chain = ~empty_i && is_trigger; // trigger element available in FIFO

// Signal to the input FIFO to unload new data can come from either RL decoder or from trigger queue processor.
logic rdreq_rl_decoder;                  // request for a new element from RL decoder
logic rdreq_rl_encoder;
assign rdreq_rl_encoder = in_valid_chain; // the trigger system will fetch the element in a single cycle, thus a wrreq to chain_trigger is also a rdreq to input FIFO
assign rdreq_i = rdreq_rl_decoder | rdreq_rl_encoder; // decoder can throttle the input FIFO, the trigger system can fetch elements in a single cycle

logic [width_control-1:0]  out_control;
logic [width_data-1:0]     out_data;
logic [width_datactrl-1:0] out_q;       // decoder output: (control, data)
logic out_wrreq;                        // write request from decoder
logic out_almost_full;                  // output FIFO nearly full, need to throtle

logic [3:0] in_opmode;
assign in_opmode = control[`BIT_MODE_HI:`BIT_MODE_LO]; // see config.vh for definitions

rl_decoder rl0 (
    .clk(clk),
    .reset(reset),

    // input to decoder
    .in_data(data),
    .in_control(control),
    .in_counter(counter),
    .in_valid(in_valid_data),
    .in_rdreq(rdreq_rl_decoder),
    .in_opmode(in_opmode),

    .initial_data(initial_value),

     // output from decoder
    .out_data(out_data),
    .out_control(out_control),
    .out_wrreq(out_wrreq),
    .out_almost_full(out_almost_full) // backpressure
);

logic streamer_rst; // reset in streamer_clk domain
sync_bit_3stage sb_inst(
 .clk_dest(streamer_clk),
 .async_in(reset),
 .sync_out(streamer_rst)
);

assign out_q = {out_control, out_data};

logic [`P_FIFO_OUT:0] used_o; // used in testbenches (will be optimized away during synthesis)
logic [width_data-1:0] qout_fifo; // output from FIFO
logic retrig_requested;
logic rdreq;

output_fifo fifo0 (
    .wrclk(clk),
    .reset(reset),

    .data(out_q),
    .wrreq(out_wrreq),
    .almost_full(out_almost_full),

    .qout(qout_fifo),
    .qout_valid(qout_valid),
    .rdreq(rdreq),
    .rdclk(streamer_clk),
    .rdrst(streamer_rst),
    .strobe(strobe),
    .strobe_enable(strobe_enable),
    .done(done),
    .buffer_error(buffer_error),
    .retrig_requested(retrig_requested),
    .used(used_o),

    .ctr_in(output_fifo_ctr_in),
    .ctr_out(output_fifo_ctr_out)
    );

assign rdreq = trigger_activated && gate_enable; // streaming out if the trigger is activated & gate enabled

logic trigger_latch;
always_ff @(posedge streamer_clk) begin
  if (streamer_rst) begin
    trigger_latch <= 0;
  end else if (trigger_activated) begin
    trigger_latch <= 1;
  end
end

assign qout = trigger_latch ? qout_fifo : initial_value;

wire trigger_o;

logic retrig;
assign retrig = retrig_requested && trigger_o;

chain_trigger ct0 (
    .wrclk(clk),
    .reset(reset),
    .pattern(data[`WIDTH_TRIGGER-1:0]),
    .mask(data[2*`WIDTH_TRIGGER-1:`WIDTH_TRIGGER]),
    .control(control[`WIDTH_TRIGGER_CONTROL-1:0]),
    .wrreq(in_valid_chain),

    .clk(streamer_clk),
    .rst(streamer_rst),
    .i(trigger_in),
    .trigger_enable(trigger_enable),
    .trigger_force(trigger_force),
    .trigger_reset(trigger_reset),
    .retrig(retrig),
    .armed(trigger_armed),
    .o(trigger_o)
    );

assign trigger_activated = trigger_o && ~done && (stop_on_buffer_error ? ~buffer_error : 1) && ~stop;

endmodule

`default_nettype wire // turn implicit nets on again to avoid side-effects
