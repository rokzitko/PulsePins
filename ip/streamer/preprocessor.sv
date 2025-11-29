// SPDX-License-Identifier: MIT
// Sequence preprocessor with robust stalling

`default_nettype none
`include "config.vh"

module preprocessor
#(
  parameter int WIDTH          = `WIDTH_TOTAL,
  parameter int WIDTH_CONTROL  = `WIDTH_CONTROL,
  parameter int WIDTH_COUNTER  = `WIDTH_COUNTER,
  parameter int WIDTH_DATA     = `WIDTH_DATA,
  parameter int POSITIONS      = `MEMORY_POSITIONS,
  parameter int WIDTH_POSITION = $clog2(POSITIONS)
)(
  input  wire               clk,
  input  wire               reset,

  // FIFO #1 interface
  input  wire [WIDTH-1:0]   din,
  input  wire               din_valid,
  output wire               din_ready,

  // FIFO #2 interface
  output wire [WIDTH-1:0]   dout,
  output wire               dout_valid,
  input  wire               dout_ready
);

  // -------------------------------------------------------------------------
  // Decompose input word
  // -------------------------------------------------------------------------

  logic [WIDTH_CONTROL-1:0] control;
  logic [WIDTH_COUNTER-1:0] counter;
  logic [WIDTH_DATA-1:0]    data;

  assign {control, counter, data} = din;

  wire in_fire = din_valid && din_ready;

  // -------------------------------------------------------------------------
  // Control decoding
  // -------------------------------------------------------------------------

  logic pass_bit, pass, store, replay, discard;

  // 0 = pass, 1 = discard / store / replay
  assign pass_bit = (control[`BIT_NOPASS] == 1'b0);

  assign pass    = in_fire &&  pass_bit;
  assign store   = in_fire && (control[`BIT_NOPASS] == 1'b1)
                           && (control[`BIT_STORE]  == 1'b1)
                           && (control[`BIT_REPLAY] == 1'b0);
  assign replay  = in_fire && (control[`BIT_NOPASS] == 1'b1)
                           && (control[`BIT_STORE]  == 1'b0)
                           && (control[`BIT_REPLAY] == 1'b1);
  assign discard = in_fire && (control[`BIT_NOPASS] == 1'b1)
                           && (control[`BIT_STORE]  == 1'b0)
                           && (control[`BIT_REPLAY] == 1'b0); // used in TB

  // -------------------------------------------------------------------------
  // Memory
  // -------------------------------------------------------------------------

  logic [WIDTH-1:0]          memory       [POSITIONS-1:0];
  logic [POSITIONS-1:0]      memory_valid; // for simulation checking
  logic [WIDTH_POSITION-1:0] position;

  assign position = control[`BIT_POSITIONS_LO+WIDTH_POSITION-1:`BIT_POSITIONS_LO];

  always_ff @(posedge clk) begin
    if (reset) begin
      memory_valid <= '0;
    end else if (store) begin
      memory[position]      <= din;
      memory_valid[position] <= 1'b1;
    end
  end

  // -------------------------------------------------------------------------
  // Replay configuration and state
  // -------------------------------------------------------------------------

  // length: number of positions to replay (1..POSITIONS); 0 => no replay
  logic [WIDTH_POSITION:0]  length;
  logic [WIDTH_POSITION:0]  req_len;
  logic [WIDTH_COUNTER-1:0] repetitions;

  // current index and repetition counter
  logic [WIDTH_POSITION-1:0] i;   // 0 .. length-1
  logic [WIDTH_COUNTER-1:0]  j;   // 0 .. repetitions-1

  wire last_elem  = (i == length-1);
  wire last_cycle = (j == repetitions-1);

  // replay active flag and infinite replay flag
  logic active;
  logic infinite;

  // current replay data
  logic [WIDTH-1:0] dout_replay_reg;

  // send: we are outputting from replay
  wire send = active;

  // handshakes
  wire pass_fire   = pass  & dout_ready;
  wire replay_fire = send  & dout_ready;

  // -------------------------------------------------------------------------
  // Replay engine (single source of truth; only advances on replay_fire)
  // -------------------------------------------------------------------------

  always_ff @(posedge clk) begin
    if (reset) begin
      active          <= 1'b0;
      infinite        <= 1'b0;
      length          <= '0;
      repetitions     <= '0;
      i               <= '0;
      j               <= '0;
      dout_replay_reg <= '0;
    end else begin
      if (!active) begin
        // Idle: check for new replay command
        if (replay) begin
          req_len = data[WIDTH_POSITION:0];
          length      <= req_len;
          repetitions <= counter;

          if (req_len != '0) begin
            active   <= 1'b1;
            infinite <= (counter == '0);

            i <= '0;
            j <= '0;

            // first element to output
            dout_replay_reg <= memory['0];
          end else begin
            // zero length: remain idle
            infinite <= 1'b0;
            i        <= '0;
            j        <= '0;
          end
        end
      end else begin
        // Active replay: advance only when downstream accepts a word
        if (replay_fire) begin
          if (infinite) begin
            // Infinite replay: cycle 0..length-1 forever
            if (length > 1) begin
              if (i < length-1) begin
                i               <= i + 1;
                dout_replay_reg <= memory[i + 1];
              end else begin
                i               <= '0;
                dout_replay_reg <= memory['0];
              end
            end else begin
              // length == 1: repeat same element
              i               <= '0;
              dout_replay_reg <= memory['0];
            end
          end else begin
            if (!last_elem) begin
              // advance within a cycle
              i               <= i + 1;
              dout_replay_reg <= memory[i + 1];
            end else begin
              if (!last_cycle) begin
                // start next cycle
                i               <= '0;
                j               <= j + 1;
                dout_replay_reg <= memory['0];
              end else begin
                // finished all repetitions
                active <= 1'b0;
                // i, j, dout_replay_reg can stay as they are
              end
            end
          end
        end
        // If dout_ready == 0 (no replay_fire), we keep active, i, j, dout_replay_reg unchanged.
      end
    end
  end

  // -------------------------------------------------------------------------
  // Outputs and handshakes
  // -------------------------------------------------------------------------

  // Mux between pass-through and replay path
  assign dout = (send ? dout_replay_reg : din);

  // dout_valid pulses exactly on accepted transfers
  assign dout_valid = pass_fire | replay_fire;

  // Ready:
  //  - For pass words, back-pressured by dout_ready.
  //  - For store/replay/discard control words, not back-pressured by dout_ready.
  //  - While replay is active, block new input (matches original behaviour).
  assign din_ready = (pass_bit ? dout_ready : 1'b1) && !active;

endmodule

`default_nettype wire
