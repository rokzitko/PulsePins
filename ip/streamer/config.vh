// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Rok Zitko

`ifndef _CONFIG_VH_
`define _CONFIG_VH_

// Shared configuration and programmer-visible constants for the streamer subsystem.
//
// This file is the common reference for:
//   - encoded element layout on the ingress path
//   - control-bit assignments used by the decoder and software sequence model
//   - streamer register enums used by `st_interface.sv` and host-side wrappers
//   - FIFO sizing choices that affect throughput/latency tradeoffs
//
// Architectural overview lives in `ip/streamer/README.md` and `docs/docs/streamer.md`.

parameter int WIDTH_AVS = 32; // Avalon MM interface data-width

// Blocks of {control, counter, data}
parameter int WIDTH_CONTROL = 32;
parameter int WIDTH_COUNTER = 32;
parameter int WIDTH_DATA = 32;
parameter int WIDTH_DATACTRL = WIDTH_DATA+WIDTH_CONTROL;
parameter int WIDTH_TOTAL = WIDTH_DATA+WIDTH_COUNTER+WIDTH_CONTROL;

// Blocks of {control, mask, pattern}, mask and pattern have equal width
parameter int WIDTH_TRIGGER = 8;
parameter int WIDTH_TRIGGER_CONTROL = 16;
parameter int P_FIFO_TRIGGER = 8; // 2^p=256

// statistics counters bit width
parameter int WIDTH_STAT = 64;

// Only one of the following buffer profiles should be defined at a time.
// These values shape how much burstiness the control-side path can absorb before the
// decoder/output path becomes the bottleneck.
//`define SMALL_BUFFERS
`define STANDARD_BUFFERS
//`define BIG_BUFFERS

`ifdef SMALL_BUFFERS
parameter int P_FIFO_IN1 = 6;  // 2^p=64
parameter int P_FIFO_IN2 = 8;  // 2^p=256
parameter int P_FIFO_OUT = 10; // 2^p=1024
`endif

`ifdef STANDARD_BUFFERS
parameter int P_FIFO_IN1 = 10; // 2^p=1024
parameter int P_FIFO_IN2 = 11; // 2^p=2048
parameter int P_FIFO_OUT = 12; // 2^p=4096
`endif

`ifdef BIG_BUFFERS
parameter int P_FIFO_IN1 = 10; // 2^p=1024
parameter int P_FIFO_IN2 = 12; // 2^p=4096
parameter int P_FIFO_OUT = 14; // 2^p=16384
`endif

// Memory
parameter int MEMORY_POSITIONS = 8;

// Bit positions in the per-element control word; 0-based indexes.
// The same layout is used by the host-side sequence model in `c++/`.
parameter int BIT_TRIGGER       = 0;  // 0 = regular element, 1 = trigger element
parameter int BIT_TRIGGER_FINAL = 1;  // 1 = final element in a chain of trigger conditions
parameter int BIT_TERMINATE     = 2;  // 1 = terminating element of the sequence
parameter int BIT_NO_STROBE     = 3;  // 1 = do not strobe data out (but the data will be present on the output data bus)
parameter int BIT_MODE_LO       = 4;
parameter int BIT_MODE_HI       = 7;  // [BIT_MODE_HI:BIT_MODE_LO] is the extent of bits controlling the meaning of data bits (load, set, clear, flip)
parameter int BIT_NOPASS        = 8;  // if high, the element is processed; if low, the element passes through the postprocessor unmodified
parameter int BIT_STORE         = 9;  // if high, the element is stored in fast memory
parameter int BIT_POSITIONS_LO  = 10;
parameter int BIT_POSITIONS_HI  = 13; // BIT_POSITIONS_LO+WIDTH_POSITION-1=10+4-1=13
parameter int BIT_REPLAY        = 15; // if high, the sequence in fast memory is decoded ("replayed")
parameter int BIT_RETRIG        = 16; // wait for a new trigger event
parameter int BIT_PRNG          = 17; // output a pseudorandom number

typedef enum logic [3:0] {
  BITLOAD  = 4'b0000,
  BITSET   = 4'b0001,
  BITCLEAR = 4'b0010,
  BITFLIP  = 4'b0011,

  BITNOT   = 4'b0100,
  BITAND   = 4'b0101,
  BITOR    = 4'b0110,
  BITXOR   = 4'b0111,
  BITXNOR  = 4'b1000,

  BITSLL   = 4'b1001,
  BITSRL   = 4'b1010
} bit_op_t;

// Avalon-MM write registers implemented by `st_interface.sv`.
typedef enum logic [4:0] {
  IF_CTRL       = 5'b00000,
  INIT_VAL      = 5'b00100,
  QOUT_OVERRIDE = 5'b00110,
  GATING_W      = 5'b00111
} st_if_w_t;

// Avalon-MM read registers implemented by `st_interface.sv`.
typedef enum logic [4:0] {
  IF_STATUS     = 5'b00000,
  EXT_TRIG_IN   = 5'b00001,
  QOUT_STREAMER = 5'b00010,
  EXT_TRIG_CTRL = 5'b00011,
  QOUT          = 5'b00100,
  OVERFLOW      = 5'b00101,
  CRC32         = 5'b00110,
  GATING_R      = 5'b00111,
  ST_INF1_IN_L  = 5'b01000,
  ST_INF1_IN_H  = 5'b01001,
  ST_INF1_OUT_L = 5'b01010,
  ST_INF1_OUT_H = 5'b01011,
  ST_INF2_IN_L  = 5'b01100,
  ST_INF2_IN_H  = 5'b01101,
  ST_INF2_OUT_L = 5'b01110,
  ST_INF2_OUT_H = 5'b01111,
  ST_OUTF_IN_L  = 5'b10000,
  ST_OUTF_IN_H  = 5'b10001,
  ST_OUTF_OUT_L = 5'b10010,
  ST_OUTF_OUT_H = 5'b10011
} st_if_r_t;

`endif
