// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Chain trigger

`default_nettype none
`include "config.vh"

module chain_trigger
(
input wire wrclk,                       // clock for writing to the trigger condition queue (FIFO)
input wire reset,                       // total reset (see below)

// input to trigger condition queue
input wire [WIDTH_TRIGGER-1:0] pattern,         // pattern to be matched
input wire [WIDTH_TRIGGER-1:0] mask,            // mask (1 for signals to be compared)
input wire [WIDTH_TRIGGER_CONTROL-1:0] control, // bit 1 for the last trigger condition in a sequence ("final element")
input wire wrreq,                       // write the trigger condition to FIFO

// triggering + status signals
input wire clk,                         // clock for reading from the trigger condition FIFO (streamer_clk)
input wire rst,                         // reset signal in streamer_clk clock domain
input wire [WIDTH_TRIGGER-1:0] i,               // trigger input signals
input wire trigger_enable,              // trigger logic enabled (trigger events ignored if not high)
input wire trigger_force,               // (internal or external) trigger force (or'ed in st_interface.sv)
input wire trigger_reset,               // trigger reset
input wire retrig,                      // asserted when retrig element is encountered at the output FIFO buffer out port
output wire armed,                      // asserted when waiting for the trigger condition
output wire wrfull,                     // write-side trigger FIFO full flag
output reg o                            // output
);

// reset: clears FIFO
// rst & trigger_reset: reset and_trigger, state -> IDLE
// trigger_reset preserves the active trigger stage; retrig discards it
// (internal) and_trigger_reset: resets and_trigger (for next trigger event)

logic rdreq; // request for next trigger condition to be read from FIFO
logic [WIDTH_TRIGGER-1:0] q_pattern; // output from trigger FIFO
logic [WIDTH_TRIGGER-1:0] q_mask; // output from trigger FIFO
logic [WIDTH_TRIGGER_CONTROL-1:0] q_control; // output from trigger FIFO

logic fifo_empty; // when FIFO empty the trigger state changes to S_IDLE
logic [P_FIFO_TRIGGER-1:0] used; // [not really used]

parameter int fifo_length = 2**P_FIFO_TRIGGER;

dcfifo
#(
 .intended_device_family("CycloneV"),
 .lpm_numwords(fifo_length),
 .lpm_width(2*WIDTH_TRIGGER+WIDTH_TRIGGER_CONTROL),
 .lpm_widthu(P_FIFO_TRIGGER),
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
 .wrfull    (wrfull),
 .wrempty   (),
 .rdusedw   (),
 .eccstatus ()
);

// ---- Simulation guards ----
// synthesis translate_off
logic wrfull_prev;
always_ff @(posedge wrclk) begin
  if (reset)
    wrfull_prev <= 0;
  else
    wrfull_prev <= wrfull;
end

always_ff @(posedge wrclk) if (!reset) begin
  assert(!(wrreq && wrfull && wrfull_prev))
    else $fatal(1, "Write attempted while trigger FIFO full");
end
// synthesis translate_on

logic one; // individual trigger output for each trigger event in a sequence
logic and_trigger_reset; // internal reset for and_trigger (controlled by the state machine)

and_trigger at (
  .clk(clk),
  .reset(rst | and_trigger_reset | trigger_reset),
  .i(i),
  .pattern(q_pattern),
  .mask(q_mask),
  .trigger_enable(trigger_enable),
  .o(one)
 );

// Moore state machine
logic [1:0] state;
logic active_stage_valid; // q_* contains a trigger stage already popped from the FIFO

localparam S_IDLE = 0, S_LOAD = 1, S_WAIT = 2, S_TRIGGERED = 3;

// IDLE = initial state after a reset
// LOAD = load pattern and mask data from the FIFO
// WAIT = pattern and mask defined, trigger is armed, we are waiting for the trigger event
// TRIGGERED = all events were detected (or the trigger has been forced), output is 1

always_comb begin
  o = 0;
  rdreq = 0;
  and_trigger_reset = 1;
  unique case (state)
    S_IDLE:      begin o = 0; rdreq = 0; and_trigger_reset = 1; end
    S_LOAD:      begin o = 0; rdreq = !(rst || retrig); and_trigger_reset = 1; end
    S_WAIT:      begin o = 0; rdreq = 0; and_trigger_reset = 0; end
    S_TRIGGERED: begin o = 1; rdreq = 0; and_trigger_reset = 0; end
  endcase
end

logic is_last;
assign is_last = q_control[BIT_TRIGGER_FINAL]; // this trigger condition is the last in a chain
assign armed = (state == S_WAIT); // armed = waiting for trigger

always_ff @(posedge clk) begin
  if (rst) begin
    state <= S_IDLE;
    active_stage_valid <= 1'b0;
  end else if (retrig) begin
    state <= S_IDLE;
    active_stage_valid <= 1'b0;
  end else begin
    if (state == S_LOAD)
      active_stage_valid <= 1'b1;

    if (trigger_reset)
      state <= S_IDLE;
    else if (trigger_force) // overrides everything (except reset signals)
      state <= S_TRIGGERED;
    else begin
      unique case (state)
        S_IDLE:
          if (active_stage_valid) // trigger_reset re-arms the already loaded condition
            state <= S_WAIT;
          else if (fifo_empty) // no trigger conditions available in FIFO -> remain idle
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
  end
end

endmodule
