// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

// Software-visible wrapper around the streamer core.
//
// Responsibilities of this module:
//   - accept encoded sequence elements on Avalon-ST
//   - expose a compact Avalon-MM programming model for trigger/gating/output control
//   - connect runtime trigger/gate inputs to the core
//   - provide visibility into current output state, CRC, overflow, and FIFO counters
//
// This is the main RTL entry point for software and system integration work. The fuller
// subsystem overview lives in `ip/streamer/README.md` and `docs/docs/streamer.md`.

`default_nettype none // turn off implicit data types
`include "config.vh"

module st_interface
(
input wire clk,
input wire reset,

// Avalon-ST bus for streaming data
input wire [WIDTH_TOTAL-1:0] asi_data,
input wire asi_valid,
input wire asi_channel,
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
logic asi_valid_streamer;
logic asi_ready_streamer;
// The generated width adapter uses this source tag; streamer semantics ignore it.
wire unused_asi_channel = asi_channel;

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
logic [WIDTH_DATA-1:0] qout_streamer; // raw streamer output before override
logic [WIDTH_DATA-1:0] qout_override; // signal from the control circuit output

logic gating;      // if true, streaming can be halted in the absence of gate signal
logic gate_in_en;  // if true, gate_in is used as the gating signal
logic [WIDTH_TRIGGER-1:0] gate_mask; // mask for selecting which trigger_in bits are used for gating

logic stop_on_buffer_error; // if true, trigger_activated deasserted if buffer_error goes high

logic reset_or_reset_streamer;
assign reset_or_reset_streamer = reset | reset_streamer;

logic streamer_global_reset;
logic streamer_reset;

sync_bit_3stage streamer_global_reset_sync_inst(
 .clk_dest(streamer_clk),
 .async_in(reset),
 .sync_out(streamer_global_reset)
);

sync_bit_3stage streamer_reset_sync_inst(
 .clk_dest(streamer_clk),
 .async_in(reset_or_reset_streamer),
 .sync_out(streamer_reset)
);

// Static output/gating configuration crosses as a coherent bundle and is committed only
// while streamer_clk logic is idle or held in streamer reset.
localparam int INIT_RELOAD_SEQ_W = 32;
localparam int STATIC_CFG_W = INIT_RELOAD_SEQ_W + 1 + WIDTH_TRIGGER + 1 + 1 + WIDTH_DATA + WIDTH_DATA + 1;
logic [STATIC_CFG_W-1:0] static_cfg_clk;
logic [STATIC_CFG_W-1:0] static_cfg_streamer;
logic static_cfg_update;
logic static_cfg_commit_streamer;
logic [INIT_RELOAD_SEQ_W-1:0] initial_reload_seq_clk;
logic [INIT_RELOAD_SEQ_W-1:0] initial_reload_seq_streamer;
logic [INIT_RELOAD_SEQ_W-1:0] initial_reload_ack_seq_streamer;
logic [INIT_RELOAD_SEQ_W-1:0] initial_reload_ack_seq_clk;
logic initial_reload_ack_update_streamer;
logic initial_reload_ack_valid_clk;

logic qout_select_streamer;
logic [WIDTH_DATA-1:0] qout_override_streamer;
logic [WIDTH_DATA-1:0] initial_value_streamer;
logic gating_streamer;
logic gate_in_en_streamer;
logic [WIDTH_TRIGGER-1:0] gate_mask_streamer;
logic stop_on_buffer_error_streamer;

assign static_cfg_clk = {initial_reload_seq_clk, stop_on_buffer_error, gate_mask, gate_in_en, gating,
                         initial_value, qout_override, qout_select};
assign {initial_reload_seq_streamer, stop_on_buffer_error_streamer, gate_mask_streamer, gate_in_en_streamer,
        gating_streamer, initial_value_streamer, qout_override_streamer,
        qout_select_streamer} = static_cfg_streamer;

logic trigger_enable_int_streamer;
logic trigger_force_int_streamer;
logic trigger_reset_int_streamer;
logic trigger_enable_ext_streamer;
logic trigger_force_ext_streamer;
logic trigger_reset_ext_streamer;
logic stop_streamer;
logic trigger_enable_streamer;
logic trigger_force_streamer;
logic trigger_reset_streamer;
logic [WIDTH_TRIGGER-1:0] trigger_in_streamer;
logic gate_in_streamer;

assign trigger_enable_streamer = trigger_enable_int_streamer | trigger_enable_ext_streamer;
assign trigger_force_streamer  = trigger_force_int_streamer  | trigger_force_ext_streamer;
assign trigger_reset_streamer  = trigger_reset_int_streamer  | trigger_reset_ext_streamer;

logic gate_signal_streamer;
logic gate_enable_streamer;
logic gate_signal;
logic gate_enable;
assign gate_signal_streamer = (gate_in_en_streamer ? gate_in_streamer : 1'b0) |
                              |(gate_mask_streamer & trigger_in_streamer);
assign gate_enable_streamer = (gating_streamer ? gate_signal_streamer : 1'b1);
assign gate_signal = gate_signal_streamer;
assign gate_enable = gate_enable_streamer;

logic trigger_armed_streamer;
logic trigger_activated_streamer;
logic done_streamer;
logic buffer_error_streamer;
logic streamer_idle;

assign streamer_idle = !trigger_armed_streamer && !trigger_activated_streamer;

logic streamer_idle_clk;
wire initial_reload = initial_reload_ack_valid_clk && (initial_reload_ack_seq_clk == initial_reload_seq_clk);
logic initial_reload_pending;
wire block_input_for_initial_reload = initial_reload_pending && streamer_idle_clk;

assign asi_valid_streamer = asi_valid && !block_input_for_initial_reload;
assign asi_ready = asi_ready_streamer && !block_input_for_initial_reload;

cdc_bit_sync sync_streamer_idle_to_clk (
  .dst_clk(clk), .dst_reset(reset),
  .async_in(streamer_idle), .sync_out(streamer_idle_clk)
);

always_ff @(posedge streamer_clk) begin
  if (streamer_global_reset) begin
    initial_reload_ack_seq_streamer <= '0;
    initial_reload_ack_update_streamer <= 1'b0;
  end else begin
    initial_reload_ack_update_streamer <= 1'b0;
    if (static_cfg_commit_streamer && (initial_reload_seq_streamer != initial_reload_ack_seq_streamer)) begin
      initial_reload_ack_seq_streamer <= initial_reload_seq_streamer;
      initial_reload_ack_update_streamer <= 1'b1;
    end
  end
end

assign trigger_armed = trigger_armed_streamer;
assign trigger_activated = trigger_activated_streamer;
assign done = done_streamer;
assign buffer_error = buffer_error_streamer;

// Statistics
logic [WIDTH_STAT-1:0] input_fifo1_ctr_in;
logic [WIDTH_STAT-1:0] input_fifo1_ctr_out;
logic [WIDTH_STAT-1:0] input_fifo2_ctr_in;
logic [WIDTH_STAT-1:0] input_fifo2_ctr_out;
logic [WIDTH_STAT-1:0] output_fifo_ctr_in;
logic [WIDTH_STAT-1:0] output_fifo_ctr_out;
logic [WIDTH_STAT-1:0] output_fifo_ctr_out_streamer;
logic input_fifo_overflow_in;
logic input_fifo_overflow_out;

cdc_bus_update #(
  .WIDTH(STATIC_CFG_W),
  .RESET_VALUE('0)
) static_cfg_cdc_inst (
  .src_clk(clk),
  .src_reset(reset),
  .src_data(static_cfg_clk),
  .src_update(static_cfg_update),
  .src_busy(),
  .dst_clk(streamer_clk),
  .dst_reset(streamer_global_reset),
  .dst_accept(streamer_reset || streamer_idle),
  .dst_data(static_cfg_streamer),
  .dst_valid(static_cfg_commit_streamer),
  .dst_pending()
);

cdc_bus_update #(
  .WIDTH(INIT_RELOAD_SEQ_W),
  .RESET_VALUE('0)
) initial_reload_ack_cdc_inst (
  .src_clk(streamer_clk),
  .src_reset(streamer_global_reset),
  .src_data(initial_reload_ack_seq_streamer),
  .src_update(initial_reload_ack_update_streamer),
  .src_busy(),
  .dst_clk(clk),
  .dst_reset(reset),
  .dst_accept(1'b1),
  .dst_data(initial_reload_ack_seq_clk),
  .dst_valid(initial_reload_ack_valid_clk),
  .dst_pending()
);

cdc_bit_sync sync_trigger_enable_int (
  .dst_clk(streamer_clk), .dst_reset(streamer_reset),
  .async_in(trigger_enable_int), .sync_out(trigger_enable_int_streamer)
);

cdc_bit_sync sync_trigger_force_int (
  .dst_clk(streamer_clk), .dst_reset(streamer_reset),
  .async_in(trigger_force_int), .sync_out(trigger_force_int_streamer)
);

cdc_bit_sync sync_trigger_reset_int (
  .dst_clk(streamer_clk), .dst_reset(streamer_reset),
  .async_in(trigger_reset_int), .sync_out(trigger_reset_int_streamer)
);

cdc_bit_sync sync_trigger_enable_ext (
  .dst_clk(streamer_clk), .dst_reset(streamer_reset),
  .async_in(trigger_enable_ext), .sync_out(trigger_enable_ext_streamer)
);

cdc_bit_sync sync_trigger_force_ext (
  .dst_clk(streamer_clk), .dst_reset(streamer_reset),
  .async_in(trigger_force_ext), .sync_out(trigger_force_ext_streamer)
);

cdc_bit_sync sync_trigger_reset_ext (
  .dst_clk(streamer_clk), .dst_reset(streamer_reset),
  .async_in(trigger_reset_ext), .sync_out(trigger_reset_ext_streamer)
);

cdc_bit_sync sync_stop (
  .dst_clk(streamer_clk), .dst_reset(streamer_reset),
  .async_in(stop), .sync_out(stop_streamer)
);

cdc_bit_sync sync_gate_in (
  .dst_clk(streamer_clk), .dst_reset(streamer_reset),
  .async_in(gate_in), .sync_out(gate_in_streamer)
);

genvar trigger_sync_i;
generate
  for (trigger_sync_i = 0; trigger_sync_i < WIDTH_TRIGGER; trigger_sync_i++) begin : gen_trigger_in_sync
    cdc_bit_sync sync_trigger_in (
      .dst_clk(streamer_clk), .dst_reset(streamer_reset),
      .async_in(trigger_in[trigger_sync_i]), .sync_out(trigger_in_streamer[trigger_sync_i])
    );
  end
endgenerate

streamer st0 (
.clk(clk),
.reset(reset_or_reset_streamer), // system-wide reset or internal forced reset
.input_data(input_data),
.input_valid(asi_valid_streamer),
.input_ready(asi_ready_streamer),
.streamer_clk(streamer_clk),
.gate_enable(gate_enable_streamer),
.initial_value(initial_value),
.initial_reload(initial_reload),
.initial_value_streamer(initial_value_streamer),
.qout(qout_streamer),
.qout_valid(qout_valid),
.strobe(qout_strobe),
.strobe_enable(strobe_enable),
.done(done_streamer),
.buffer_error(buffer_error_streamer),
.trigger_in(trigger_in_streamer),
.trigger_enable(trigger_enable_streamer),
.trigger_reset(trigger_reset_streamer),
.trigger_force(trigger_force_streamer),
.trigger_armed(trigger_armed_streamer),
.trigger_activated(trigger_activated_streamer),
.stop(stop_streamer),
.stop_on_buffer_error(stop_on_buffer_error_streamer),
.input_fifo1_ctr_in(input_fifo1_ctr_in),
.input_fifo1_ctr_out(input_fifo1_ctr_out),
.input_fifo2_ctr_in(input_fifo2_ctr_in),
.input_fifo2_ctr_out(input_fifo2_ctr_out),
.output_fifo_ctr_in(output_fifo_ctr_in),
.output_fifo_ctr_out(output_fifo_ctr_out_streamer),
.input_fifo_overflow_in(input_fifo_overflow_in),
.input_fifo_overflow_out(input_fifo_overflow_out)
);

logic [31:0] data_in;

logic [31:0] crc_out;
logic        crc_valid;

crc32 crc32_inst (
 .clk(streamer_clk),
 .reset(streamer_reset),
 .data_en(qout_valid),
 .data_in(qout),
 .crc_out(crc_out),
 .crc_valid(crc_valid)
);

localparam int GATING_STATUS_W = WIDTH_TRIGGER + 5;

logic [3:0] status_streamer;
logic [3:0] status_clk;
logic [WIDTH_DATA-1:0] qout_clk;
logic [WIDTH_DATA-1:0] qout_streamer_clk;
logic [31:0] crc_out_clk;
logic [WIDTH_TRIGGER-1:0] trigger_in_clk;
logic [2:0] ext_trig_ctrl_clk;
logic [GATING_STATUS_W-1:0] gating_status_streamer;
logic [GATING_STATUS_W-1:0] gating_status_clk;

assign status_streamer = { trigger_armed_streamer, trigger_activated_streamer,
                           done_streamer, buffer_error_streamer };
assign gating_status_streamer = { gate_enable_streamer, gate_signal_streamer,
                                  gate_in_streamer, gate_mask_streamer,
                                  gate_in_en_streamer, gating_streamer };

cdc_snapshot #(.WIDTH(4)) status_snapshot_inst (
  .src_clk(streamer_clk), .src_reset(streamer_global_reset), .src_data(status_streamer),
  .dst_clk(clk), .dst_reset(reset), .dst_data(status_clk), .dst_valid()
);

cdc_snapshot #(.WIDTH(WIDTH_DATA)) qout_snapshot_inst (
  .src_clk(streamer_clk), .src_reset(streamer_global_reset), .src_data(qout),
  .dst_clk(clk), .dst_reset(reset), .dst_data(qout_clk), .dst_valid()
);

cdc_snapshot #(.WIDTH(WIDTH_DATA)) qout_streamer_snapshot_inst (
  .src_clk(streamer_clk), .src_reset(streamer_global_reset), .src_data(qout_streamer),
  .dst_clk(clk), .dst_reset(reset), .dst_data(qout_streamer_clk), .dst_valid()
);

cdc_snapshot #(.WIDTH(32)) crc_snapshot_inst (
  .src_clk(streamer_clk), .src_reset(streamer_global_reset), .src_data(crc_out),
  .dst_clk(clk), .dst_reset(reset), .dst_data(crc_out_clk), .dst_valid()
);

cdc_snapshot #(.WIDTH(WIDTH_STAT)) output_ctr_out_snapshot_inst (
  .src_clk(streamer_clk), .src_reset(streamer_global_reset), .src_data(output_fifo_ctr_out_streamer),
  .dst_clk(clk), .dst_reset(reset), .dst_data(output_fifo_ctr_out), .dst_valid()
);

cdc_snapshot #(.WIDTH(WIDTH_TRIGGER)) trigger_in_snapshot_inst (
  .src_clk(streamer_clk), .src_reset(streamer_global_reset), .src_data(trigger_in_streamer),
  .dst_clk(clk), .dst_reset(reset), .dst_data(trigger_in_clk), .dst_valid()
);

cdc_snapshot #(.WIDTH(3)) ext_trig_ctrl_snapshot_inst (
  .src_clk(streamer_clk), .src_reset(streamer_global_reset),
  .src_data({trigger_reset_ext_streamer, trigger_force_ext_streamer, trigger_enable_ext_streamer}),
  .dst_clk(clk), .dst_reset(reset), .dst_data(ext_trig_ctrl_clk), .dst_valid()
);

cdc_snapshot #(.WIDTH(GATING_STATUS_W)) gating_snapshot_inst (
  .src_clk(streamer_clk), .src_reset(streamer_global_reset), .src_data(gating_status_streamer),
  .dst_clk(clk), .dst_reset(reset), .dst_data(gating_status_clk), .dst_valid()
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
    stop_on_buffer_error <= 0;
    initial_reload_seq_clk <= '0;
    initial_reload_pending <= 0;
    static_cfg_update <= 0;
  end else begin
    static_cfg_update <= avs_s0_write &&
      ((st_if_w_t'(avs_s0_address) == IF_CTRL) ||
       (st_if_w_t'(avs_s0_address) == INIT_VAL) ||
       (st_if_w_t'(avs_s0_address) == QOUT_OVERRIDE) ||
       (st_if_w_t'(avs_s0_address) == GATING_W));

    if (initial_reload) begin
      initial_reload_pending <= 1'b0;
    end

    if (avs_s0_write) begin
      // Control writes intentionally stay in the Avalon/control clock domain; streamer_clk
      // consumers receive synchronized runtime controls or idle/reset-committed config shadows.
      unique case (st_if_w_t'(avs_s0_address))
        IF_CTRL:       {stop_on_buffer_error, qout_select, trigger_reset_int, reset_streamer, trigger_enable_int, trigger_force_int, stop} <= avs_s0_writedata[6:0];
        INIT_VAL: begin
          initial_value[WIDTH_AVS-1:0] <= avs_s0_writedata;
          initial_reload_seq_clk <= initial_reload_seq_clk + 1'b1;
          initial_reload_pending <= 1'b1;
        end
        QOUT_OVERRIDE: qout_override[WIDTH_AVS-1:0] <= avs_s0_writedata;
        GATING_W:      {gate_mask, gate_in_en, gating} <= avs_s0_writedata[WIDTH_TRIGGER+2-1:0];
        default:       ;
      endcase
    end
  end
end

always_ff @(posedge clk) begin
  if (reset) begin
    avs_s0_readdata <= 0;
  end else if (avs_s0_read) begin
    // Readback exposes both direct state (status, outputs, trigger inputs) and transport
    // health information (overflow/CRC/FIFO counters) so software can verify streamer runs.
    unique case (st_if_r_t'(avs_s0_address))
      IF_STATUS:     avs_s0_readdata <= $bits(avs_s0_readdata)'(status_clk);
      QOUT:          avs_s0_readdata <= qout_clk[WIDTH_AVS-1:0]; // 32 bit
      QOUT_STREAMER: avs_s0_readdata <= qout_streamer_clk[WIDTH_AVS-1:0]; // 32 bit
      EXT_TRIG_IN:   avs_s0_readdata <= $bits(avs_s0_readdata)'(trigger_in_clk);
      EXT_TRIG_CTRL: avs_s0_readdata <= $bits(avs_s0_readdata)'(ext_trig_ctrl_clk);
      GATING_R:      avs_s0_readdata <= $bits(avs_s0_readdata)'(gating_status_clk);
      OVERFLOW:      avs_s0_readdata <= $bits(avs_s0_readdata)'({ input_fifo_overflow_out, input_fifo_overflow_in});
      CRC32:         avs_s0_readdata <= crc_out_clk;
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

assign qout = (qout_select_streamer ? qout_override_streamer : qout_streamer);

endmodule

`default_nettype wire // turn implicit nets on again to avoid side-effects
