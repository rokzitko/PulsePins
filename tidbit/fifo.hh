// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// First-in-first-out buffers

#pragma once

#include <cstdint> // uint32_t
#include <string>
#include <iostream>
#include <iomanip>
#include <bitset>

using namespace std::string_literals;

constexpr int fifo_i_status_shift = 1*sizeof(uint32_t);
static_assert(fifo_i_status_shift == 4);
constexpr int fifo_event_shift = 2*sizeof(uint32_t);
static_assert(fifo_event_shift == 8);

// Documented in "Embedded peripherals IP user guide", section 24 (in version 20.1)
// Use status register in output clock domain (MM interface) for read_fifo
// Use status register in input clock domain (MM interface) for write_fifo
class fifo {
 protected:
   loc f, lfill, lstatus, levent;
 public:
   fifo(const mm &dev, const std::uintptr_t base, const std::uintptr_t csr_base) :
     f(dev.get_loc(base)),
     lfill(dev.get_loc(csr_base)),
     lstatus(dev.get_loc(csr_base, fifo_i_status_shift)),
     levent(dev.get_loc(csr_base, fifo_event_shift))
     {}
   uint32_t read() {
     return f.read();
   }
   void write(const uint32_t val) const noexcept {
     f.write(val);
   }
   auto status() {
     uint32_t status = lstatus.read();
     return status;
   }
   auto status_str(const uint32_t status) {
     std::stringstream ss;
     ss  << std::bitset<6>(status)
         << (status&1 ? " FULL" : "")
         << (status&2 ? " EMPTY" : "")
         << (status&4 ? " ALMOSTFULL" : "")
         << (status&8 ? " ALMOSTEMPTY" : "")
         << (status&16 ? " OVERFLOW" : "")
         << (status&32 ? " UNDERFLOW" : "");
     return ss.str();
   }
   auto status_str() {
     const auto status = lstatus.read();
     return status_str(status);
   }
   auto full() {
     const auto status = lstatus.read();
     return status&1;
   }
   auto empty() {
     const auto status = lstatus.read();
     return status&2;
   }
   auto almostfull() {
     const auto status = lstatus.read();
     return status&4;
   }
   auto almostempty() {
     const auto status = lstatus.read();
     return status&8;
   }
   auto overflow() {
     const auto status = lstatus.read();
     return status&16;
   }
   auto underflow() {
     const auto status = lstatus.read();
     return status&32;
   }
   auto event(const bool verbose = false, std::string prefix = ""s) {
     uint32_t event = levent.read();
     if (verbose)
       std::cout << prefix << "event=" << std::bitset<6>(event)
         << (event&1 ? " FULL" : "")
         << (event&2 ? " EMPTY" : "")
         << (event&4 ? " ALMOSTFULL" : "")
         << (event&8 ? " ALMOSTEMPTY" : "")
         << (event&16 ? " OVERFLOW" : "")
         << (event&32 ? " UNDERFLOW" : "") << std::endl;
     return event;
   }
   auto check(const bool verbose = false, std::string prefix = ""s) {
     uint32_t fill = lfill.read();
     if (verbose)
       std::cout << prefix << "fill=" << std::dec << fill << std::endl;
     return fill;
   }
   auto fill() const {
     return lfill.read();
   }
   uint32_t clear_fifo(const bool verbose = false) {
     if (verbose)
       std::cout << "clear_fifo" << std::endl;
     auto fill = check();
     while (fill > 0) {
       read(); // ignore return value
       fill = check();
     }
     return fill;
   }
};
