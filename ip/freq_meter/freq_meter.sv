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

  // Controls to cnt_clk via toggles
  reg        enable_tgl_avs;
  reg        clear_tgl_avs;
  reg [31:0] gate_len_shadow_avs;
  reg        gate_len_tgl_avs;

  // Results sampled into avs_clk domain
  reg [COUNTER_W-1:0] result_avs [N_CH];

  // Results from cnt_clk domain + per-channel update toggles
  wire [COUNTER_W-1:0] result_cnt [N_CH];
  wire [N_CH-1:0]      upd_tgl_cnt;

  // Sync update toggles into avs_clk and sample stable-held result_cnt
  reg [N_CH-1:0] upd_tgl_meta, upd_tgl_sync, upd_tgl_sync_d;

  integer i;
  always @(posedge avs_clk or posedge avs_reset) begin
    if (avs_reset) begin
      upd_tgl_meta   <= '0;
      upd_tgl_sync   <= '0;
      upd_tgl_sync_d <= '0;
      for (i = 0; i < N_CH; i++) result_avs[i] <= '0;
    end else begin
      upd_tgl_meta   <= upd_tgl_cnt;
      upd_tgl_sync   <= upd_tgl_meta;
      upd_tgl_sync_d <= upd_tgl_sync;

      for (i = 0; i < N_CH; i++) begin
        if (upd_tgl_sync[i] ^ upd_tgl_sync_d[i]) begin
          result_avs[i] <= result_cnt[i];
        end
      end
    end
  end

  // --------------------------
  // Avalon address map (word offsets)
  // 0x00 CTRL: bit0 enable (RW), bit1 clear (W1P)
  // 0x04 GATE_LEN (RW) in cnt_clk cycles, min 1
  // 0x08 N_CH (RO)
  // 0x10 RESULT[i] (RO), consecutive words
  // --------------------------
  localparam int A_CTRL     = 0;
  localparam int A_GATE_LEN = 1;
  localparam int A_NCH      = 2;
  localparam int A_RES_BASE = 4;

  always @(posedge avs_clk or posedge avs_reset) begin
    if (avs_reset) begin
      reg_enable          <= 1'b0;
      reg_gate_len        <= 32'd100000;
      gate_len_shadow_avs <= 32'd100000;

      enable_tgl_avs      <= 1'b0;
      clear_tgl_avs       <= 1'b0;
      gate_len_tgl_avs    <= 1'b0;
    end else begin
      if (avs_write) begin
        case (avs_address)
          A_CTRL: begin
            reg_enable     <= avs_writedata[0];
            enable_tgl_avs <= ~enable_tgl_avs;
            if (avs_writedata[1]) begin
              clear_tgl_avs <= ~clear_tgl_avs; // W1P
            end
          end
          A_GATE_LEN: begin
            reg_gate_len        <= avs_writedata;
            gate_len_shadow_avs <= avs_writedata;
            gate_len_tgl_avs    <= ~gate_len_tgl_avs;
          end
          default: /* no-op */ ;
        endcase
      end
    end
  end

  always @(*) begin
    avs_readdata = 32'h0;
    unique case (avs_address)
      A_CTRL: begin
        avs_readdata[0] = reg_enable;
        avs_readdata[1] = 1'b0;
      end
      A_GATE_LEN: avs_readdata = reg_gate_len;
      A_NCH:      avs_readdata = N_CH[31:0];
      4: avs_readdata = result_avs[0];
      5: avs_readdata = result_avs[1];
      6: avs_readdata = result_avs[2];
      7: avs_readdata = result_avs[3];
    endcase
  end

  // --------------------------
  // CDC controls: avs_clk -> cnt_clk (toggle sync)
  // --------------------------
  reg enable_tgl_meta_cnt, enable_tgl_sync_cnt, enable_tgl_sync_d_cnt;
  reg clear_tgl_meta_cnt,  clear_tgl_sync_cnt,  clear_tgl_sync_d_cnt;
  reg gate_len_tgl_meta_cnt, gate_len_tgl_sync_cnt, gate_len_tgl_sync_d_cnt;

  reg        enable_cnt;
  reg [31:0] gate_len_cnt;

  wire [31:0] gate_len_shadow_to_cnt = gate_len_shadow_avs;

  always @(posedge cnt_clk or posedge cnt_reset) begin
    if (cnt_reset) begin
      enable_tgl_meta_cnt     <= 1'b0;
      enable_tgl_sync_cnt     <= 1'b0;
      enable_tgl_sync_d_cnt   <= 1'b0;

      clear_tgl_meta_cnt      <= 1'b0;
      clear_tgl_sync_cnt      <= 1'b0;
      clear_tgl_sync_d_cnt    <= 1'b0;

      gate_len_tgl_meta_cnt   <= 1'b0;
      gate_len_tgl_sync_cnt   <= 1'b0;
      gate_len_tgl_sync_d_cnt <= 1'b0;

      enable_cnt              <= 1'b0;
      gate_len_cnt            <= 32'd100000;
    end else begin
      enable_tgl_meta_cnt   <= enable_tgl_avs;
      enable_tgl_sync_cnt   <= enable_tgl_meta_cnt;
      enable_tgl_sync_d_cnt <= enable_tgl_sync_cnt;

      clear_tgl_meta_cnt    <= clear_tgl_avs;
      clear_tgl_sync_cnt    <= clear_tgl_meta_cnt;
      clear_tgl_sync_d_cnt  <= clear_tgl_sync_cnt;

      gate_len_tgl_meta_cnt   <= gate_len_tgl_avs;
      gate_len_tgl_sync_cnt   <= gate_len_tgl_meta_cnt;
      gate_len_tgl_sync_d_cnt <= gate_len_tgl_sync_cnt;

      if (enable_tgl_sync_cnt ^ enable_tgl_sync_d_cnt) begin
        enable_cnt <= reg_enable;
      end
      if (gate_len_tgl_sync_cnt ^ gate_len_tgl_sync_d_cnt) begin
        gate_len_cnt <= (gate_len_shadow_to_cnt == 32'd0) ? 32'd1 : gate_len_shadow_to_cnt;
      end
    end
  end

  wire clear_pulse_cnt = (clear_tgl_sync_cnt ^ clear_tgl_sync_d_cnt);

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

  // Stable-held results and update toggles in cnt_clk domain
  reg [COUNTER_W-1:0] result_cnt_r [N_CH];
  reg [N_CH-1:0]      upd_tgl_cnt_r;

  assign upd_tgl_cnt = upd_tgl_cnt_r;

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

  // Gate timer in cnt_clk domain, compute deltas at gate end
  reg [31:0] gate_down_cnt;

  integer c;
  always @(posedge cnt_clk or posedge cnt_reset) begin
    if (cnt_reset) begin
      gate_down_cnt <= 32'd0;
      upd_tgl_cnt_r <= '0;
      for (c = 0; c < N_CH; c++) begin
        bin_prev_cnt[c] <= '0;
        result_cnt_r[c] <= '0;
      end
    end else begin
      if (clear_pulse_cnt) begin
        gate_down_cnt <= gate_len_cnt;
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
          for (c = 0; c < N_CH; c++) begin
            logic [COUNTER_W-1:0] bin_now;
            bin_now = gray2bin(gray_sync[c]);
            result_cnt_r[c]  <= bin_now - bin_prev_cnt[c]; // modulo 2^W is fine
            bin_prev_cnt[c]  <= bin_now;
            upd_tgl_cnt_r[c] <= ~upd_tgl_cnt_r[c];
          end
          gate_down_cnt <= gate_len_cnt;
        end else begin
          gate_down_cnt <= gate_down_cnt - 32'd1;
        end
      end
    end
  end

endmodule
