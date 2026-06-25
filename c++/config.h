// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Constants

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include "misc.hh"

constexpr int P_FIFO_IN1 = 10;
constexpr int P_FIFO_IN2 = 11;
constexpr int P_FIFO_OUT = 12;
constexpr int SIZE_FIFO_IN1 = 1 << P_FIFO_IN1;
constexpr int SIZE_FIFO_IN2 = 1 << P_FIFO_IN2;
constexpr int SIZE_FIFO_OUT = 1 << P_FIFO_OUT;
constexpr int almost_shift = 16; // see input_fifo.sv

constexpr int WIDTH_AVS_BUS = 32; // port width of the AVS bus used in the streaming interface
using bus_t = uint32_t;           // corresponding C++ type
constexpr int WIDTH_PORT = 32;    // port width in bits for various FPGA interfaces
using port_t = uint32_t;          // corresponding C++ type

static_assert(sizeof(bus_t)*8 == WIDTH_AVS_BUS);
static_assert(sizeof(port_t)*8 == WIDTH_PORT);

// These must be consistent with ip/streamer/config.vh

constexpr int WIDTH_CONTROL = 32;
constexpr int WIDTH_COUNTER = 32;
constexpr int WIDTH_DATA = 32;
constexpr int WIDTH_TOTAL = WIDTH_CONTROL + WIDTH_COUNTER + WIDTH_DATA;
constexpr int BYTES_TOTAL = WIDTH_TOTAL/8;

using control_t = uint32_t; // control register data type
using count_t   = uint32_t; // counter register data type
using value_t   = uint32_t; // value data type

static_assert(sizeof(control_t)*8 == WIDTH_CONTROL);
static_assert(sizeof(count_t)*8   == WIDTH_COUNTER);
static_assert(sizeof(value_t)*8   == WIDTH_DATA);

const uint64_t max_count_t = std::numeric_limits<count_t>::max(); // for checking boundaries

using trigger_t = uint8_t;
constexpr int WIDTH_TRIGGER = 8;
constexpr int TRIGGER_MASK = 0xFF;

using aux_t = uint8_t;
constexpr int WIDTH_AUX = 8;
constexpr int AUX_MASK = 0xFF;

// Function aliases; we need two sets due to different interfaces (_t versions accept
// a string; those without _t suffix accept InputParse, keyword, default_value)
const auto parse_value_t = parse_uint32_t;
const auto parse_count_t = parse_uint32_t;
const auto parse_trigger_t = parse_uint8_t;
const auto parse_value = parse_uint32;
const auto parse_count = parse_uint32;
const auto parse_trigger = parse_uint8;

constexpr int PIO1_ENABLE = 8;
constexpr int PIO1_FORCE  = 9;
constexpr int PIO1_RESET  = 10;

static_assert(sizeof(trigger_t)*8 == WIDTH_TRIGGER);

// control (least significant bits):
//        0 - regular element [bit 0]
//        1 - trigger element
//       01 - trigger element (part of the sequence) [bit 1]
//       11 - trigger element (last in the sequence)
//      1x0 - sequence terminator (subtype of regular data) [bit 2]
//     1xx0 - pulses without strobes [bit 3]
// 0000xxx0 - load word [bits 4-7]
// 0001xxx0 - bit set (for bit addressing)
// 0010xxx0 - bit clear (for bit addressing)
// 0011xxx0 - bit flip (for bit addressing)
// [bit 8]  - 0=pass, 1=discard/store/replay
// [bit 9]  - store
// [bit 15] - replay
// [bit 16] - retrig
// [bit 17] - random

// These must be consistent with ip/streamer/config.vh
constexpr control_t STROBE       = 0b0000;
constexpr control_t TRIGGERBITS  = 0b0011; // mask for testing the trigger condition
constexpr control_t TRIGGER      = 0b0001;
constexpr control_t TRIGGERFINAL = 0b0010;
constexpr control_t TERMINATE    = 0b0100;
constexpr control_t NOSTROBE     = 0b1000;

constexpr control_t MODEBITS     = 0b1111'0000; // mask for testing the update mode

constexpr control_t BITLOAD      = 0b0000'0000;
constexpr control_t BITSET       = 0b0001'0000;
constexpr control_t BITCLEAR     = 0b0010'0000;
constexpr control_t BITFLIP      = 0b0011'0000;

constexpr control_t BITNOT       = 0b0100'0000;
constexpr control_t BITAND       = 0b0101'0000;
constexpr control_t BITOR        = 0b0110'0000;
constexpr control_t BITXOR       = 0b0111'0000;
constexpr control_t BITXNOR      = 0b1000'0000;

constexpr control_t BITSLL       = 0b1001'0000;
constexpr control_t BITSRL       = 0b1010'0000;

constexpr control_t PASS         = 0;
constexpr control_t NOPASS       = (1UL << 8);
constexpr control_t DISCARD      = NOPASS;
constexpr control_t STORE        = NOPASS + (1UL << 9);
constexpr control_t REPLAY       = NOPASS + (1UL << 15);
constexpr control_t RETRIG       = (1UL << 16);
constexpr control_t PRNG         = (1UL << 17);

constexpr size_t POSITIONS = 8; // number of elements that can be stored in fast memory
constexpr size_t SHIFT_POSITION = 10;
constexpr control_t POSITIONS_MASK = (0b111UL << SHIFT_POSITION);

// This must be consistent with ip/streamer/config.vh
constexpr port_t IF_CTRL       = 0b00000;
constexpr port_t INIT_VAL      = 0b00100;
constexpr port_t QOUT_OVERRIDE = 0b00110;
constexpr port_t GATING_W      = 0b00111;

constexpr port_t IF_STATUS     = 0b00000;
constexpr port_t EXT_TRIG_IN   = 0b00001;
constexpr port_t QOUT_STREAMER = 0b00010;
constexpr port_t EXT_TRIG_CTRL = 0b00011;
constexpr port_t QOUT          = 0b00100;
constexpr port_t FIFO_OVERFLOW = 0b00101;
constexpr port_t CRC32         = 0b00110;
constexpr port_t GATING_R      = 0b00111;

constexpr port_t INPUT_FIFO1_CTR_IN_L  = 0b01000;
constexpr port_t INPUT_FIFO1_CTR_IN_H  = 0b01001;
constexpr port_t INPUT_FIFO1_CTR_OUT_L = 0b01010;
constexpr port_t INPUT_FIFO1_CTR_OUT_H = 0b01011;
constexpr port_t INPUT_FIFO2_CTR_IN_L  = 0b01100;
constexpr port_t INPUT_FIFO2_CTR_IN_H  = 0b01101;
constexpr port_t INPUT_FIFO2_CTR_OUT_L = 0b01110;
constexpr port_t INPUT_FIFO2_CTR_OUT_H = 0b01111;
constexpr port_t OUTPUT_FIFO_CTR_IN_L  = 0b10000;
constexpr port_t OUTPUT_FIFO_CTR_IN_H  = 0b10001;
constexpr port_t OUTPUT_FIFO_CTR_OUT_L = 0b10010;
constexpr port_t OUTPUT_FIFO_CTR_OUT_H = 0b10011;

static_assert(GATING_W == GATING_R);

// Sanity tests
static_assert((TRIGGERBITS & TRIGGER)      == TRIGGER);
static_assert((TRIGGERBITS & TRIGGERFINAL) == TRIGGERFINAL);
static_assert((MODEBITS & BITLOAD)  == BITLOAD);
static_assert((MODEBITS & BITSET)   == BITSET);
static_assert((MODEBITS & BITCLEAR) == BITCLEAR);
static_assert((MODEBITS & BITFLIP)  == BITFLIP);

// must be consistent with ip/streamer/st_interface.sv
// status bits
constexpr port_t BUFFER_ERROR = 0x01;
constexpr port_t DONE         = 0x02;
constexpr port_t TRIGGERED    = 0x04;
constexpr port_t ARMED        = 0x08;

// control bits
constexpr port_t STOP                 = 0x01;
constexpr port_t TRIGGER_FORCE_INT    = 0x02;
constexpr port_t TRIGGER_ENABLE_INT   = 0x04;
constexpr port_t RESET                = 0x08;
constexpr port_t TRIGGER_RESET_INT    = 0x10;
constexpr port_t QOUT_SELECT          = 0x20;
constexpr port_t STOP_ON_BUFFER_ERROR = 0x40;

// bit masks for EXT_TRIG_CTRL
constexpr port_t EXT_TRIG_CTRL_ENABLE = 1<<0;
constexpr port_t EXT_TRIG_CTRL_FORCE  = 1<<1;
constexpr port_t EXT_TRIG_CTRL_RESET  = 1<<2;

// bit masks for TRIG_CTRL when combined with other signals (in combiner)
constexpr port_t TRIG_CTRL_ENABLE = 1<<(WIDTH_TRIGGER+0);
constexpr port_t TRIG_CTRL_FORCE  = 1<<(WIDTH_TRIGGER+1);
constexpr port_t TRIG_CTRL_RESET  = 1<<(WIDTH_TRIGGER+2);

// Final value when streaming terminates
constexpr value_t default_final_value = 0xFFFFFFFF;
