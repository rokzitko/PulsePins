// SPDX-License-Identifier: MIT
// Copyright (c) 2025,2026 Rok Zitko

// Top-level ARM-side ownership wrapper for FPGA-facing resources.
//
// `FPGA` is the main host-side integration object. It owns the core memory maps, top-level
// control/status GPIO, trigger helpers, PLL helpers, and the cached measured streamer-clock
// frequency used by timing-sensitive host logic.
//
// Architectural overview lives in `c++/README.md`, `docs/docs/cpp.md`, and
// `docs/docs/clock_domain.md`.

#pragma once

// Clock mapping needs to be checked using pptool: e.g. pptool -clk 0 -int_pll 25M, pptool -clk 3 -int_pll 25M, etc.
#define SELECT_CLK_CLEAN

//#ifdef SELECT_CLK
//  static const int ch_ext = 2;
//  static const int ch_int = 0;
//#endif
#ifdef SELECT_CLK_CLEAN
  static const int ch_ext = 2;
  static const int ch_int = 3;
#endif

#include <iostream>
#include <iomanip>
#include <bitset>
#include <unistd.h>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include "socal/alt_fpgamgr.h"
#include "hps_0.h"
#include "ppmisc.hh"

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
#include "trigger_int.hh"
#include "trigger_ext.hh"
#include "pll_clk.hh"
#include "pll_rules.hh"
#include "options.hh"

class MGR {
private:
  loc stat;
  loc gpin, gpout; // ARM<->FPGA general purpose 32-bit I/O ports, gp_io and gp_out
  const Verbosity &v;

public:
  MGR(mm &dev_fpgamgr, const Verbosity &_v) :
    stat(dev_fpgamgr.get_addr(ALT_FPGAMGR_BASE, ALT_FPGAMGR_STAT_OFST), "stat"),
    gpin(dev_fpgamgr.get_addr(ALT_FPGAMGR_BASE, ALT_FPGAMGR_GPI_OFST), "gpin"),
    gpout(dev_fpgamgr.get_addr(ALT_FPGAMGR_BASE, ALT_FPGAMGR_GPO_OFST), "gpout"),
    v(_v)
    {}

  // Decode the top-level FPGA manager status register and print it when verbosity allows.
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
  Elapsed(mm &dev, const std::uintptr_t base, std::string name = "elapsed"s) :
    p(dev, base, name) {}

  uint32_t seconds() {
    return p.read();
  }
};

// Top-level FPGA resource owner used by host-side tools.
class FPGA {
public:
  mm dev_lw, dev_h2f, dev_hps, dev_sysmgr, dev_fpgamgr;
  hpsled led;
  MGR mgr;
  uint32_t cfg = 0; // current value on gp_out (configuration bits)
  pio_out_bits pio_cfg; // oe signal
  Elapsed elapsed;
  const Verbosity &v;
  inline static std::atomic<bool> constructed {false};
  std::mutex m;
  trigger_int trig_int;
  trigger_ext trig_ext;
  double streamer_clk = -1.0; // measured streamer clock frequency in Hz
  pll_core_clk pll_core;
  pll_int_clk pll_int;

  FPGA(const Verbosity &_v) :
    dev_lw(LWHPSFPGA_OFST, LWH2F_RANGE, "lw"),
    dev_h2f(HPSFPGA_OFST, H2F_RANGE, "h2f"),
    dev_hps(HPS_REGS_OFST, HPS_REGS_RANGE, "hps"),
    dev_sysmgr(ALT_SYSMGR_BASE, ALT_SYSMGR_RANGE, "sysmgr"),
    dev_fpgamgr(ALT_FPGAMGR_BASE, ALT_FPGAMGR_RANGE, "fpgamgr"),
    led(dev_hps),
    mgr(dev_fpgamgr, _v),
    pio_cfg(dev_lw, PIO_CFG_BASE, "pio_cfg"),
    elapsed(dev_lw, PIO_ELAPSED_BASE, "elapsed"),
    v(_v),
    trig_int(dev_lw, PIO_TRIG_INT_BASE, "trig_int"),
    trig_ext(dev_lw, PIO_TRIG_MONITOR_BASE, "trig_ext"),
    pll_core(dev_lw, "pll_core"),
    pll_int(dev_lw, "pll_int")
    {
      // The host software assumes one coherent owner of the memory maps and top-level
      // control bits, so creating multiple `FPGA` instances is treated as a logic error.
      bool expected = false;
      if (!constructed.compare_exchange_strong(expected, true,
                                                std::memory_order_acq_rel)) {
        throw std::logic_error("FPGA: second instance construction attempted");
      }
    }

  void blink_led() {
    led.on();
    usleep(10*1000); // 10ms
    led.off();
  }

  ~FPGA() noexcept {
    if (v.veryverbose) {
      std::cout << "Elapsed time (since last reset): " << elapsed.seconds() << std::endl;
    }
    constructed.store(false, std::memory_order_release);
  }

  FPGA(const FPGA&)            = delete;
  FPGA& operator=(const FPGA&) = delete;
  FPGA(FPGA&&)                 = delete;
  FPGA& operator=(FPGA&&)      = delete;

  uint32_t status() const {
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

  void output_enable(const bool oe = true) {
    pio_cfg.write_at(0, oe);
  }

  // Apply streamer clock-source selection. Reset is only pulsed when the caller explicitly
  // requested a source change through the resolved options object.
  void set_clk(const ClockSelectionOptions &opts) {
    rstmgr rm;
    if (opts.source == StreamerClockSource::internal) {
      rm.s2f_hold_reset();
      sel_clk_int();
      rm.s2f_release_reset();
    }
    if (opts.source == StreamerClockSource::external) {
      rm.s2f_hold_reset();
      sel_clk_ext();
      rm.s2f_release_reset();
    }
    if (opts.source == StreamerClockSource::raw_select) {
      const auto val = int(opts.raw_select.value_or(0));
      if (val < 0 || val > 3)
        std::cerr << "Invalid sel_clk value " << val << "." << std::endl;
      rm.s2f_hold_reset();
      sel_clk(val);
      rm.s2f_release_reset();
    }
  }

  void sel_clk(uint32_t sel) {
    // Only the low two configuration bits currently map to the top-level clock selector.
    sel &= 3; // only bits 0 and 1 are relevant for sel_clk
    cfg = (cfg & ~uint32_t(3)) | sel;
    if (v.verbose) {
      std::cout << "Setting clock select bits (sel_clk) to " << std::bitset<2>(sel) << ".";
      switch (sel) {
      case ch_ext:
        std::cout << " streamer_clk=ext_clk" << std::endl;
        break;
      case ch_int:
        std::cout << " streamer_clk=int_clk" << std::endl;
        break;
      default:
        std::cout << " WARNING: invalid clock-select setting." << std::endl;
      }
    }
    mgr.gpio_write(cfg);
  }

  void sel_clk_ext() {
    sel_clk(ch_ext);
  }

  void sel_clk_int() {
    sel_clk(ch_int);
  }

  // Global lock guarding coordinated access to shared top-level FPGA state.
  void lock() {
    m.lock();
  }

  void unlock() {
    m.unlock();
  }

  auto acquire_lock() {
    return std::unique_lock<std::mutex>(m);
  }

  void set_streamer_clk(const double hz) {
    if (hz < 0.0)
      throw std::runtime_error("Streamer clock frequency should be a positive quantity.");
    streamer_clk = hz;
  }

  // Streaming clock frequency [Hz], as measured by the frequency meter.
  double streamer_freq() const {
    if (streamer_clk < 0.0)
      throw std::runtime_error("Streamer clock not measured yet.");
    return streamer_clk;
  }

  // Streaming clock period [s]
  double streamer_period() const {
    return 1.0/streamer_freq();
  }

  void wait_for_N_streamer_clk_periods(int N) const {
    uint32_t delay = 1000000UL * streamer_period(); // in microseconds
    for (int i = 0; i < N; i++)
      usleep(delay);
  }
};
