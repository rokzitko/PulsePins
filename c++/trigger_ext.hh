// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

// External trigger status reporting
class trigger_ext : public pio_in {
 public:
   trigger_ext(mm &dev, uintptr_t base) : pio_in(dev, base) {}

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
