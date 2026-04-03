// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Output FIFO, strobing of output data, buffer underflow testing

`default_nettype none // turn off implicit data types
`include "config.vh"

module output_fifo
(
input wire wrclk,              // clock for writing data to FIFO
input wire rdclk,              // clock for reading data from FIFO
input wire reset,              // reset FIFO and buffer underflow logic
input wire rdrst,              // reset in the rdclk clock domain

input wire [WIDTH_DATACTRL-1:0] data,   // input data + control register
input wire wrreq,              // if 1, data is written to FIFO each time wrclk is asserted
input wire rdreq,              // if 1, data is read from FIFO each time rdclk is asserted

output reg [WIDTH_DATA-1:0] qout,  // output data
output reg qout_valid,         // valid/enable signal
output reg strobe,             // clock/strobe for output data (inverted rdclk)
output reg strobe_enable,      // Goes high when all conditions for streaming are fulfilled. Signal is exported for debugging purposes.
output reg almost_full,        // 1 if the buffer is too full, used for input data throttling
output reg done,               // goes high when done
output reg buffer_error,       // goes high if read is attempted from an empty buffer
output reg retrig_requested,   // goes high when a retrig request element is encountered
output reg [WIDTH_STAT-1:0] ctr_in, // elements clocked in
output reg [WIDTH_STAT-1:0] ctr_out // elements clocked out
);

localparam int p = P_FIFO_OUT;
localparam int length = 2**p;
localparam int widthu = p+1;   // width of `used` port
wire [widthu-1:0] used; // Keep this port! Useful for debugging.

logic rdreq_nrr; // rdreq if no retrig_requested
assign rdreq_nrr = rdreq && ~retrig_requested;

// Statistics counters
always_ff @(posedge wrclk) begin
  if (reset) begin
    ctr_in <= '0;
  end else begin
    if (wrreq) begin  // all writes
      ctr_in <= ctr_in + 1;
    end
  end
end

always_ff @(posedge rdclk) begin
  if (rdrst) begin
    ctr_out <= '0;
  end else begin
    if (rdreq_nrr) begin  // all reads
      ctr_out <= ctr_out + 1;
    end
  end
end

logic [WIDTH_DATACTRL-1:0] qc; // combined output from FIFO (data+control)
logic empty;

dcfifo #(
 .intended_device_family("CycloneV"), // only relevant for functional simulation
 .lpm_numwords(length),
 .lpm_width(WIDTH_DATACTRL),
 .lpm_widthu(widthu),
 .add_usedw_msb_bit("ON"),            // adds one bit to wrusedw port, prevents wrapping around to zero when FIFO is full
 .lpm_showahead("ON"),                // OFF (Normal mode: FIFO IP core treats the rdreq port as a normal read request that only performs read operation when the port is asserted.
 .add_ram_output_register("ON"),      // For Cyclone series, DCFIFO only supports registered q output in Normal mode.
 .overflow_checking("ON"),
 .underflow_checking("ON"),
 .clocks_are_synchronized("FALSE"),   // if FALSE, the core handles potential metastability if clocks are not multiples of each other (different clock domains); default is FALSE
 .wrsync_delaypipe(4),                // 4 FF stages for write->read sync
 .rdsync_delaypipe(4),                // 4 FF stages for read->write sync
 .write_aclr_synch("ON")              // Add a circuit that causes the aclr port to be internally synchronized by the wrclk clock. Adding the circuit prevents the race condition between the wrreq and aclr ports that could corrupt the FIFO IP core.
)
inst1
(
 .wrclk   (wrclk),
 .rdclk   (rdclk),
 .data    (data),
 .wrreq   (wrreq),
 .rdreq   (rdreq_nrr),
 .aclr    (reset),
 .q       (qc),
 .rdempty (empty),
 .wrusedw (used)
);

logic [WIDTH_DATA-1:0] q;
assign q = qc[WIDTH_DATA-1:0];
logic [WIDTH_CONTROL-1:0] control;
assign control = qc[WIDTH_DATACTRL-1:WIDTH_DATA];

// Element type control settings
logic is_retrig, is_last, is_data, is_no_strobe, is_prng;
assign is_retrig    = control[BIT_RETRIG];         // indicates a retrigger request
assign is_last      = control[BIT_TERMINATE];      // indicates the end of the sequence
assign is_data      = ~is_retrig & ~is_last;        // regular (data) element
assign is_no_strobe = control[BIT_NO_STROBE];      // element should not be strobed out
assign is_prng      = control[BIT_PRNG];           // randomize the output value

// 'done' signal logic: 'done' signal indicates a successful completion of the RL decoding process, i.e., if there were no buffer underflows
always_ff @(posedge rdclk) begin
  if (rdrst)
    done <= 0;
  else if (rdreq && is_last && !buffer_error)
    // 'done' is asserted when the terminator event is encountered at the output of the FIFO, but only if there was no buffer underflow.
    // This is different from the behaviour of 'strobe_enable' which is deasserted irrespective of the error status.
    done <= 1;
end

always_ff @(posedge rdclk) begin
  if (rdrst)
    retrig_requested <= 0;
  else if (rdreq && is_retrig)
    retrig_requested <= 1;
  else
    retrig_requested <= 0;
end

// 'buffer_error' signal logic: triggered if the output FIFO buffer is emptied before the completion of the RL decoding process
always_ff @(posedge rdclk) begin
  if (rdrst)
    buffer_error <= 0;
  else if (rdreq && is_data && !done && empty)
    buffer_error <= 1;
end

logic [63:0] rnd;
prng_xoroshiro128plus prng(
 .clk(rdclk),
 .rst_n(~rdrst),
 .en(1'b1),
 .reseed(1'b0),
 .rnd(rnd),
 .seed(128'd123456789) // fixed seed
);

logic valid, wr_last;
// `retrig_requested` blocks normal output advancement while the streamer is waiting to re-arm on
// a later trigger event, so valid data writes are suppressed in that state.
assign valid   = rdreq && is_data && !empty && !done && !retrig_requested && !is_no_strobe;
assign wr_last = rdreq && is_last && !empty && !done;

always_ff @(posedge rdclk) begin
  if (rdrst) begin
    qout <= 0;
    qout_valid <= 0;
  end else begin
    if (valid || wr_last) begin
      qout <= (is_prng ? rnd[WIDTH_DATA-1:0] : q);
     end else begin
      qout <= qout; // this allows to keep the "final value" in the output register
    end
    qout_valid <= valid;
  end
end

// 'strobe_enable' logic: asserted when data are requested (rdreq=1) and available (empty!=1). Deasserted when
// the terminating element of the sequence is encountered (done=1) or if reset is triggered (rdrst=1).
always_ff @(posedge rdclk) begin
  if (rdreq && !empty && !rdrst && !done)
    strobe_enable <= 1'b1;
  else
    strobe_enable <= 1'b0;
end

logic strobe_clk;
assign strobe_clk = ~rdclk;
assign strobe = qout_valid & strobe_clk;

localparam fifo_threshold = length-16;

// wrusedw is in the wrclk clock domain
always_ff @(posedge wrclk) begin
  if (reset) begin
    almost_full <= 0;
  end else begin
    almost_full <= (used > fifo_threshold);
  end
end

  // ---- Simulation guards ----
  // synthesis translate_off
  always_ff @(posedge wrclk) if (!reset) begin
    assert(!(almost_full && wrreq))
      else $error("Write attempted while FIFO almost_full");
  end
  // synthesis translate_on

endmodule

`default_nettype wire // turn implicit nets on again to avoid side-effects
