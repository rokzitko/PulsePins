// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Input FIFO for storing RL encoded data

`default_nettype none // turn off implicit data types
`include "config.vh"

module input_fifo
#(
 parameter int p1 = P_FIFO_IN1,
 parameter int p2 = P_FIFO_IN2
)(
 input wire clk,
 input wire reset,

 // input side
 input wire [WIDTH_TOTAL-1:0] data,
 input wire wrreq,
 output wire full,

 // output side
 output wire [WIDTH_TOTAL-1:0] q,
 input wire rdreq,
 output wire empty,

 output reg [WIDTH_STAT-1:0] ctr1_in,  // FIFO 1, elements clocked in
 output reg [WIDTH_STAT-1:0] ctr1_out, // FIFO 1, elements clocked out
 output reg [WIDTH_STAT-1:0] ctr2_in,  // FIFO 2, elements clocked in
 output reg [WIDTH_STAT-1:0] ctr2_out, // FIFO 2, elements clocked out
 output reg overflow_in,
 output reg overflow_out
);

localparam int length1 = 2**p1;
localparam int length2 = 2**p2;
localparam int almost_shift = 16;

logic rdempty;                 // FIFO 1 empty
logic din_ready;               // preprocessor ready to receive (rdreq to FIFO 1)
logic [WIDTH_TOTAL-1:0] din;         // output from FIFO 1, emitted to preprocessor
logic [WIDTH_TOTAL-1:0] dout;        // output from preprocessor, input to FIFO 2
logic dout_valid;              // valid output from preprocessor

logic [p1-1:0] used1;
logic [p2-1:0] used2;

logic wrreq1;
logic wrreq2;

logic full1;
logic full2;

logic almost_full1;
logic almost_full2;

assign full = almost_full1;
assign wrreq1 = wrreq && ~almost_full1;
assign wrreq2 = dout_valid && ~almost_full2;

// Statistics counters
`define HAS_INPUT_FIFO_COUNTERS
`ifdef HAS_INPUT_FIFO_COUNTERS
  always_ff @(posedge clk) begin
    if (reset) begin
      ctr1_in <= '0;
    end else begin
      if (wrreq1) begin
        ctr1_in <= ctr1_in + 1;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (reset) begin
      ctr1_out <= '0;
    end else begin
      if (din_ready && ~rdempty) begin
        ctr1_out <= ctr1_out + 1;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (reset) begin
      ctr2_in <= '0;
    end else begin
      if (wrreq2) begin
        ctr2_in <= ctr2_in + 1;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (reset) begin
      ctr2_out <= '0;
    end else begin
      if (rdreq && ~empty) begin
        ctr2_out <= ctr2_out + 1;
      end
    end
  end
`endif

scfifo
#(
.intended_device_family("CycloneV"),
.lpm_numwords(length1),
.lpm_width(WIDTH_TOTAL),
.lpm_widthu(p1),
.lpm_showahead("ON"),            // show-ahead mode: data becomes available on output port q before rdreq is asserted
.add_ram_output_register("OFF"), // buffer output from FIFO's RAM block, allowing higher clock frequencies (on Cyclone, only unregistered q output is supported in Show-ahead mode)
.overflow_checking("ON"),        // prevent writes when FIFO is full that could lead to data loss or corruption (automatically disables wrreq signal)
.underflow_checking("ON"),       // prevent read operation from occuring when FIFO is empty (automatically disables rdreq signal)
.almost_full_value(length1-almost_shift)
)
inst1
(
 .clock   (clk),
 .sclr    (reset),

 .data    (data),
 .wrreq   (wrreq1),
 .full    (full1),
 .almost_full (almost_full1),

 .q       (din),
 .rdreq   (din_ready),
 .empty   (rdempty),

 .usedw   (used1)
);

// Filter stage
preprocessor proc (
  .clk(clk),
  .reset(reset),

  .din(din),
  .din_valid(~rdempty),            // valid when fifo_in not empty
  .din_ready(din_ready),

  .dout(dout),
  .dout_valid(dout_valid),
  .dout_ready(~almost_full2)       // ready if fifo_out can accept
);

scfifo
#(
.intended_device_family("CycloneV"),
.lpm_numwords(length2),
.lpm_width(WIDTH_TOTAL),
.lpm_widthu(p2),
.lpm_showahead("ON"),            // show-ahead mode: data becomes available on output port q before rdreq is asserted
.add_ram_output_register("OFF"), // buffer output from FIFO's RAM block, allowing higher clock frequencies (on Cyclone, only unregistered q output is supported in Show-ahead mode)
.overflow_checking("ON"),        // prevent writes when FIFO is full that could lead to data loss or corruption (automatically disables wrreq signal)
.underflow_checking("ON"),       // prevent read operation from occuring when FIFO is empty (automatically disables rdreq signal)
.almost_full_value(length2-almost_shift)
)
inst2
(
 .clock   (clk),
 .sclr    (reset),

 .data    (dout),
 .wrreq   (wrreq2),
 .full    (full2),
 .almost_full (almost_full2),

 .q       (q),
 .rdreq   (rdreq),
 .empty   (empty),

 .usedw   (used2)
);

// Detect overflows (latch)
always_ff @(posedge clk) begin
  if (reset) begin
    overflow_in <= 0;
  end else begin
    if (full1 & wrreq1) begin
      overflow_in <= 1;
    end
  end
end

always_ff @(posedge clk) begin
  if (reset) begin
    overflow_out <= 0;
  end else begin
    if (full2 & wrreq2) begin
      overflow_out <= 1;
    end
  end
end

endmodule

`default_nettype wire // turn implicit nets on again to avoid side-effects
