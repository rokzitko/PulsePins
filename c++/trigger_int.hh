// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Internal trigger control helpers for the PulsePins host tools.
//
// This wrapper drives the software-controlled internal trigger PIO. It is the low-level
// path used when trigger generation comes directly from host software rather than from the
// trigger combiner or an external signal source.

#pragma once

#include "address_map.hh"
#include "bitops.hh"
#include "misc.hh"
#include "pio.hh"
#include "config.h"

constexpr uint32_t set_low8(uint32_t x, uint8_t b) noexcept {
      return (x & 0xFFFFFF00u) | static_cast<uint32_t>(b);
}

constexpr uint32_t set_masked(uint32_t x, uint32_t mask, uint32_t value) noexcept {
      return (x & ~mask) | (value & mask);
}

class trigger_int : public pio_out {
public:
  trigger_int(mm &dev,
              const address_map::LwRegion base = address_map::lw::pio_trig_int,
              bool clear_at_startup = true,
              std::string name = "trigger_int"s) :
     pio_out(dev, base.base, name) {
       if (clear_at_startup)
         write(0);
     }

  // Update only the trigger-pattern bits while preserving the enable/force/reset flags.
  void trig(const trigger_t p) {
    const auto oldval = read();
    const auto newval = set_masked(oldval, TRIGGER_MASK, p);
    write(newval);
  }

  // Gate whether the internal trigger source is allowed to affect the downstream path.
  void enable(bool x = true) {
    auto val = read();
    if (x)
      BIT_SET(val, PIO1_ENABLE);
    else
      BIT_CLEAR(val, PIO1_ENABLE);
    write(val);
  }

  // Assert or deassert the host-controlled force bit.
  void force(bool x = true) {
    auto val = read();
    if (x)
      BIT_SET(val, PIO1_FORCE);
    else
      BIT_CLEAR(val, PIO1_FORCE);
    write(val);
  }

  // Drive the host-controlled reset bit for the internal trigger path.
  void reset(bool x = true) {
    auto val = read();
    if (x)
      BIT_SET(val, PIO1_RESET);
    else
      BIT_CLEAR(val, PIO1_RESET);
    write(val);
  }
};
