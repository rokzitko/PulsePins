// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

constexpr uint32_t set_low8(uint32_t x, uint8_t b) noexcept {
      return (x & 0xFFFFFF00u) | static_cast<uint32_t>(b);
}

constexpr uint32_t set_masked(uint32_t x, uint32_t mask, uint32_t value) noexcept {
      return (x & ~mask) | (value & mask);
}

class trigger_int : public pio_out {
 public:
   trigger_int(mm &dev, uintptr_t base = PIO_TRIG_INT_BASE, bool clear_at_startup = true) : pio_out(dev, base) {
     if (clear_at_startup)
       write(0);
   }

   void trig(const trigger_t p) {
     const auto oldval = read();
     const auto newval = set_masked(oldval, TRIGGER_MASK, p);
     write(newval);
   }

   void enable(bool x = true) {
     auto val = read();
     if (x)
       BIT_SET(val, PIO1_ENABLE);
     else
       BIT_CLEAR(val, PIO1_ENABLE);
     write(val);
   }

   void force(bool x = true) {
     auto val = read();
     if (x)
       BIT_SET(val, PIO1_FORCE);
     else
       BIT_CLEAR(val, PIO1_FORCE);
     write(val);
   }

   void reset(bool x = true) {
     auto val = read();
     if (x)
       BIT_SET(val, PIO1_RESET);
     else
       BIT_CLEAR(val, PIO1_RESET);
     write(val);
   }
};
