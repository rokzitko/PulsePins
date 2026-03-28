// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Memory-access

#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <exception>
#include <cassert>
#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h> // close

#include "hwlib.h"
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include "hps_0.h"

#include "virt_to_phys.h"

#include "defs.hh"

using namespace std::string_literals;

const size_t de10nano_maxram = 512 * 1000 * 1000UL;

class ram_block {
protected:
   std::uintptr_t addr;
   size_t size;
public:
   ram_block(const std::uintptr_t _addr, const size_t _size, bool verbose = false, std::ostream &f = std::cerr) : addr(_addr), size(_size) {
     assert(size < de10nano_maxram);
     if (verbose)
       f << "size=" << std::dec << size << std::endl;
   }
   ram_block chunk(const int i, const int nr_chunks) {
     const size_t chunk_size = size/nr_chunks;
     return ram_block(addr+i*chunk_size, chunk_size);
   }
   auto get_addr() const {
     return addr;
   }
   auto get_size() const {
     return size;
   }
};

class mm;

#ifdef DEBUG_FPGA_IO
constexpr bool default_loc_debug = true;
#else
constexpr bool default_loc_debug = false;
#endif

// Memory location read/write operations wrapper
class loc
{
 private:
   std::uintptr_t base;
   bool debug = default_loc_debug;
 public:
   loc(std::uintptr_t _base) : base(_base) {
     if (debug)
       std::cout << "loc: base=0x" << std::hex << base << std::endl;
   }
   inline void write(const uint32_t val, const uint32_t offset = 0) const noexcept {
     if (debug)
       try {
         std::cout << "write base=0x" << std::hex << base << " offset=0x"
           << std::hex << offset << " (" << std::dec << offset << ")"
           << " val=0x" << std::hex << val << " (" << std::dec << val << ")" << std::endl; // cast required
       } catch (...) { } // swallow errors
     *(volatile uint32_t *)(base + offset) = val; // volatile prevents the compiler from optimizing away the call
   }
   inline uint32_t read(const uint32_t offset = 0) const noexcept {
     const auto val = *(volatile uint32_t *)(base + offset); // volatile prevents the compiler from optimizing away the call
     if (debug)
       try {
         std::cout << "read base=0x" << std::hex << base << " offset=0x" << std::hex << offset << " (" << std::dec << offset << ")"
         << " val=0x" << std::hex << val << " (" << std::dec << val << ")" << std::endl; // cast required
       } catch (...) { } // swallow errors
     return val;
   }
   auto get_base() const noexcept {
     return base;
   }
   auto get_loc() const noexcept { // same as get_base(), just a different name
     return base;
   }
   auto get_ptr() const noexcept {
     return (void*)base;
   }
   void set_debug(const bool _debug) {
     debug = _debug;
     if (debug) std::cout << "debug=" << debug << std::endl;
   }
};

// Memory-mapped device access
class mm
{
 private:
   std::uintptr_t base;
   std::uintptr_t span;
   std::uintptr_t mask;
   int fd;
   std::uintptr_t virtual_base;
 public:
   mm(std::uintptr_t _base, std::uintptr_t _span) : base(_base), span(_span), mask(_span-1)
   {
     assert(_span >= 1);
     if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1)
       throw std::runtime_error("Could not open /dev/mem");
     auto res = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
     if (res == MAP_FAILED) {
       close(fd);
       throw std::system_error(errno, std::generic_category(), "mmap failed");
     }
     virtual_base = (std::uintptr_t)res;
   }
   auto get_base() const noexcept {
     return base;
   }
   auto get_span() const noexcept {
     return span;
   }
   auto get_mask() const noexcept {
     return mask;
   }
   auto get_address(std::uintptr_t ptr_base = 0, std::uintptr_t shift = 0) const noexcept {
     return virtual_base + ((ptr_base + shift) & mask);
   }
   auto get_ptr(std::uintptr_t ptr_base = 0, std::uintptr_t shift = 0) const noexcept {
     return (void*)get_address(ptr_base, shift);
   }
   auto get_loc(std::uintptr_t ptr_base = 0, std::uintptr_t shift = 0) const noexcept {
     return loc(get_address(ptr_base, shift));
   }
   ~mm() {
     munmap((void*)virtual_base, span); // ignore return value
     close(fd);
   }
};

class on_chip_memory {
 private:
   loc f;
   size_t size; // in bytes
 public:
   on_chip_memory(mm &dev, const std::uintptr_t base, const size_t _size = 0) :
     f(dev.get_loc(base)), size(_size) {}
   void write(const std::uintptr_t addr, const uint32_t v) {
     f.write(v, addr);
   }
   uint32_t read32(const std::uintptr_t addr) {
     return f.read(addr);
   }
   auto begin() const {
     return f.get_ptr();
   }
};
