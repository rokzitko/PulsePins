// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

// Streamer interface

`default_nettype none // turn off implicit data types
`include "config.vh"

module st_interface
(
input wire clk,
input wire reset,

// Avalon-ST bus for streaming data
input wire [WIDTH_TOTAL-1:0] asi_data,
input wire asi_valid,
output wire asi_ready,

// Avalon-MM bus for controlling the streamer core
input wire [4:0] avs_s0_address,
input wire avs_s0_read,
input wire avs_s0_write,
output reg [WIDTH_AVS-1:0] avs_s0_readdata,
input wire [WIDTH_AVS-1:0] avs_s0_writedata,

// Clock
input wire streamer_clk,              // clock in the output domain

// Output
output wire [WIDTH_DATA-1:0] qout,   // output
output wire qout_valid,               // valid/enable
output wire qout_strobe,              // strobe (pulses when valid data is available on qout bus)
output wire strobe_enable,            // active when strobe pulses are occuring (can be used to trigger capturing devices for debugging)
output wire done,                     // goes high when streaming has successfully completed
output wire buffer_error,             // goes high if an underflow error occurs

// Trigger signals
input wire [WIDTH_TRIGGER-1:0] trigger_in,
input wire trigger_enable_ext,
input wire trigger_force_ext,
input wire trigger_reset_ext,
output wire trigger_armed,
output wire trigger_activated,

// Gate signal
input wire gate_in
);

logic [WIDTH_TOTAL-1:0] input_data;

swap_endianness_96 sw(.in(asi_data),
                      .out(input_data));

logic reset_streamer;           // clear all FIFOs, reset decoder
logic trigger_enable_int;       // enable trigger
logic trigger_force_int;        // force trigger (trigger even if the trigger condition is not met)
logic trigger_reset_int;        // reset trigger
logic stop;                     // stop streaming
logic qout_select;              // 0 = output from streamer, 1 = output from qout_override
logic [WIDTH_DATA-1:0] initial_value;

logic full_i;                          // input FIFO full
logic [WIDTH_DATA-1:0] qout_streamer; // output from the RL decoder (streamer)
logic [WIDTH_DATA-1:0] qout_override; // signal from the control circuit output

logic gating;      // if true, streaming can be halted in the absence of gate signal
logic gate_in_en;  // if true, gate_in is used as the gating signal
logic [WIDTH_TRIGGER-1:0] gate_mask; // mask for selecting which trigger_in bits are used for gating

logic gate_signal; // the gate signal can be activated either through gate_in or trigger_in signals
assign gate_signal = (gate_in_en ? gate_in : 1'b0) | |(gate_mask & trigger_in);

logic gate_enable; // if true, streaming out can occur
assign gate_enable = (gating ? gate_signal : 1'b1);

logic stop_on_buffer_error; // if true, trigger_activated deasserted if buffer_error goes high

// Statistics
logic [WIDTH_STAT-1:0] input_fifo1_ctr_in;
logic [WIDTH_STAT-1:0] input_fifo1_ctr_out;
logic [WIDTH_STAT-1:0] input_fifo2_ctr_in;
logic [WIDTH_STAT-1:0] input_fifo2_ctr_out;
logic [WIDTH_STAT-1:0] output_fifo_ctr_in;
logic [WIDTH_STAT-1:0] output_fifo_ctr_out;
logic input_fifo_overflow_in;
logic input_fifo_overflow_out;

streamer st0 (
.clk(clk),
.reset(reset | reset_streamer), // system-wide reset or internal forced reset
.input_data(input_data),
.input_valid(asi_valid),
.input_ready(asi_ready),
.streamer_clk(streamer_clk),
.gate_enable(gate_enable),
.initial_value(initial_value),
.qout(qout_streamer),
.qout_valid(qout_valid),
.strobe(qout_strobe),
.strobe_enable(strobe_enable),
.done(done),
.buffer_error(buffer_error),
.trigger_in(trigger_in),
.trigger_enable(trigger_enable_ext | trigger_enable_int),
.trigger_reset(trigger_reset_ext | trigger_reset_int),
.trigger_force(trigger_force_ext | trigger_force_int),
.trigger_armed(trigger_armed),
.trigger_activated(trigger_activated),
.stop,
.stop_on_buffer_error,
.input_fifo1_ctr_in(input_fifo1_ctr_in),
.input_fifo1_ctr_out(input_fifo1_ctr_out),
.input_fifo2_ctr_in(input_fifo2_ctr_in),
.input_fifo2_ctr_out(input_fifo2_ctr_out),
.output_fifo_ctr_in(output_fifo_ctr_in),
.output_fifo_ctr_out(output_fifo_ctr_out),
.input_fifo_overflow_in(input_fifo_overflow_in),
.input_fifo_overflow_out(input_fifo_overflow_out)
);

logic [31:0] data_in;

logic [31:0] crc_out;
logic        crc_valid;

crc32 crc32_inst (
 .clk(streamer_clk),
 .reset(reset | reset_streamer),
 .data_en(qout_valid),
 .data_in(qout),
 .crc_out(crc_out),
 .crc_valid(crc_valid)
);

always_ff @(posedge clk) begin
  if (reset) begin
    qout_select <= 0;
    trigger_reset_int <= 0;
    reset_streamer <= 0;
    trigger_enable_int <= 0;
    trigger_force_int <= 0;
    stop <= 0;
    initial_value <= 0;
    qout_override <= 0;
    gating <= 0;
    gate_in_en <= 0;
    gate_mask <= 0;
  end else if (avs_s0_write) begin
    unique case (st_if_w_t'(avs_s0_address))
      IF_CTRL:       {stop_on_buffer_error, qout_select, trigger_reset_int, reset_streamer, trigger_enable_int, trigger_force_int, stop} <= avs_s0_writedata[5:0];
      INIT_VAL:      initial_value[WIDTH_AVS-1:0] <= avs_s0_writedata;
      QOUT_OVERRIDE: qout_override[WIDTH_AVS-1:0] <= avs_s0_writedata;
      GATING_W:      {gate_mask, gate_in_en, gating} <= avs_s0_writedata[WIDTH_TRIGGER+2-1:0];
    endcase
  end
end

always_ff @(posedge clk) begin
  if (reset) begin
    avs_s0_readdata <= 0;
  end else if (avs_s0_read) begin
    unique case (st_if_r_t'(avs_s0_address))
      IF_STATUS:     avs_s0_readdata <= $bits(avs_s0_readdata)'({ trigger_armed, trigger_activated, done, buffer_error});
      QOUT:          avs_s0_readdata <= qout[WIDTH_AVS-1:0]; // 32 bit
      QOUT_STREAMER: avs_s0_readdata <= qout_streamer[WIDTH_AVS-1:0]; // 32 bit
      EXT_TRIG_IN:   avs_s0_readdata <= $bits(avs_s0_readdata)'(trigger_in);
      EXT_TRIG_CTRL: avs_s0_readdata <= $bits(avs_s0_readdata)'({ trigger_reset_ext, trigger_force_ext, trigger_enable_ext });
      GATING_R:      avs_s0_readdata <= $bits(avs_s0_readdata)'({ gate_enable, gate_signal, gate_in, gate_mask, gate_in_en, gating });
      OVERFLOW:      avs_s0_readdata <= $bits(avs_s0_readdata)'({ input_fifo_overflow_out, input_fifo_overflow_in});
      CRC32:         avs_s0_readdata <= crc_out;
      ST_INF1_IN_L:  avs_s0_readdata <= input_fifo1_ctr_in[31:0];
      ST_INF1_IN_H:  avs_s0_readdata <= input_fifo1_ctr_in[63:32];
      ST_INF1_OUT_L: avs_s0_readdata <= input_fifo1_ctr_out[31:0];
      ST_INF1_OUT_H: avs_s0_readdata <= input_fifo1_ctr_out[63:32];
      ST_INF2_IN_L:  avs_s0_readdata <= input_fifo2_ctr_in[31:0];
      ST_INF2_IN_H:  avs_s0_readdata <= input_fifo2_ctr_in[63:32];
      ST_INF2_OUT_L: avs_s0_readdata <= input_fifo2_ctr_out[31:0];
      ST_INF2_OUT_H: avs_s0_readdata <= input_fifo2_ctr_out[63:32];
      ST_OUTF_IN_L:  avs_s0_readdata <= output_fifo_ctr_in[31:0];
      ST_OUTF_IN_H:  avs_s0_readdata <= output_fifo_ctr_in[63:32];
      ST_OUTF_OUT_L: avs_s0_readdata <= output_fifo_ctr_out[31:0];
      ST_OUTF_OUT_H: avs_s0_readdata <= output_fifo_ctr_out[63:32];
      default:        avs_s0_readdata <= 0;
    endcase
  end
end

assign qout = (qout_select ? qout_override : qout_streamer);

endmodule

`default_nettype wire // turn implicit nets on again to avoid side-effects
