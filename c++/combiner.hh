// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Host-side wrappers for the combiner subsystem.
//
// The combiner blocks are the late-stage routing/mixing layer for multiple streamer
// outputs and trigger groups. This header mirrors the shared Avalon-MM programming model
// used by the RTL: mode selection plus per-port inversion, masking, forcing, and readback.
// Architectural overview lives in `docs/docs/combiner.md` and `ip/combiner/README.md`.

#pragma once

#include <cassert>
#include <iostream>
#include <random>
#include <cstdint>
#include <string>
#include <sstream>
#include <bitset>

#include "tidbit.hh"
#include "config.h"
#include "fpga.hh"

// Returns a string representing the trigger input and control (enable, force, reset) signals
inline std::string trig_parse(const uint32_t v)
{
  std::stringstream ss;
  ss << "trig=" << std::bitset<WIDTH_TRIGGER>(v)
    << (v & TRIG_CTRL_ENABLE ? " [enable]" : "")
    << (v & TRIG_CTRL_FORCE  ? " [force]"  : "")
    << (v & TRIG_CTRL_RESET  ? " [reset]"  : "");
  return ss.str();
}

enum class comb_mode : int { SEL1 = 0, SEL2 = 1, SEL3 = 2, SEL4 = 3, AND = 4, OR = 5, XOR = 6,
    XNOR = 7, MAJ = 8, BLOCK8 = 9, BLOCK16 = 10, SUM12 = 11, SUM1234 = 12, DIFF12 = 13 };

inline std::string to_string(const comb_mode c) {
  switch(c) {
  case comb_mode::SEL1: return "SEL1";
  case comb_mode::SEL2: return "SEL2";
  case comb_mode::SEL3: return "SEL3";
  case comb_mode::SEL4: return "SEL4";
  case comb_mode::AND: return "AND";
  case comb_mode::OR: return "OR";
  case comb_mode::XOR: return "XOR";
  case comb_mode::XNOR: return "XNOR";
  case comb_mode::MAJ: return "MAJ";
  case comb_mode::BLOCK8: return "BLOCK8";
  case comb_mode::BLOCK16: return "BLOCK16";
  case comb_mode::SUM12: return "SUM12";
  case comb_mode::SUM1234: return "SUM1234";
  case comb_mode::DIFF12: return "DIFF12";
  }
  return "";
}

// interface ports (read returns the register value)
constexpr port_t C_CFG    = 0b0000;

constexpr port_t C_INVo   = 0b0001;
constexpr port_t C_INV1   = 0b0100;
constexpr port_t C_INV2   = 0b0101;
constexpr port_t C_INV3   = 0b0110;
constexpr port_t C_INV4   = 0b0111;

constexpr port_t C_MASKo  = 0b0010;
constexpr port_t C_MASK1  = 0b1000;
constexpr port_t C_MASK2  = 0b1001;
constexpr port_t C_MASK3  = 0b1010;
constexpr port_t C_MASK4  = 0b1011;

constexpr port_t C_VALUEo = 0b0011;
constexpr port_t C_VALUE1 = 0b1100;
constexpr port_t C_VALUE2 = 0b1101;
constexpr port_t C_VALUE3 = 0b1110;
constexpr port_t C_VALUE4 = 0b1111;

// config port bits
constexpr int B_FORCEo = 16;
constexpr int B_FORCE1 = 17;
constexpr int B_FORCE2 = 18;
constexpr int B_FORCE3 = 19;
constexpr int B_FORCE4 = 20;

constexpr int B_RBo    = 24; // readback
constexpr int B_RB1    = 25;
constexpr int B_RB2    = 26;
constexpr int B_RB3    = 27;
constexpr int B_RB4    = 28;

constexpr uint32_t MODE_MASK  =                             0b1111;
constexpr uint32_t FORCE_MASK =         0b000111110000000000000000;
constexpr uint32_t RB_MASK    = 0b00011111000000000000000000000000;

static_assert(FORCE_MASK & (1ULL << B_FORCEo));
static_assert(FORCE_MASK & (1ULL << B_FORCE1));
static_assert(FORCE_MASK & (1ULL << B_FORCE2));
static_assert(FORCE_MASK & (1ULL << B_FORCE3));
static_assert(FORCE_MASK & (1ULL << B_FORCE4));

static_assert(RB_MASK & (1ULL << B_RBo));
static_assert(RB_MASK & (1ULL << B_RB1));
static_assert(RB_MASK & (1ULL << B_RB2));
static_assert(RB_MASK & (1ULL << B_RB3));
static_assert(RB_MASK & (1ULL << B_RB4));

class combiner {
private:
  using Value = uint32_t;

  loc lcfg;
  loc inverto, invert1, invert2, invert3, invert4;
  loc masko, mask1, mask2, mask3, mask4;
  loc valueo, value1, value2, value3, value4;
  Value c; // cfg
  const int NR = 4;

public:
  combiner(const mm &dev, const std::uintptr_t base, std::string name = "combiner"s) :
    lcfg(dev.get_loc(base,    C_CFG*4),    name + "/cfg"),
    inverto(dev.get_loc(base, C_INVo*4),   name + "/invert_out"),
    invert1(dev.get_loc(base, C_INV1*4),   name + "/invert_in1"),
    invert2(dev.get_loc(base, C_INV2*4),   name + "/invert_in2"),
    invert3(dev.get_loc(base, C_INV3*4),   name + "/invert_in3"),
    invert4(dev.get_loc(base, C_INV4*4),   name + "/invert_in4"),
    masko(dev.get_loc(base,   C_MASKo*4),  name + "/mask_out"),
    mask1(dev.get_loc(base,   C_MASK1*4),  name + "/mask_in1"),
    mask2(dev.get_loc(base,   C_MASK2*4),  name + "/mask_in2"),
    mask3(dev.get_loc(base,   C_MASK3*4),  name + "/mask_in3"),
    mask4(dev.get_loc(base,   C_MASK4*4),  name + "/mask_in4"),
    valueo(dev.get_loc(base,  C_VALUEo*4), name + "/value_out"),
    value1(dev.get_loc(base,  C_VALUE1*4), name + "/value_in1"),
    value2(dev.get_loc(base,  C_VALUE2*4), name + "/value_in2"),
    value3(dev.get_loc(base,  C_VALUE3*4), name + "/value_in3"),
    value4(dev.get_loc(base,  C_VALUE4*4), name + "/value_in4")
    {
      // Do not call mode() or cfg() in the constructor.
      c = 0;
    }

  // Select the main combination mode.
  void mode(comb_mode _m) {
    Value m = static_cast<int>(_m);
    c = (c & ~MODE_MASK) | m;
    lcfg.write(c);
  }

  auto get_mode() {
    return static_cast<comb_mode>(lcfg.read() & MODE_MASK);
  }

  // Replace the raw configuration register. Useful for low-level bring-up and debugging.
  void cfg(Value _c) {
    c = _c;
    lcfg.write(c);
  }

  auto get_cfg() {
    return lcfg.read();
  }

  // Invert selected bits. `n=0` targets the output, `1..4` target inputs.
  void invert(const int n, const Value v) {
    assert(n >= 0 && n <= NR);
    if (n == 0)
      inverto.write(v);
    if (n == 1)
      invert1.write(v);
    if (n == 2)
      invert2.write(v);
    if (n == 3)
      invert3.write(v);
    if (n == 4)
      invert4.write(v);
  }

  Value get_invert(const int n) {
    assert(n >= 0 && n <= NR);
    if (n == 0)
      return inverto.read();
    if (n == 1)
      return invert1.read();
    if (n == 2)
      return invert2.read();
    if (n == 3)
      return invert3.read();
    if (n == 4)
      return invert4.read();
    return 0;
  }

  // Mask selected bits. Ones pass through; zeros suppress that bit position.
  void mask(const int n, const Value v) {
    assert(n >= 0 && n <= NR);
    if (n == 0)
      masko.write(v);
    if (n == 1)
      mask1.write(v);
    if (n == 2)
      mask2.write(v);
    if (n == 3)
      mask3.write(v);
    if (n == 4)
      mask4.write(v);
  }

  Value get_mask(const int n) {
    assert(n >= 0 && n <= NR);
    if (n == 0)
      return masko.read();
    if (n == 1)
      return mask1.read();
    if (n == 2)
      return mask2.read();
    if (n == 3)
      return mask3.read();
    if (n == 4)
      return mask4.read();
    return 0;
  }

  // Store an override value without enabling it yet.
  void value(const int n, const Value v) {
    assert(n >= 0 && n <= NR);
    if (n == 0)
      valueo.write(v);
    if (n == 1)
      value1.write(v);
    if (n == 2)
      value2.write(v);
    if (n == 3)
      value3.write(v);
    if (n == 4)
      value4.write(v);
  }

  Value get_value(const int n) {
    assert(n >= 0 && n <= NR);
    if (n == 0)
      return valueo.read();
    if (n == 1)
      return value1.read();
    if (n == 2)
      return value2.read();
    if (n == 3)
      return value3.read();
    if (n == 4)
      return value4.read();
    return 0;
  }

  // Enable forcing for one input or for the output.
  // For inputs, the stored value still goes through inversion/masking. For the output,
  // the forced value bypasses the normal output processing path.
  void enable_force(const int n) {
    switch (n) {
    case 0:
      cfg(c | (1ULL << B_FORCEo)); // bit set: true = force
      break;
    case 1:
      cfg(c | (1ULL << B_FORCE1));
      break;
    case 2:
      cfg(c | (1ULL << B_FORCE2));
      break;
    case 3:
      cfg(c | (1ULL << B_FORCE3));
      break;
    case 4:
      cfg(c | (1ULL << B_FORCE4));
      break;
    default:
      throw std::runtime_error("enable_force(): incorrect argument");
    }
  }

  void release_force(const int n) {
    switch (n) {
    case 0:
      cfg(c & ~(1ULL << B_FORCEo));
      break;
    case 1:
      cfg(c & ~(1ULL << B_FORCE1));
      break;
    case 2:
      cfg(c & ~(1ULL << B_FORCE2));
      break;
    case 3:
      cfg(c & ~(1ULL << B_FORCE3));
      break;
    case 4:
      cfg(c & ~(1ULL << B_FORCE4));
      break;
    default:
      throw std::runtime_error("release_force(): incorrect argument");
    }
  }

  // call to force() sets up the value and enables the force mode on the chosen bit
  void force(const int n, const Value v) {
    assert(n >= 0 && n <= NR);
    if (n == 0)
      valueo.write(v); // write first
    if (n == 1)
      value1.write(v);
    if (n == 2)
      value2.write(v);
    if (n == 3)
      value3.write(v);
    if (n == 4)
      value4.write(v);
    enable_force(n); // and then enable
  }

  Value get_force(const int n) {
    assert(n >= 0 && n <= NR);
    if (n == 0) {
      cfg(c & ~(1ULL << B_RBo)); // rb bit clear: false = get forced value
      return valueo.read();
    }
    if (n == 1) {
      cfg(c & ~(1ULL << B_RB1));
      return value1.read();
    }
    if (n == 2) {
      cfg(c & ~(1ULL << B_RB2));
      return value2.read();
    }
    if (n == 3) {
      cfg(c & ~(1ULL << B_RB3));
      return value3.read();
    }
    if (n == 4) {
      cfg(c & ~(1ULL << B_RB4));
      return value4.read();
    }
    return 0;
  }

  // Current values, same as get_value()
  Value out() {
    cfg(c | (1ULL << B_RBo)); // rb bit set: true = get value at port
    return valueo.read();
  }
  Value in1() {
    cfg(c | (1ULL << B_RB1));
    return value1.read();
  }
  Value in2() {
    cfg(c | (1ULL << B_RB2));
    return value2.read();
  }
  Value in3() {
    cfg(c | (1ULL << B_RB3));
    return value3.read();
  }
  Value in4() {
    cfg(c | (1ULL << B_RB4));
    return value4.read();
  }

  // Readback: forced values
  void rb_force() {
    cfg(c & ~RB_MASK); // clear RB bits
  }

  // Readback: port values
  void rb_port() {
    cfg(c | RB_MASK); // set RB bits
  }

  void report() {
    std::cout << "---- Combiner report ----" << std::endl;
    const auto cc = get_cfg();
    std::cout << "Combiner setting: 0x" << std::hex << cc << " " <<
      to_string(static_cast<comb_mode>(cc & MODE_MASK)) << std::endl;
    if (c != cc)
      throw std::runtime_error("Unexpected combiner configuration setting.");
    for (int j = 0; j <= NR; j++) {
      const int i = (j < NR ? j+1 : 0); // out comes last
      std::string label = (i == 0 ? "_out" : std::to_string(i));
      std::cout << "invert" << label << "=" << hex_and_bin(get_invert(i)) << std::endl;
      std::cout << " force" << label << "=" << hex_and_bin(get_force(i)) << std::endl;
      std::cout << "  mask" << label << "=" << hex_and_bin(get_mask(i)) << std::endl;
    }
    rb_port();
    for (int j = 0; j <= NR; j++) {
      const int i = (j < NR ? j+1 : 0); // out comes last
      std::cout << (i == 0 ? "out" : "in") << (i == 0 ? "" : std::to_string(i)) << "=" << hex_and_bin(get_value(i)) << std::endl;
    }
    std::cout << "----" << std::endl;
  }

  // returns 0 if successfull
  int self_test() {
    rnd32 rng;
    Value v;
    for (int i = 0; i < 5; i++) {
      v = rng();
      invert(i, v);
      if (get_invert(i) != v)
        return 1;
      v = rng();
      mask(i, v);
      if (get_mask(i) != v)
        return 2;
      v = rng();
      force(i, v);
      if (get_force(i) != v)
        return 3;
    }
    return 0;
  }
};

enum class trig_mode : int { INT = 0, EXT = 1, MISC = 2, AUX = 3, AND = 4, OR = 5, XOR = 6 };

// Specifics for the combiner used for trigger signals.
// Input ports are named
// 1: int
// 2: ext
// 3: misc
class combiner_trig : public combiner {
public:
  combiner_trig(const mm &dev,
                const std::uintptr_t base,
                std::string name = "combiner_trig"s) :
    combiner::combiner(dev, base, name) {}

  void mode(const trig_mode m) {
    combiner::mode(static_cast<comb_mode>(m));
  }

  void invert_int(const uint32_t v) {
    invert(1, v);
  }

  void invert_ext(const uint32_t v) {
    invert(2, v);
  }

  void invert_misc(const uint32_t v) {
    invert(3, v);
  }

  void invert_result(const uint32_t v) {
    invert(0, v);
  }

  void mask_int(const uint32_t v) {
    mask(1, v);
  }

  void mask_ext(const uint32_t v) {
    mask(2, v);
  }

  void mask_misc(const uint32_t v) {
    mask(3, v);
  }
};

inline auto ndx2mode(const int i) {
    return static_cast<comb_mode>(i-1); // not the offset by 1
}
