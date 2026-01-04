// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Dimensions of structures, bit positions, constants

`define WIDTH_AVS 32 // Avalon MM interface data-width

// Blocks of {control, counter, data}
`define WIDTH_CONTROL 32
`define WIDTH_COUNTER 32
`define WIDTH_DATA 32
`define WIDTH_DATACTRL (`WIDTH_DATA+`WIDTH_CONTROL)
`define WIDTH_TOTAL (`WIDTH_DATA+`WIDTH_COUNTER+`WIDTH_CONTROL)

// Blocks of {control, mask, pattern}, mask and pattern have equal width
`define WIDTH_TRIGGER 8
`define WIDTH_TRIGGER_CONTROL 16
`define P_FIFO_TRIGGER 8 // 2^p=256

// Only one of the following settings should be defined
//`define SMALL_BUFFERS
`define STANDARD_BUFFERS
//`define BIG_BUFFERS

`ifdef SMALL_BUFFERS
`define P_FIFO_IN1 6  // 2^p=64
`define P_FIFO_IN2 8  // 2^p=256
`define P_FIFO_OUT 10 // 2^p=1024
`endif

`ifdef STANDARD_BUFFERS
`define P_FIFO_IN1 10 // 2^p=1024
`define P_FIFO_IN2 11 // 2^p=2048
`define P_FIFO_OUT 12 // 2^p=4096
`endif

`ifdef BIG_BUFFERS
`define P_FIFO_IN1 10 // 2^p=1024
`define P_FIFO_IN2 12 // 2^p=4096
`define P_FIFO_OUT 14 // 2^p=16384
`endif

// Memory
`define MEMORY_POSITIONS 8

// Bit positions in control register; 0-based indexes
`define BIT_TRIGGER       0  // 0 = regular element, 1 = trigger element
`define BIT_TRIGGER_FINAL 1  // 1 = final element in a chain of trigger conditions
`define BIT_TERMINATE     2  // 1 = terminating element of the sequence
`define BIT_NO_STROBE     3  // 1 = do not strobe data out (but the data will be present on the output data bus)
`define BIT_MODE_LO       4
`define BIT_MODE_HI       7  // [BIT_MODE_HI:BIT_MODE_LO] is the extent of bits controlling the meaning of data bits (load, set, clear, flip)
`define BIT_NOPASS        8  // if high, the element is processed; if low, the element passes through the postprocessor unmodified
`define BIT_STORE         9  // if high, the element is stored in fast memory
`define BIT_POSITIONS_LO  10
`define BIT_POSITIONS_HI  BIT_POSITIONS_HI+WIDTH_POSITION-1 // 10+4-1=13
`define BIT_REPLAY        15 // if high, the sequence in fast memory is decoded ("replayed")
`define BIT_RETRIG        16 // wait for a new trigger event
`define BIT_PRNG          17 // output a pseudorandom number

`define BITLOAD  4'b0000
`define BITSET   4'b0001
`define BITCLEAR 4'b0010
`define BITFLIP  4'b0011

`define BITNOT   4'b0100
`define BITAND   4'b0101
`define BITOR    4'b0110
`define BITXOR   4'b0111
`define BITXNOR  4'b1000

`define BITSLL   4'b1001
`define BITSRL   4'b1010

// Write registers in st_interface.v
`define IF_CTRL       5'b00000
`define INIT_VAL      5'b00100
`define QOUT_OVERRIDE 5'b00110
`define GATING_W      5'b00111

// Read registers in st_interface.v
`define IF_STATUS     5'b00000
`define EXT_TRIG_IN   5'b00001
`define QOUT_STREAMER 5'b00010
`define EXT_TRIG_CTRL 5'b00011
`define QOUT          5'b00100
`define OVERFLOW      5'b00101
`define GATING_R      5'b00111
`define ST_INF1_IN_L  5'b01000
`define ST_INF1_IN_H  5'b01001
`define ST_INF1_OUT_L 5'b01010
`define ST_INF1_OUT_H 5'b01011
`define ST_INF2_IN_L  5'b01100
`define ST_INF2_IN_H  5'b01101
`define ST_INF2_OUT_L 5'b01110
`define ST_INF2_OUT_H 5'b01111
`define ST_OUTF_IN_L  5'b10000
`define ST_OUTF_IN_H  5'b10001
`define ST_OUTF_OUT_L 5'b10010
`define ST_OUTF_OUT_H 5'b10011
