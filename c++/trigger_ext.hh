// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// External trigger status reporting helpers.
//
// This wrapper is read-only: it mirrors the external trigger/control PIO so host tools can
// inspect the live trigger input bits together with the current enable/force/reset state.

#pragma once

#include <bitset>
#include <cstdint>
#include <iostream>

#include "config.h"
#include "pio.hh"

class trigger_ext : public pio_in {
public:
  trigger_ext(mm &dev, uintptr_t base) : pio_in(dev, base) {}

  // Print a decoded view of the current trigger input word and control flags.
  void status() {
    const auto x = read();
    const trigger_t trigger_in = x & TRIGGER_MASK;
    const bool trigger_enable  = x & (1ULL << PIO1_ENABLE);
    const bool trigger_force   = x & (1ULL << PIO1_FORCE);
    const bool trigger_reset   = x & (1ULL << PIO1_RESET);
    std::cout << "trigger: " << std::bitset<WIDTH_TRIGGER>(trigger_in)
      << (trigger_enable ? " [enable]" : "")
      << (trigger_force  ? " [force]"  : "")
      << (trigger_reset  ? " [reset]"  : "") << std::endl;
  }
};
