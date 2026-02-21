// SPDX-License-Identifier: MIT
// Copyright (c) 2025,2026 Rok Zitko

// Low-level interface to FPGA; holds memory-map objects

#pragma once

#include <iostream>
#include <iomanip>
#include <bitset>
#include <unistd.h>

#include "socal/alt_fpgamgr.h"

#ifndef HPS_REGS_OFST
 #define HPS_REGS_OFST  0xFF700000
#endif
#ifndef HPS_REGS_RANGE
 #define HPS_REGS_RANGE 0x00010000
#endif

#define ALT_SYSMGR_BASE          0xFFD08000
#define ALT_SYSMGR_RANGE         0x4000
#define HPS_TO_FPGA_GP_OUT_OFST  0x400
#define HPS_TO_FPGA_GP_IN_OFST   0x410  // 0xFFD08410 absolute

#define ALT_FPGAMGR_BASE         0xFF706000
#define ALT_FPGAMGR_RANGE        0x1000

static_assert(ALT_FPGAMGR_BASE == ALT_FPGAMGR_OFST);

#include "tidbit.hh"
#include "memory.hh"
#include "verbosity.hh"

class MGR {
 private:
   loc stat;
   loc gpin, gpout; // ARM<->FPGA general purpose 32-bit I/O ports, gp_io and gp_out
   const Verbosity &v;

 public:
  MGR(mm &dev_fpgamgr, const Verbosity &_v) :
     stat(dev_fpgamgr.get_loc(ALT_FPGAMGR_BASE, ALT_FPGAMGR_STAT_OFST)),
     gpin(dev_fpgamgr.get_loc(ALT_FPGAMGR_BASE, ALT_FPGAMGR_GPI_OFST)),
     gpout(dev_fpgamgr.get_loc(ALT_FPGAMGR_BASE, ALT_FPGAMGR_GPO_OFST)),
     v(_v)
     {}

   // Read status bits in gp_in port
   auto status() const {
     const auto s = stat.read();
     if (v.verbose) {
       // see socal/alt_fpgamgr.h
       std::cout << "FPGA MGR status=0x" << std::hex << s;
       const auto mode = ALT_FPGAMGR_STAT_MOD_GET(s);
       std::cout << " mode=" << std::dec << mode;
       switch (mode) {
       case ALT_FPGAMGR_STAT_MOD_E_FPGAOFF:
         std::cout << " (powered off)";
         break;
       case ALT_FPGAMGR_STAT_MOD_E_RSTPHASE:
         std::cout << " (reset phase)";
         break;
       case ALT_FPGAMGR_STAT_MOD_E_CFGPHASE:
         std::cout << " (configuration phase)";
         break;
       case ALT_FPGAMGR_STAT_MOD_E_INITPHASE:
         std::cout << " (init phase)";
         break;
       case ALT_FPGAMGR_STAT_MOD_E_USERMOD:
         std::cout << " (user mode)";
         break;
       case  ALT_FPGAMGR_STAT_MOD_E_UNKNOWN:
         std::cout << " (unknown)";
         break;
       }
       std::cout << " msel=" << std::dec << ALT_FPGAMGR_STAT_MSEL_GET(s) << std::endl;
     }
     return s;
   }

   void gpio_write(uint32_t x) {
     gpout.write(x);
   }

   uint32_t gpio_read() const {
     return gpin.read();
   }
};

class Elapsed {
 private:
   pio_in p;

 public:
   Elapsed(mm &dev, const std::uintptr_t base) :
     p(dev, base) {}

   uint32_t seconds() {
     return p.read();
   }
};

// Low-level interfacing to FPGA
class FPGA {
 public:
   mm dev_lw, dev_h2f, dev_hps, dev_sysmgr, dev_fpgamgr;
   hpsled led;
   MGR mgr;
   uint32_t cfg = 0; // current value on gp_out (configuration bits)
   pio_out_bits pio_cfg; // oe signal
   Elapsed elapsed;
   const InputParser &input;
   const Verbosity &v;

   FPGA(const InputParser &_input, const Verbosity &_v, const bool oe = true) :
     dev_lw(LWHPSFPGA_OFST, LWH2F_RANGE),
     dev_h2f(HPSFPGA_OFST, H2F_RANGE),
     dev_hps(HPS_REGS_OFST, HPS_REGS_RANGE),
     dev_sysmgr(ALT_SYSMGR_BASE, ALT_SYSMGR_RANGE),
     dev_fpgamgr(ALT_FPGAMGR_BASE, ALT_FPGAMGR_RANGE),
     led(dev_hps),
     mgr(dev_fpgamgr, _v),
     pio_cfg(dev_lw, PIO_CFG_BASE),
     elapsed(dev_lw, PIO_ELAPSED_BASE),
     input(_input),
     v(_v)
     {
       if (oe) // if false, leave as is
         output_enable(true);
       // blink on-board diagnostic LED on startup
       led.on();
       usleep(10*1000); // 10ms
       led.off();
     }

   ~FPGA() {
      if (v.veryverbose) {
        std::cout << "Elapsed time (since last reset): " << elapsed.seconds() << std::endl;
      }
   }

   auto status() const {
     auto s = mgr.gpio_read();
     if (v.verbose) {
       std::cout << "gpio in=0x" << std::hex << s << " ";
       if (s & (1 << 0)) std::cout << "[core_clk pll locked] ";
       if (s & (1 << 1)) std::cout << "[int_clk pll locked] ";
       if (s & (1 << 2)) std::cout << "[ext_clk pll locked] ";
       if (s & (1 << 3)) std::cout << "[core_clk pll ready] ";
       if (s & (1 << 4)) std::cout << "[sys reset hold] ";
       if (s & (1 << 5)) std::cout << "[activity] ";
       if (s & (1 << 6)) std::cout << "[reset altera] ";
       if (s & (1 << 7)) std::cout << "[reset ai] ";
       std::cout << std::endl;
     }
     return s;
   }

   void output_enable(const bool oe = false) {
     pio_cfg.write_at(0, oe);
   }

   void sel_clk(uint32_t sel) {
     sel &= 3; // only bits 0 and 1 are relevant for sel_clk
     cfg = (cfg | 3) + sel;
     if (v.verbose) {
       std::cout << "Setting clock select bits (sel_clk) to " << std::bitset<2>(sel) << ".";
       switch (sel) {
       case 0:
         std::cout << " streamer_clk=int_clk" << std::endl;
         break;
       case 1:
         std::cout << " streamer_clk=ext_clk" << std::endl;
         break;
       default:
         std::cout << " WARNING: invalid setting." << std::endl;
       }
     }
     mgr.gpio_write(cfg);
   }

   void sel_clk_int() {
     sel_clk(0);
   }

   void sel_clk_ext() {
     sel_clk(1);
   }
};
