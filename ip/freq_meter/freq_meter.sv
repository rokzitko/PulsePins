// Avalon-MM controlled multi-channel frequency meter.
//
// Each observed input is itself a running clock, so direct sampling in the Avalon domain
// would be unsafe and inaccurate. The implementation therefore:
//   - counts edges in each input clock domain
//   - converts those counters to Gray code
//   - synchronizes the Gray values into the gate/reference clock domain
//   - computes per-gate deltas there
//   - transfers coherent latest results back through an acknowledged CDC mailbox
//
// This is the main hardware block wrapped by `c++/freq_meter.hh`.

module freq_meter_avalon_gray #(
  parameter int N_CH        = 4,
  parameter int COUNTER_W   = 32   // width of edge counters and results
)(
  // Avalon-MM (avs_clk)
  input  wire                avs_clk,
  input  wire                avs_reset,
  input  wire [3:0]          avs_address,
  input  wire                avs_read,
  input  wire                avs_write,
  input  wire [31:0]         avs_writedata,
  output reg  [31:0]         avs_readdata,
  output wire                avs_waitrequest,

  // Reference / gate clock domain
  input  wire                cnt_clk,
  input  wire                cnt_reset,
  input  wire [N_CH-1:0]     sig_in          // each is a continuously running clock
);

  assign avs_waitrequest = 1'b0;

  // --------------------------
  // Avalon regs (avs_clk domain)
  // --------------------------
  reg        reg_enable;
  reg [31:0] reg_gate_len;

  // A coherent mailbox retains the latest configuration even if several writes
  // occur between reference-clock edges. Clear requests travel as a sequence
  // number so an even number of requests cannot cancel each other.
  reg [31:0] clear_seq_avs;
  reg        control_update_avs;

  // Results sampled into avs_clk domain.
  wire [COUNTER_W-1:0] result_avs [N_CH];

  // Results from cnt_clk domain, published as one coherent multi-channel bundle.
  wire [COUNTER_W-1:0] result_cnt [N_CH];
  wire [N_CH*COUNTER_W-1:0] result_bundle_cnt, result_bundle_avs;
  reg result_update_cnt;
  cdc_bus_update #(.WIDTH(N_CH*COUNTER_W), .RESET_VALUE('0)) result_cdc (
    .src_clk(cnt_clk), .src_reset(cnt_reset), .src_data(result_bundle_cnt),
    .src_update(result_update_cnt), .src_busy(),
    .dst_clk(avs_clk), .dst_reset(avs_reset), .dst_accept(1'b1),
    .dst_data(result_bundle_avs), .dst_valid(), .dst_pending()
  );

  // --------------------------
  // Avalon address map (word offsets)
  // 0x00 CTRL: bit0 enable (RW), bit1 clear (W1P)
  // 0x04 GATE_LEN (RW) in cnt_clk cycles, min 1
  // 0x08 N_CH (RO)
  // 0x10 RESULT[i] (RO), consecutive words
  // --------------------------
  localparam logic [3:0] A_CTRL     = 4'd0;
  localparam logic [3:0] A_GATE_LEN = 4'd1;
  localparam logic [3:0] A_NCH      = 4'd2;
  localparam logic [3:0] A_RES_BASE = 4'd4;
  localparam logic [4:0] A_RES_LIMIT = 5'(A_RES_BASE) + 5'(N_CH);
  localparam int RESULT_INDEX_W = (N_CH <= 1) ? 1 : $clog2(N_CH);
  logic [RESULT_INDEX_W-1:0] result_index;

  always @(*) begin
    result_index = avs_address[RESULT_INDEX_W-1:0] - A_RES_BASE[RESULT_INDEX_W-1:0];
  end

  initial assert (A_RES_LIMIT <= 5'd16)
    else $error("N_CH exceeds the Avalon address map");

  always @(posedge avs_clk or posedge avs_reset) begin
    if (avs_reset) begin
      reg_enable          <= 1'b0;
      reg_gate_len        <= 32'd100000;
      clear_seq_avs      <= 32'd0;
      control_update_avs <= 1'b0;
    end else begin
      control_update_avs <= avs_write && (avs_address == A_CTRL || avs_address == A_GATE_LEN);
      if (avs_write) begin
        case (avs_address)
          A_CTRL: begin
            reg_enable     <= avs_writedata[0];
            if (avs_writedata[1]) begin
              clear_seq_avs <= clear_seq_avs + 1'b1; // W1P
            end
          end
          A_GATE_LEN: begin
            reg_gate_len        <= avs_writedata;
          end
          default: /* no-op */ ;
        endcase
      end
    end
  end

  always @(*) begin
    avs_readdata = 32'h0;
    if ({1'b0, avs_address} >= {1'b0, A_RES_BASE} && {1'b0, avs_address} < A_RES_LIMIT) begin
      avs_readdata = result_avs[result_index];
    end else unique case (avs_address)
      A_CTRL: begin
        avs_readdata[0] = reg_enable;
        avs_readdata[1] = 1'b0;
      end
      A_GATE_LEN: avs_readdata = reg_gate_len;
      A_NCH:      avs_readdata = N_CH[31:0];
      default:    avs_readdata = 32'h0;
    endcase
  end

  // --------------------------
  // CDC controls: avs_clk -> cnt_clk (coherent latest-value mailbox)
  // --------------------------
  wire [64:0] control_cnt;
  wire control_valid_cnt;
  wire enable_cnt = control_cnt[0];
  wire [31:0] gate_len_cnt = control_cnt[32:1] == 0 ? 32'd1 : control_cnt[32:1];
  reg [31:0] clear_seq_seen_cnt;
  cdc_bus_update #(.WIDTH(65), .RESET_VALUE({32'd0, 32'd100000, 1'b0})) control_cdc (
    .src_clk(avs_clk), .src_reset(avs_reset),
    .src_data({clear_seq_avs, reg_gate_len, reg_enable}), .src_update(control_update_avs), .src_busy(),
    .dst_clk(cnt_clk), .dst_reset(cnt_reset), .dst_accept(1'b1),
    .dst_data(control_cnt), .dst_valid(control_valid_cnt), .dst_pending()
  );
  always @(posedge cnt_clk or posedge cnt_reset) begin
    if (cnt_reset) clear_seq_seen_cnt <= 0;
    else if (control_valid_cnt) clear_seq_seen_cnt <= control_cnt[64:33];
  end
  wire clear_pulse_cnt = control_valid_cnt && (control_cnt[64:33] != clear_seq_seen_cnt);

  // ============================================================
  // Per-channel input-clocked Gray counters + CDC into cnt_clk
  // ============================================================

  // Binary and Gray in each input clock domain
  reg [COUNTER_W-1:0] bin_in [N_CH];
  reg [COUNTER_W-1:0] gray_in [N_CH];

  // Gray synchronized into cnt_clk (2-stage per bit)
  reg [COUNTER_W-1:0] gray_meta [N_CH];
  reg [COUNTER_W-1:0] gray_sync [N_CH];

  // Previous sampled binary count in cnt_clk domain (for delta)
  reg [COUNTER_W-1:0] bin_prev_cnt [N_CH];

  // Stable-held results in cnt_clk domain
  reg [COUNTER_W-1:0] result_cnt_r [N_CH];

  genvar g;
  generate
    for (g = 0; g < N_CH; g++) begin : CH
      // Input-clocked increment (this treats sig_in[g] as a clock)
      always @(posedge sig_in[g] or posedge cnt_reset) begin
        if (cnt_reset) begin
          bin_in[g]  <= '0;
          gray_in[g] <= '0;
        end else begin
          bin_in[g]  <= bin_in[g] + {{(COUNTER_W-1){1'b0}},1'b1};
          // gray = (bin>>1) ^ bin, computed from NEXT bin for coherence
          // next_bin = bin_in + 1
          gray_in[g] <= ((bin_in[g] + 1'b1) >> 1) ^ (bin_in[g] + 1'b1);
        end
      end

      // Synchronize Gray bus into cnt_clk (bitwise 2-flop)
      integer b;
      always @(posedge cnt_clk or posedge cnt_reset) begin
        if (cnt_reset) begin
          gray_meta[g] <= '0;
          gray_sync[g] <= '0;
        end else begin
          gray_meta[g] <= gray_in[g];
          gray_sync[g] <= gray_meta[g];
        end
      end

      // expose stable-held result
      assign result_cnt[g] = result_cnt_r[g];
      assign result_bundle_cnt[g*COUNTER_W +: COUNTER_W] = result_cnt[g];
      assign result_avs[g] = result_bundle_avs[g*COUNTER_W +: COUNTER_W];
    end
  endgenerate

  // Gray -> binary conversion (combinational)
  function automatic [COUNTER_W-1:0] gray2bin(input [COUNTER_W-1:0] gr);
    integer k;
    begin
      gray2bin[COUNTER_W-1] = gr[COUNTER_W-1];
      for (k = COUNTER_W-2; k >= 0; k--) begin
        gray2bin[k] = gray2bin[k+1] ^ gr[k];
      end
    end
  endfunction

  // Gate timer in cnt_clk domain; compute per-channel deltas at gate end.
  reg [31:0] gate_down_cnt;

  integer c;
  always @(posedge cnt_clk or posedge cnt_reset) begin
    if (cnt_reset) begin
      gate_down_cnt <= 32'd0;
      result_update_cnt <= 1'b0;
      for (c = 0; c < N_CH; c++) begin
        bin_prev_cnt[c] <= '0;
        result_cnt_r[c] <= '0;
      end
    end else begin
      result_update_cnt <= 1'b0;
      if (clear_pulse_cnt) begin
        gate_down_cnt <= gate_len_cnt;
        result_update_cnt <= 1'b1;
        for (c = 0; c < N_CH; c++) begin
          bin_prev_cnt[c] <= gray2bin(gray_sync[c]);
          result_cnt_r[c] <= '0;
        end
      end else if (!enable_cnt) begin
        gate_down_cnt <= gate_len_cnt;
        for (c = 0; c < N_CH; c++) begin
          bin_prev_cnt[c] <= gray2bin(gray_sync[c]);
          // keep last result or clear; here we clear accumulator notion
          result_cnt_r[c] <= '0;
        end
      end else begin
        if (gate_down_cnt <= 32'd1) begin
          // Gate ends: snapshot current counts and compute delta
          result_update_cnt <= 1'b1;
          for (c = 0; c < N_CH; c++) begin
            logic [COUNTER_W-1:0] bin_now;
            bin_now = gray2bin(gray_sync[c]);
            result_cnt_r[c]  <= bin_now - bin_prev_cnt[c]; // modulo 2^W is fine
            bin_prev_cnt[c]  <= bin_now;
          end
          gate_down_cnt <= gate_len_cnt;
        end else begin
          gate_down_cnt <= gate_down_cnt - 32'd1;
        end
      end
    end
  end

endmodule
