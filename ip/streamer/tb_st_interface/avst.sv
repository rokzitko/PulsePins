// my_avst_source_fm.sv — lightweight stand-in for avalon_st_source_bfm
module avalon_st_source_bfm #(
  parameter int AVALON_ST_DATA_WIDTH = 96
)(
  input  logic                          clk,
  input  logic                          reset,      // active-high
  output logic [AVALON_ST_DATA_WIDTH-1:0] src_data,
  output logic                          src_valid,
  input  logic                          src_ready,
  output logic                          src_sop,
  output logic                          src_eop
);

  // Internal "transaction" registers
  logic [AVALON_ST_DATA_WIDTH-1:0] tr_data;
  logic tr_sop, tr_eop;
  event signal_src_transaction_complete;

  // Defaults
  initial begin
    src_data  = '0;
    src_valid = 1'b0;
    src_sop   = 1'b0;
    src_eop   = 1'b0;
  end

  // --- Public API (subset compatible with Intel AVIP guide) ---

  // Initialize internal state
  task init();
    // nothing special here, keep for API compatibility
  endtask

  // Set fields for the next single-beat transaction
  task set_transaction_data(input logic [AVALON_ST_DATA_WIDTH-1:0] d);
    tr_data = d;
  endtask
  task set_transaction_sop(input bit v); tr_sop = v; endtask
  task set_transaction_eop(input bit v); tr_eop = v; endtask

  // Push one transaction: assert valid until ready is seen on a clock edge
  task push_transaction();
    // Apply on next clock edge
    @(posedge clk iff !reset);
    src_data  <= tr_data;
    src_sop   <= tr_sop;
    src_eop   <= tr_eop;
    src_valid <= 1'b1;

    // Wait for handshake
    do @(posedge clk); while (!src_ready || reset);

    // Deassert after a successful beat
    src_valid <= 1'b0;
    src_sop   <= 1'b0;
    src_eop   <= 1'b0;

    // Notify completion
    -> signal_src_transaction_complete;
  endtask

endmodule
