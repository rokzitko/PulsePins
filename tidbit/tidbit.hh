// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Common include file for Rok Zitko's FPGA designs
// RZ, 2022-2025

#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <functional>
#include <vector>
#include <tuple>
#include <list>
#include <deque>
#include <bitset>
#include <chrono>
#include <thread>
#include <mutex>
#include <numeric>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cerrno>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <system_error>

#include "hwlib.h"
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include "hps_0.h"

#ifndef DEVICE
#define DEVICE DE10NANO
#define DE10NANO
#endif

using namespace std::string_literals;

#include "bitops.hh"
#include "memory.hh"

inline uint32_t chars_to_uint32(const char buffer[4])
{
  const uint32_t num =
    (uint32_t)buffer[0] << 24 |
    (uint32_t)buffer[1] << 16 |
    (uint32_t)buffer[2] << 8  |
    (uint32_t)buffer[3];
  return num;
};

inline bool compare(const uint32_t val, const uint32_t expected, std::ostream &f = std::cout)
{
  if (val != expected) {
    f << "mismatch: received=0x" << std::hex << val << " (" << std::dec << val << ")"
      << " expected=0x" << std::hex << expected <<" (" << std::dec << expected << ")" << std::endl;
  }
  return val != expected;
};

inline bool compare(const uint32_t val, const char expected[4], std::ostream &f = std::cout)
{
  return compare(val, chars_to_uint32(expected));
};

void hello_msg(int tidbit, std::ostream &f = std::cerr) {
  f << "test for tidbit" << tidbit << ", Rok Zitko 2022-2025" << std::endl;
  f << "compiled on " << __DATE__ << " " << __TIME__ << std::endl;
}

#include "sysid.hh"
#include "pio.hh"
#include "st.hh"
#include "pll.hh"
#include "fifo.hh"
#include "dma.hh"

class hpsled
{
 protected:
   loc data, dir;
   const static uint32_t LED_MASK = (1 << 24);
   const static uintptr_t HPS_GPIO1_BASE = 0xFF709000;
 public:
   hpsled(mm &dev, std::uintptr_t base = HPS_GPIO1_BASE) :
     data(dev.get_loc(base, 0)),
     dir (dev.get_loc(base, 4)) { dir.write(dir.read() | LED_MASK); }

   void on() {
     data.write(data.read() | LED_MASK);
   }

   void off() {
     data.write(data.read() & ~LED_MASK);
   }
};

// #define ALT_RSTMGR_MISCMODRST_OFST        0x20
// cv_5v4-19-2-683126-705349, page 4-30
class rstmgr {
protected:
   mm dev_mgr;
   static constexpr off_t PHYS_RSTMGR_BASE = 0xFFD05000; // Reset Manager physical base
   static constexpr size_t MAP_SPAN_BYTES  = 0x1000;
   loc rstmgr_miscmodrst;

public:
   rstmgr(const std::uintptr_t rstmgr_h2f_base = ALT_RSTMGR_OFST) : // 0xFFD05000
     dev_mgr(PHYS_RSTMGR_BASE, MAP_SPAN_BYTES),
     rstmgr_miscmodrst(dev_mgr.get_loc(rstmgr_h2f_base, 0x20)) {}

   void s2f_reset(const bool verbose = true) {
     if (verbose) std::cout << "Performing FPGA reset." << std::endl;
     auto state = rstmgr_miscmodrst.read();
     BIT_SET(state, 6);
     rstmgr_miscmodrst.write(state);
     usleep(10);
     BIT_CLEAR(state, 6);
     rstmgr_miscmodrst.write(state);
   }

   void s2f_hold_reset(const bool verbose = true) {
     if (verbose) std::cout << "Putting FPGA in reset." << std::endl;
     auto state = rstmgr_miscmodrst.read();
     BIT_SET(state, 6);
     rstmgr_miscmodrst.write(state);
   }

   void s2f_release_reset(const bool verbose = true) {
     if (verbose) std::cout << "Releasing FPGA from reset." << std::endl;
     auto state = rstmgr_miscmodrst.read();
     BIT_CLEAR(state, 6);
     rstmgr_miscmodrst.write(state);
   }
};

class timeit {
 private:
   using tp = std::chrono::time_point<std::chrono::steady_clock>;
   tp begin = std::chrono::steady_clock::now();
   uint64_t size = 0;
 public:
   timeit() {};
   timeit(uint64_t _size) : size(_size) {};
   ~timeit() {
     const auto now = std::chrono::steady_clock::now();
     const double microsecs = std::chrono::duration_cast<std::chrono::microseconds>(now - begin).count();
     std::cout << "elapsed=" << std::fixed << microsecs/1000000UL << " s" << std::endl;
     if (size > 0) {
       const double rate_bytes = double(size)/microsecs; // result in units of Mbytes/s
       const double rate_bits = rate_bytes*8; // result in units of Mbits/s
       std::cout << "size=" << size << " bytes: rate=" << std::fixed << rate_bits << " Mbit/s = " << rate_bytes << "MB/s" << std::endl;
     }
   }
};
