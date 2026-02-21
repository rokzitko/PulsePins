//`default_nettype none

module cdc_mailbox #(
  parameter int WIDTH = 32
) (
  // Input clock domain
  input  logic               in_clk,
  input  logic               in_reset,   // synchronous to in_clk (can adapt to async if needed)
  input  logic [WIDTH-1:0]   in_data,
  input  logic               in_valid,

  // Output clock domain
  input  logic               out_clk,
  input  logic               out_reset,  // synchronous to out_clk
  output logic [WIDTH-1:0]   out_data,
  input  logic               out_req,    // hold request: when 1, out_data must not change
  output logic               out_valid   // indicates out_data is synchronized & ready
);

  // -----------------------------
  // Input-domain storage & toggle
  // -----------------------------
  logic [WIDTH-1:0] in_buf;
  logic             in_toggle;           // flips whenever new sample is committed
  logic             out_hold_in;         // out_req synchronized into in_clk domain

  // Synchronize out_req into in_clk domain (2FF)
  logic out_req_in_ff1, out_req_in_ff2;
  always_ff @(posedge in_clk) begin
    if (in_reset) begin
      out_req_in_ff1 <= 1'b0;
      out_req_in_ff2 <= 1'b0;
    end else begin
      out_req_in_ff1 <= out_req;
      out_req_in_ff2 <= out_req_in_ff1;
    end
  end
  assign out_hold_in = out_req_in_ff2;

  // Capture in_data only when not held by out_req (backpressure)
  always_ff @(posedge in_clk) begin
    if (in_reset) begin
      in_buf    <= '0;
      in_toggle <= 1'b0;
    end else begin
      if (in_valid && !out_hold_in) begin
        in_buf    <= in_data;
        in_toggle <= ~in_toggle;
      end
    end
  end

  // -----------------------------------
  // Output-domain sync & data transfer
  // -----------------------------------
  // Synchronize the toggle into out_clk domain
  logic in_toggle_out_ff1, in_toggle_out_ff2;
  always_ff @(posedge out_clk) begin
    if (out_reset) begin
      in_toggle_out_ff1 <= 1'b0;
      in_toggle_out_ff2 <= 1'b0;
    end else begin
      in_toggle_out_ff1 <= in_toggle;
      in_toggle_out_ff2 <= in_toggle_out_ff1;
    end
  end

  // Detect new sample availability in out_clk domain
  logic in_toggle_out_prev;
  logic new_sample_pulse;
  always_ff @(posedge out_clk) begin
    if (out_reset) begin
      in_toggle_out_prev <= 1'b0;
    end else begin
      in_toggle_out_prev <= in_toggle_out_ff2;
    end
  end
  assign new_sample_pulse = (in_toggle_out_ff2 ^ in_toggle_out_prev);

  // Bring the data across: because in_buf changes only when out_req is low
  // and we only *latch* into out_data when out_req is low, this avoids
  // changing out_data while held. The latching edge occurs after the toggle
  // is synchronized, giving time for in_buf to be stable.
  logic [WIDTH-1:0] in_buf_sampled;

  // Optional: register a sampled version of in_buf in out_clk domain.
  // This reads the bus directly; stability is provided by the handshake:
  // in_buf only changes when out_req is low (which is exactly when we allow updates).
  always_ff @(posedge out_clk) begin
    if (out_reset) begin
      in_buf_sampled <= '0;
    end else begin
      // keep it simple: capture whenever a new sample is announced
      if (new_sample_pulse) begin
        in_buf_sampled <= in_buf;
      end
    end
  end

  // Output data/valid with hold behavior
  always_ff @(posedge out_clk) begin
    if (out_reset) begin
      out_data  <= '0;
      out_valid <= 1'b0;
    end else begin
      // out_valid becomes true once we have at least one sample
      if (new_sample_pulse)
        out_valid <= 1'b1;

      // Update out_data only when NOT held
      if (!out_req) begin
        // If a new sample arrived, commit it to out_data.
        // If no new sample, keep out_data unchanged.
        if (new_sample_pulse)
          out_data <= in_buf_sampled;
      end
      // else: out_req==1 => hold out_data constant
    end
  end

endmodule

//`default_nettype wire
