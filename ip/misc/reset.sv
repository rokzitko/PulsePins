// SPDX-License-Identifier: MIT
//
// reset_sync2_hold.v
//
// Robust reset synchronizer:
//   - Two asynchronous reset inputs (active-high).
//   - Asynchronous assertion (for POR, watchdog, etc.).
//   - Synchronous de-assertion (multi-flop).
//   - Programmable hold-after-release.
//
// Parameters:
//   ACTIVE_LOW   : 1 -> output active-low, 0 -> output active-high.
//   STAGES       : synchronizer depth (>=2 recommended).
//   HOLD_CYCLES  : number of extra cycles to keep reset asserted after sync.
//
// Instantiate once per clock domain.

module reset_sync2_hold #(
    parameter bit ACTIVE_LOW  = 0,
    parameter int STAGES = 2,
    parameter int HOLD_CYCLES = 16
) (
    input  wire clk,        // target clock domain
    input  wire rst1_in,    // async reset source 1 (active-high)
    input  wire rst2_in,    // async reset source 2 (active-high)
    output wire rst_out     // synchronized reset (polarity per ACTIVE_LOW)
);

    // ---------------------------------------------------------------------
    // Normalize inputs and combine
    // ---------------------------------------------------------------------
    wire arst_ah = rst1_in | rst2_in;  // combined asynchronous active-high reset

    // ---------------------------------------------------------------------
    // Multi-flop synchronizer for synchronous deassertion
    // ---------------------------------------------------------------------
    (* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED" *)
    reg [STAGES-1:0] sync_ff;

    always @(posedge clk or posedge arst_ah) begin
        if (arst_ah)
            sync_ff <= '0;                         // async assert
        else
            sync_ff <= {sync_ff[STAGES-2:0], 1'b1}; // sync release
    end

    wire sync_released = &sync_ff;  // all ones → synchronized reset released

    // ---------------------------------------------------------------------
    // Hold-after-release counter
    // ---------------------------------------------------------------------
    localparam int CNTR_BITS = $clog2(HOLD_CYCLES + 1);
    reg [CNTR_BITS-1:0] hold_cntr = '0;
    reg hold_active = 1'b1;

    always @(posedge clk or posedge arst_ah) begin
        if (arst_ah) begin
            hold_cntr   <= '0;
            hold_active <= 1'b1;
        end else if (sync_released && hold_active) begin
            if (hold_cntr == HOLD_CYCLES[CNTR_BITS-1:0])
                hold_active <= 1'b0;   // done holding
            else
                hold_cntr <= hold_cntr + 1'b1;
        end
    end

    // Internal active-high reset signal
    wire rst_synced_ah = ~sync_released | hold_active;

    // ---------------------------------------------------------------------
    // Output polarity conversion
    // ---------------------------------------------------------------------
    assign rst_out = (ACTIVE_LOW) ? ~rst_synced_ah : rst_synced_ah;

endmodule
