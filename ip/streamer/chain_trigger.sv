// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Chain trigger

`default_nettype none
`include "config.vh"

module chain_trigger
#(
parameter int width         = `WIDTH_TRIGGER,         // pattern and mask width
parameter int width_control = `WIDTH_TRIGGER_CONTROL,
parameter int fifo_length   = 2**`P_FIFO_TRIGGER)
(
input wire wrclk,                       // clock for writing to the trigger condition queue (FIFO)
input wire reset,                       // total reset (see below)

// input to trigger condition queue
input wire [width-1:0] pattern,         // pattern to be matched
input wire [width-1:0] mask,            // mask (1 for signals to be compared)
input wire [width_control-1:0] control, // bit 1 for the last trigger condition in a sequence ("final element")
input wire wrreq,                       // write the trigger condition to FIFO

// triggering + status signals
input wire clk,                         // clock for reading from the trigger condition FIFO (output domain clock)
input wire [width-1:0] i,               // trigger input signals
input wire trigger_enable,              // trigger logic enabled (trigger events ignored if not high)
input wire trigger_force,               // (internal or external) trigger force (or'ed in st_interface.sv)
input wire trigger_reset,               // trigger reset
output wire armed,                      // asserted when waiting for the trigger condition
output reg o                            // output
);

// reset: clears FIFO, resets and_trigger, state -> IDLE
// trigger_reset: resets and_trigger, state -> IDLE
// (internal) and_trigger_reset: resets the and_trigger (for next trigger event)

logic rdreq; // request for next trigger condition to be read from FIFO
logic [width-1:0] q_pattern; // output from trigger FIFO
logic [width-1:0] q_mask; // output from trigger FIFO
logic [width_control-1:0] q_control; // output from trigger FIFO

logic fifo_empty; // when FIFO empty the trigger state changes to S_IDLE
logic [`P_FIFO_TRIGGER-1:0] used; // [not really used]

dcfifo
#(
 .intended_device_family("CycloneV"),
 .lpm_numwords(fifo_length),
 .lpm_width(2*width+width_control),
 .lpm_widthu(`P_FIFO_TRIGGER),
 .add_ram_output_register("OFF"),
 .overflow_checking("ON"),
 .underflow_checking("ON"),
 .clocks_are_synchronized("FALSE"),
 .wrsync_delaypipe(4),                // 4 FF stages for write->read sync
 .rdsync_delaypipe(4),                // 4 FF stages for read->write sync
 .write_aclr_synch("ON")
)
fifo
(
 .wrclk     (wrclk),
 .rdclk     (clk),
 .data      ({pattern, mask, control}),
 .wrreq     (wrreq),
 .rdreq     (rdreq),
 .aclr      (reset),
 .q         ({q_pattern, q_mask, q_control}),
 .rdempty   (fifo_empty),
 .wrusedw   (used),
 .rdfull    (),
 .wrfull    (),
 .wrempty   (),
 .rdusedw   (),
 .eccstatus ()
);

logic one; // individual trigger output for each trigger event in a sequence
logic and_trigger_reset; // internal reset for and_trigger (controlled by the state machine)

and_trigger at (
  .clk(clk),
  .reset(reset | and_trigger_reset | trigger_reset),
  .i(i),
  .pattern(q_pattern),
  .mask(q_mask),
  .trigger_enable(trigger_enable),
  .o(one)
 );

// Moore state machine
logic [1:0] state;

localparam S_IDLE = 0, S_LOAD = 1, S_WAIT = 2, S_TRIGGERED = 3;

// IDLE = initial state after a reset
// LOAD = load pattern and mask data from the FIFO
// WAIT = pattern and mask defined, trigger is armed, we are waiting for the trigger event
// TRIGGERED = all events were detected (or the trigger has been forced), output is 1

always_ff @(state) begin
  unique case (state)
    S_IDLE:      begin o <= 0; rdreq <= 0; and_trigger_reset <= 1; end
    S_LOAD:      begin o <= 0; rdreq <= 1; and_trigger_reset <= 1; end
    S_WAIT:      begin o <= 0; rdreq <= 0; and_trigger_reset <= 0; end
    S_TRIGGERED: begin o <= 1; rdreq <= 0; and_trigger_reset <= 0; end
  endcase
end

logic is_last;
assign is_last = q_control[`BIT_TRIGGER_FINAL]; // this trigger condition is the last in a chain
assign armed = (state == S_WAIT); // armed = waiting for trigger

// trigger_reset must be asynchronous (necesarry for proper operation of retriggering; cf. test15)
always_ff @(posedge clk or posedge reset or posedge trigger_reset or posedge trigger_force) begin
  if (reset)
    state <= S_IDLE;
  else if (trigger_reset)
    state <= S_IDLE;
  else if (trigger_force) // overrides everything (except reset signals)
    state <= S_TRIGGERED;
  else
    unique case (state)
      S_IDLE:
        if (fifo_empty) // no trigger conditions available in FIFO -> remain idle
          state <= S_IDLE;
        else
          state <= S_LOAD;
      S_LOAD:
        state <= S_WAIT;
      S_WAIT:
        if (!one) // trigger condition not fulfilled yet
          state <= S_WAIT;
        else if (is_last || fifo_empty) // no more trigger conditions (explicitly or implicitly by FIFO being empty)
          state <= S_TRIGGERED;
        else
          state <= S_LOAD; // load next trigger condition from the FIFO
      S_TRIGGERED:
        state <= S_TRIGGERED; // we remain in the triggered condition until the reset
    endcase
end

endmodule
