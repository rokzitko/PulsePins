// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Rok Zitko

#pragma once

#include <cstdint> // integer types, uint32_t, etc.
#include <string>
#include <iostream>
#include <sstream>
#include <bitset>
#include <memory> // shared_ptr
#include <deque>
#include <unistd.h> // usleep
#include <type_traits> // is_same_v

#include "tidbit.hh"
#include "delay.hh"
#include "fifo.hh"
#include "dma.hh"
#include "misc.hh"
#include "verbosity.hh"
#include "config.h"
#include "definitions.hh"
#include "colors.hh"

// FIFO for streaming out
class streamer_fifo : private fifo
{
 private:
   static constexpr size_t size = SIZE_FIFO_IN1;

   bool has_room() const noexcept {
     return (fill() + BYTES_TOTAL) < SIZE_FIFO_IN1;
   }

   // Low-level output function (ensures correct chunking into 32-bit word parts, especially important
   // for 64-bit extensions)
   void perform_write(const bus_t a, const bus_t b, const bus_t c) const noexcept {
     f.write(a);
     f.write(b);
     f.write(c);
   }

   void write(const bus_t a, const bus_t b, const bus_t c) const noexcept {
     while (1) {
       if (has_room()) {  // prevents lockups!
         perform_write(a, b, c);
         return;
       }
     }
   }

   void write_with_warning(const bus_t a, const bus_t b, const bus_t c) const {
     bool warning_emitted = false;
     while (1) {
       if (has_room()) {
         perform_write(a, b, c);
         return;
       }
       if (!warning_emitted) { // Show warning only once!
         std::cout << "FIFO full, stalling." << std::endl;
         warning_emitted = true;
       }
     }
   }

   void unsafe_write(const bus_t a, const bus_t b, const bus_t c) const noexcept {
     perform_write(a, b, c);
   }

   // Intermediate-level output function (ensures correct order of transmitted data)
   void out_ll(const control_t y, const count_t c, const value_t v) const noexcept {
      write(y, c, v);
   }

 public:
   streamer_fifo(const mm &dev, const std::uintptr_t base, const std::uintptr_t in_csr_base) :
     fifo(dev, base, in_csr_base) {}

   // High-level output function (at the level of el objects)
   void out(const el &e, const bool dump = false) const noexcept {
     out_ll(e.control(), e.count(), e.value());
     if (dump)
       std::cout << "Wrote " << e << std::endl;
   }

   // Returns the number of elements in the FIFO queue as well as the FIFO status (as an informative string)
   void check_fill_status() {
     std::cout << "streamer FIFO fill=" << std::dec << fill() << " status=" << status_str() << std::endl;
   }

   void report() {
     check_fill_status();
   }

   void send_sequence(const Sequence &elements) const noexcept {
     for(const auto &e : elements)
       out(e);
   }
};
