// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Memory-access

#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <exception>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
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
     if (size >= de10nano_maxram) {
       std::ostringstream ss;
       ss << "ram_block size exceeds DE10-Nano RAM: size=" << size << " max=" << de10nano_maxram;
       throw std::out_of_range(ss.str());
     }
     if (verbose)
       f << "size=" << std::dec << size << std::endl;
   }
   ram_block chunk(const int i, const int nr_chunks) const {
     if (nr_chunks <= 0)
       throw std::invalid_argument("ram_block chunk count must be positive");
     if (i < 0 || i >= nr_chunks)
       throw std::out_of_range("ram_block chunk index is outside chunk range");
     const size_t chunk_count = static_cast<size_t>(nr_chunks);
     const size_t chunk_index = static_cast<size_t>(i);
     const size_t chunk_size = size/chunk_count;
     if (chunk_size == 0)
       throw std::invalid_argument("ram_block chunk size must be nonzero");
     if (chunk_index > (UINTPTR_MAX - addr)/chunk_size)
       throw std::out_of_range("ram_block chunk address overflows uintptr_t");
     return ram_block(addr + chunk_index*chunk_size, chunk_size);
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
   std::string name;
#ifdef PP_DEBUG_LOC_BOUNDS
   bool bounds_check_enabled = false;
   std::uintptr_t bounds_size = 0;

   void set_bounds(const mm &dev, std::uintptr_t ptr_base, std::uintptr_t shift) noexcept;
   void check_bounds(const char *operation, uint32_t offset) const noexcept;
#endif
 public:
   loc(std::uintptr_t _base, [[maybe_unused]] std::string _name = "") :
     base(_base),
     name(_name)
     {
#ifdef DEBUG_CONSTR
       std::cout << "loc " << name
         << " base=0x" << std::hex << base << std::endl;
#endif
     }
   loc(const mm &dev, std::uintptr_t ptr_base, std::string _name = "");
   loc(const mm &dev, std::uintptr_t ptr_base, std::uintptr_t shift, std::string _name = "");
   inline void write(const uint32_t val, const uint32_t offset = 0) const noexcept {
#ifdef PP_DEBUG_LOC_BOUNDS
     check_bounds("write", offset);
#endif
     if (debug)
       try {
         std::cout << "write " << name
           << " base=0x" << std::hex << base << " offset=0x"
           << std::hex << offset << " (" << std::dec << offset << ")"
           << " val=0x" << std::hex << val << " (" << std::dec << val << ")" << std::endl; // cast required
       } catch (...) { } // swallow errors
     *(volatile uint32_t *)(base + offset) = val; // volatile prevents the compiler from optimizing away the call
   }
   inline uint32_t read(const uint32_t offset = 0) const noexcept {
#ifdef PP_DEBUG_LOC_BOUNDS
     check_bounds("read", offset);
#endif
     const auto val = *(volatile uint32_t *)(base + offset); // volatile prevents the compiler from optimizing away the call
     if (debug)
       try {
         std::cout << "read " << name
           << " base=0x" << std::hex << base << " offset=0x" << std::hex << offset << " (" << std::dec << offset << ")"
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
   static std::uintptr_t checked_span(std::uintptr_t span) {
     if (span < 1)
       throw std::invalid_argument("mm span must be at least one byte");
     return span;
   }

   std::uintptr_t base;
   std::uintptr_t span;
   std::uintptr_t mask;
   int fd;
   std::uintptr_t virtual_base;
 public:
   mm(std::uintptr_t _base,
      std::uintptr_t _span,
      [[maybe_unused]] std::string name = ""s
     ) :
     base(_base),
     span(checked_span(_span)),
     mask(span-1)
   {
     if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1)
       throw std::system_error(errno, std::generic_category(), "open(/dev/mem) failed");
     auto res = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
     if (res == MAP_FAILED) {
       const int mmap_errno = errno;
       close(fd);
       std::ostringstream ss;
       ss << "mmap(/dev/mem) failed: base=0x" << std::hex << base << " span=0x" << span;
       throw std::system_error(mmap_errno, std::generic_category(), ss.str());
     }
     virtual_base = (std::uintptr_t)res;
#ifdef DEBUG_CONSTR
     std::cout << "mm " << name
       << " base=0x" << std::hex << base
       << " span=0x" << std::hex << span
       << " virtual_base=0x" << std::hex << virtual_base << std::endl;
#endif
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
   auto get_addr(std::uintptr_t ptr_base = 0, std::uintptr_t shift = 0) const noexcept {
     return virtual_base + ((ptr_base + shift) & mask);
   }
   bool contains(std::uintptr_t ptr_base = 0, std::uintptr_t shift = 0, std::uintptr_t bytes = sizeof(uint32_t)) const noexcept {
     auto offset = ptr_base;
     if (ptr_base >= base && ptr_base - base < span)
       offset = ptr_base - base;
     if (shift > UINTPTR_MAX - offset)
       return false;
     offset += shift;
     return offset <= span && bytes <= span - offset;
   }
   auto checked_addr(std::uintptr_t ptr_base = 0, std::uintptr_t shift = 0, std::uintptr_t bytes = sizeof(uint32_t)) const {
     if (!contains(ptr_base, shift, bytes)) {
       std::ostringstream ss;
       ss << "mm::checked_addr outside mapped span: map_base=0x" << std::hex << base
          << " span=0x" << span
          << " ptr_base=0x" << ptr_base
          << " shift=0x" << shift
          << " bytes=0x" << bytes;
       throw std::out_of_range(ss.str());
     }
     auto offset = ptr_base;
     if (ptr_base >= base && ptr_base - base < span)
       offset = ptr_base - base;
     offset += shift;
     return virtual_base + offset;
   }
   auto get_ptr(std::uintptr_t ptr_base = 0, std::uintptr_t shift = 0) const noexcept {
     return (void*)get_addr(ptr_base, shift);
   }
   auto get_loc(std::uintptr_t ptr_base = 0, std::uintptr_t shift = 0) const {
     return loc(*this, ptr_base, shift);
   }
   mm(const mm&) = delete;
   mm& operator=(const mm&) = delete;
   mm(mm&&) = delete;
   mm& operator=(mm&&) = delete;
   ~mm() {
     if (munmap((void*)virtual_base, span) != 0)
       std::cerr << "Warning: munmap failed: " << std::strerror(errno) << std::endl;
     if (close(fd) != 0)
       std::cerr << "Warning: close(/dev/mem) failed: " << std::strerror(errno) << std::endl;
   }
};

inline loc::loc(const mm &dev, std::uintptr_t ptr_base, std::string _name) :
  loc(dev.checked_addr(ptr_base), _name) {
#ifdef PP_DEBUG_LOC_BOUNDS
  set_bounds(dev, ptr_base, 0);
#endif
}

inline loc::loc(const mm &dev, std::uintptr_t ptr_base, std::uintptr_t shift, std::string _name) :
  loc(dev.checked_addr(ptr_base, shift), _name) {
#ifdef PP_DEBUG_LOC_BOUNDS
  set_bounds(dev, ptr_base, shift);
#endif
}

#ifdef PP_DEBUG_LOC_BOUNDS
inline void loc::set_bounds(const mm &dev, std::uintptr_t ptr_base, std::uintptr_t shift) noexcept {
  const auto map_base = dev.get_base();
  const auto map_span = dev.get_span();
  auto offset = ptr_base;
  if (ptr_base >= map_base && ptr_base - map_base < map_span)
    offset = ptr_base - map_base;
  if (shift > UINTPTR_MAX - offset)
    offset = map_span + 1;
  else
    offset += shift;
  bounds_check_enabled = true;
  bounds_size = offset <= map_span ? map_span - offset : 0;
}

inline void loc::check_bounds(const char *operation, uint32_t offset) const noexcept {
  if (!bounds_check_enabled)
    return;
  constexpr std::uintptr_t access_size = sizeof(uint32_t);
  const std::uintptr_t access_offset = offset;
  if (access_offset <= bounds_size && access_size <= bounds_size - access_offset)
    return;
  std::fprintf(stderr,
               "loc bounds check failed: %s %s base=0x%jx offset=0x%jx bytes=%ju remaining=%ju\n",
               operation,
               name.c_str(),
               static_cast<uintmax_t>(base),
               static_cast<uintmax_t>(access_offset),
               static_cast<uintmax_t>(access_size),
               static_cast<uintmax_t>(bounds_size));
  std::abort();
}
#endif

class on_chip_memory {
 private:
    loc f;
 public:
    on_chip_memory(mm &dev, const std::uintptr_t base, const size_t = 0) :
      f(dev.get_loc(base)) {}
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
