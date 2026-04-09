// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Programmable input output

#pragma once

#include <thread>

#include "memory.hh"

// PIO core
class pio
{
 protected:
   loc data;
 public:
   pio(mm &dev, std::uintptr_t base, std::string name = "pio"s) :
     data(dev.get_addr(base), name + "/data") {}
};

class pio_out: public pio {
 using pio::pio;
 public:
   void write(const uint32_t q) {
     data.write(q);
   }
   uint32_t read() {
     return data.read();
   }
};

//  <parameter name="bitModifyingOutReg" value="true" />
class pio_out_bits: public pio {
 using pio::pio;
 protected:
   loc bset, bclear;
 public:
   pio_out_bits(mm &dev, std::uintptr_t base, std::string name = "pio"s) :
     pio(dev, base),
     bset(dev.get_addr(base, 0x04*4),   name + "/bset"),
     bclear(dev.get_addr(base, 0x04*5), name + "/bclear") {}
   void write(const uint32_t q) {
     data.write(q);
   }
   // Set selected bits
   void set(const uint32_t q) {
     bset.write(q);
   }
   // Clear selected bits
   void clear(const uint32_t q) {
     bclear.write(q);
   }
   // Set bit at position i (0-based) to logical value 'value'
   void write_at(const int i, const bool value) {
     if (value)
       set(1UL<<i);
     else
       clear(1UL<<i);
   }
   template <typename T> void set_for(const uint32_t n, T duration) {
     set(n);
     std::this_thread::sleep_for(duration);
     clear(n);
   }
   uint32_t read() {
     return data.read();
   }
};

constexpr std::uintptr_t interruptmask_offset = 2*sizeof(uint32_t);
constexpr std::uintptr_t edgecapture_offset   = 3*sizeof(uint32_t);

class pio_in: public pio {
 using pio::pio;
 private:
  loc interruptmask;
  loc edge;
 public:
   pio_in(mm &dev, std::uintptr_t base, std::string name = "pio"s) :
     pio(dev, base),
     interruptmask(dev.get_addr(base, interruptmask_offset), name + "/int"),
     edge(dev.get_addr(base, edgecapture_offset),            name + "/edge")
   {}
   uint32_t read() {
     return data.read();
   }
   void mask(uint32_t _mask = 0x0) {
     interruptmask.write(_mask);
   }
   auto edgecapture() {
     return edge.read();
   }
   void edgecapture_clear(const uint32_t mask = 0xffffffff) {
     edge.write(mask);
   }
};
