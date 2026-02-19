// SPDX-License-Identifier: MIT
// Copyright (c) 2025,2026 Rok Zitko

// Runtime-length encoder

`default_nettype none
`include "rl_config.vh"

module rl_encoder
#(
 parameter width_data    = cfg::WIDTH_DATA,
 parameter width_counter = cfg::WIDTH_COUNTER,
 parameter fifo_p        = 9,
 parameter fifo_length   = 2**fifo_p
)(
 input  wire clk,   // clock for reading RLE elecments from the FIFO buffer
 input  wire reset, // reset in clk clock domain

 input  wire mode,  // 1 = use data_clk; 0 = use strobe pulses for clocking in data

 input  wire [width_data-1:0] data,
 input  wire                  valid,  // must be asserted for valid data even when strobe is used (mode=0)
 input  wire                  strobe,
 input  wire                  data_clk,

 // Interface
 output wire [width_data+width_counter-1:0] element,
 output wire empty,
 input  wire rdreq,

 output reg overflow  // FIFO buffer overflow error detection
);

  logic input_clk;
  `ifdef WEIRD_CLOCK
     assign input_clk = mode ? data_clk : (valid ? strobe : ~data_clk);
  `else
     assign input_clk = data_clk;
  `endif

  logic reset_iclk; // reset in input_clk clock domain
  sync_bit_3stage sb_inst(
     .clk_dest(input_clk),
     .async_in(reset),
     .sync_out(reset_iclk)
   );

  logic                       have_run;
  logic [width_data-1:0]      run_value;
  logic [width_counter-1:0]   run_count;

  logic                       wrreq;
  logic [width_data+width_counter-1:0] j;

  localparam logic [width_counter-1:0] ONE       = {{(width_counter-1){1'b0}}, 1'b1};
  localparam logic [width_counter-1:0] MAX_COUNT = {width_counter{1'b1}};

  // sample event definition preserved from original meaning:
  // mode=1   -> sample whenever valid=1 (data_clk domain)
  // mode=0   -> sample when strobe & valid (clk domain)
  `ifdef WEIRD_CLOCK
     wire sample_event = mode ? valid : (valid & strobe);
  `else
     wire sample_event = valid;
  `endif

  // valid dropped -> flush pending run
  wire flush_event = (~valid);

  always_ff @(posedge input_clk or posedge reset_iclk) begin
    if (reset_iclk) begin
      have_run  <= 1'b0;
      run_value <= {width_data{1'b0}};
      run_count <= {width_counter{1'b0}};
      j         <= {(width_data+width_counter){1'b0}};
      wrreq     <= 1'b0;
    end else begin
      wrreq <= 1'b0;  // default

      // ------------------------------------------------------------
      // SAMPLE EVENT
      // ------------------------------------------------------------
      if (sample_event) begin
        if (!have_run) begin
          // start first run
          have_run  <= 1'b1;
          run_value <= data;
          run_count <= ONE;   // = 1
        end else if (data == run_value && run_count != MAX_COUNT) begin
          // extend run
          run_count <= run_count + ONE;
        end else begin
          // emit old run due to value change or saturation
          j         <= {run_count, run_value};
          wrreq     <= 1'b1;

          // start new run with current data
          have_run  <= 1'b1;
          run_value <= data;
          run_count <= ONE;
        end

      // ------------------------------------------------------------
      // FLUSH EVENT (valid dropped)
      // ------------------------------------------------------------
      end else if (flush_event && have_run) begin
        j         <= {run_count, run_value};
        wrreq     <= 1'b1;
        have_run  <= 1'b0;
      end
    end
  end

  logic [fifo_p:0] used;
  logic            full;

  dcfifo #(
   .intended_device_family("CycloneV"),
   .lpm_numwords(fifo_length),
   .lpm_width(width_data+width_counter),
   .lpm_widthu(fifo_p+1),
   .add_usedw_msb_bit("ON"),
   .lpm_showahead("ON"),
   .add_ram_output_register("OFF"),
   .overflow_checking("ON"),
   .underflow_checking("ON"),
   .clocks_are_synchronized("FALSE"),
   .wrsync_delaypipe(4),
   .rdsync_delaypipe(4),
   .write_aclr_synch("ON")
  )
  inst1
  (
   .wrclk  (input_clk),
   .rdclk  (clk),
   .data   (j),
   .wrreq  (wrreq),
   .rdreq  (rdreq),
   .aclr   (reset),
   .q      (element),
   .wrfull (full),
   .rdempty(empty),
   .wrusedw(used)
  );

  // Detect overflows (latch). This typically indicates that we are not reading the data from the
  // FIFO buffer fast enough.
  always_ff @(posedge input_clk) begin
    if (reset_iclk) begin
      overflow <= 0;
    end else
      if (full & wrreq) begin
        overflow <= 1;
      end
  end

endmodule

`default_nettype wire
