// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

`default_nettype none

// Software-visible wrapper for the integrated counter/measurement subsystem.
//
// Responsibilities of this module:
//   - expose one compact Avalon-MM programming surface for several instruments
//   - select observed input channels through small mux helpers
//   - synchronize latch/reset control into the sampled-data clock domain
//   - multiplex the chosen instrument result back to software
//
// Architectural overview lives in `ip/counter/README.md` and `docs/docs/counter.md`.

module counter_if
#(
 parameter width_data = 32,      // number of inputs
 parameter width_bus  = 32,      // Avalon bus width
 parameter width_addr = 8        // maximal address width for "instruments"
)(
 input wire clk,   // system clock
 input wire reset, // global reset

 input wire [width_data-1:0] d,
 input wire d_valid,
 input wire d_clk,

 // Avalon-MM port for control
 input wire [3:0] avs_s0_address,
 input wire avs_s0_read,
 input wire avs_s0_write,
 output reg [31:0] avs_s0_readdata,
 input wire [31:0] avs_s0_writedata
);

logic reset_all, d_reset; // reset signal synchronized into the data clock domain
cdc_twoff sync_reset (.clk_dst(d_clk), .d_async(reset | reset_all), .q_sync(d_reset));

logic latch_all, d_latch; // latch signal + sync'd version
cdc_twoff sync_latch (.clk_dst(d_clk), .d_async(latch_all), .q_sync(d_latch));

localparam int width_ch = $clog2(width_data); // 32 -> 5
localparam int width_instr = 4;

// These control selections live in the Avalon/control domain and steer readout and
// channel routing rather than representing sampled signal data themselves.
logic [width_instr-1:0] instr;
logic high_low;
logic [width_addr-1:0] addr;

// Register data
logic [width_data-1:0] d_reg;
logic d_valid_reg;

// The full input bus is registered here for simplicity. Only a few selected channels are used
// downstream, so this remains a candidate for later area-focused optimization rather than a
// functional requirement.
always_ff @(posedge d_clk) begin
  if (d_reset) begin
    d_reg <= '0;
    d_valid_reg <= 0;
  end else begin
    d_reg <= d;
    d_valid_reg <= d_valid;
  end
end

// Channel selectors feeding the various instruments. Most blocks observe `sel0`, while
// correlation and timing measurements additionally use `sel1` and `sel2`.
localparam width_sel = $clog2(width_data); // 32 -> 5
logic [width_sel-1:0] sel0;
logic d0;
mux32to1 mux1 (
  .in(d_reg),
  .sel(sel0),
  .out(d0)
);

logic [width_sel-1:0] sel1, sel2;
logic d1, d2;
mux32to2 mux2 (
  .in(d_reg),
  .sel1(sel1),
  .sel2(sel2),
  .out1(d1),
  .out2(d2)
);

logic [width_bus-1:0] result_bc;
logic overflow_bc;

basic_counter bc (
  .clk(clk),
  .d_clk(d_clk),
  .d_reset(d_reset),
  .d(d0),
  .d_valid(d_valid_reg),
  .d_latch(d_latch),
  .high_low(high_low),
  .addr(addr[2:0]),
  .result(result_bc),
  .overflow(overflow_bc)
);

logic [width_bus-1:0] result_rc;

runs_counter rc (
  .clk(clk),
  .d_clk(d_clk),
  .reset(d_reset),
  .d(d0),
  .valid(d_valid_reg),
  .latch(d_latch),
  .high_low(high_low),
  .addr(addr[3:0]),
  .result(result_rc)
);

logic [width_bus-1:0] result_ps;
logic overflow_ps;

packet_stats ps (
  .clk(clk),
  .d_clk(d_clk),
  .reset(d_reset),
  .valid(d_valid_reg),
  .latch(d_latch),
  .high_low(high_low),
  .addr(addr[2:0]),
  .result(result_ps),
  .overflow(overflow_ps)
);

logic d1_prev, d2_prev;

always_ff @(posedge clk) begin
  if (reset) begin
    d1_prev <= 0;
    d2_prev <= 0;
  end else begin
    d1_prev <= d1;
    d2_prev <= d2;
  end
end

logic start_async1, start_async2;
logic stop_async1, stop_async2;

always @(posedge clk) begin
  if (reset) begin
    start_async1 <= 0;
    stop_async1 <= 0;
    start_async2 <= 0;
    stop_async2 <= 0;
  end else begin
    start_async1 <= d1 && !d1_prev;
    stop_async1 <= !d1 && d1_prev;
    start_async2 <= d2 && !d2_prev;
    stop_async2 <= !d2 && d2_prev;
  end
end

logic ready_tc1;
logic ready_tc2;
logic [width_bus-1:0] result_tc1;
logic [width_bus-1:0] result_tc2;

time_counter tc1 (
  .clk(clk), // system clock here!
  .reset(reset | reset_all),
  .start_async(start_async1),
  .stop_async(stop_async1),
  .high_low(high_low),
  .result(result_tc1),
  .ready(ready_tc1)
);

time_counter tc2 (
  .clk(clk), // system clock here!
  .reset(reset | reset_all),
  .start_async(start_async2),
  .stop_async(stop_async2),
  .high_low(high_low),
  .result(result_tc2),
  .ready(ready_tc2)
);

logic [width_bus-1:0] result_ac;

localparam c_len = 3;
localparam c_width = $clog2(c_len+1); // 4 -> 2

autocorrelation #(
  .length(c_len),
  .width_addr(c_width)
) acdeep (
  .clk(clk),
  .d_clk(d_clk),
  .reset(d_reset),
  .d(d0), // d0 here!
  .valid(d_valid_reg),
  .latch(d_latch),
  .high_low(high_low),
  .addr(addr[c_width-1:0]),
  .result(result_ac)
);

logic [width_bus-1:0] result_sc;

seq_counter #(
  .length(4),
  .width_addr(4),
  .rolling(0)
) sc (
  .clk(clk),
  .d_clk(d_clk),
  .reset(d_reset),
  .d(d0),
  .valid(d_valid_reg),
  .latch(d_latch),
  .high_low(high_low),
  .addr(addr[3:0]),
  .result(result_sc)
);

`ifdef COUNTER_CC
logic [width_bus-1:0] result_cc;

crosscorrelation #(
  .length(c_len),
  .width_addr(c_width)
) ccdeep (
  .clk(clk),
  .d_clk(d_clk),
  .reset(d_reset),
  .d1(d1), // d1, d2 here!
  .d2(d2),
  .valid(d_valid_reg),
  .latch(d_latch),
  .high_low(high_low),
  .addr(addr[c_width-1:0]),
  .result(result_cc)
);
`endif

always_ff @(posedge clk) begin
  if (reset) begin
    instr <= 0;
    high_low <= 0;
    addr <= 0;
    sel0 <= 0;
    sel1 <= 0;
    sel2 <= 0;
    latch_all <= 0;
    reset_all <= 0;
  end else if (avs_s0_write) begin
    // The Avalon register model is intentionally small: software selects instrument,
    // result word, and observed channels, then pulses latch/reset as needed.
    case (avs_s0_address)
      3'b001: instr      <= avs_s0_writedata[width_instr-1:0];
      3'b010: high_low   <= avs_s0_writedata[0];
      3'b011: addr       <= avs_s0_writedata[width_addr-1:0];
      3'b100: sel0       <= avs_s0_writedata[width_sel-1:0];
      3'b101: sel1       <= avs_s0_writedata[width_sel-1:0];
      3'b110: sel2       <= avs_s0_writedata[width_sel-1:0];
      3'b111: { latch_all, reset_all } <= avs_s0_writedata[1:0];
    endcase
  end
end

logic [width_bus-1:0] result;

always_comb begin
  // Instruments run in parallel; software sees a muxed readout selected by `instr`.
  unique case (instr)
    4'b0001: result = result_bc;
    4'b0010: result = result_rc;
    4'b0011: result = result_sc;
    4'b0100: result = '0;
    4'b0101: result = result_ps;
    4'b0110: result = result_ac;
`ifdef COUNTER_CC
    4'b0111: result = result_cc;
`endif
    4'b1000: result = result_tc1;
    4'b1001: result = result_tc2;
    default: result = '0;
  endcase
end

always_ff @(posedge clk) begin
  if (reset) begin
    avs_s0_readdata <= 0;
  end else if (avs_s0_read) begin
    // Besides the selected result word, software can directly observe overflow and
    // time-counter ready status through a few fixed readback locations.
    case (avs_s0_address)
      4'b0000: avs_s0_readdata <= result;
      4'b0001: avs_s0_readdata <= overflow_bc;
      4'b0010: avs_s0_readdata <= overflow_ps;
      4'b0011: avs_s0_readdata <= { ready_tc2, ready_tc1 };
      default: avs_s0_readdata <= 0;
    endcase
  end
end

endmodule: counter_if

`default_nettype wire
