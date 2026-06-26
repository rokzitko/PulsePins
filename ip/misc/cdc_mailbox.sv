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

  // Capture newly announced data even while the output is held, then commit it once the
  // consumer releases out_req.
  logic [WIDTH-1:0] pending_data;
  logic             pending_valid;

  // Output data/valid with hold behavior
  always_ff @(posedge out_clk) begin
    if (out_reset) begin
      out_data      <= '0;
      out_valid     <= 1'b0;
      pending_data  <= '0;
      pending_valid <= 1'b0;
    end else begin
      if (new_sample_pulse) begin
        pending_data  <= in_buf;
        pending_valid <= 1'b1;
      end

      // Update out_data only when NOT held
      if (!out_req) begin
        if (new_sample_pulse) begin
          out_data      <= in_buf;
          out_valid     <= 1'b1;
          pending_valid <= 1'b0;
        end else if (pending_valid) begin
          out_data      <= pending_data;
          out_valid     <= 1'b1;
          pending_valid <= 1'b0;
        end
      end
      // else: out_req==1 => hold out_data constant
    end
  end

endmodule

//`default_nettype wire
