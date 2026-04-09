// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko
//
// Helpers for configuring the core and internal PLL clocks.
//
// These wrappers are intentionally thin: they translate typed `PllOptions` into the lower-
// level PLL reconfiguration API, apply symbolic preset expansion via `pll_rules.hh`, wait
// for the hardware to settle, and optionally report the resulting clock frequency.

#pragma once

#include <cstdlib>
#include <string>
#include <iostream>

#include "pll.hh"
#include "pll_rules.hh"
#include "parser.hh"
#include "options.hh"

constexpr int pll_delay = 2*1000;  // 2ms delay for things to settle (docs say 500us is worst case)

class pll_core_clk {
public:
  pll core_clk;

  pll_core_clk(mm &dev_lw, std::string name = "pll_core"s) :
    core_clk(dev_lw, PLL_RECONFIG_INT_CLK_BASE, name) {}

  // Program the core clock PLL using the resolved preset/raw string plus optional fine-tuning.
  void set_core_clk(const PllOptions &opts, const Verbosity &v) {
    core_clk.set_from_string(applyReplacement(opts.profile, pll_rules));
    if (opts.charge_pump)
      core_clk.set_charge_pump(*opts.charge_pump);
    if (opts.bandwidth)
      core_clk.set_bandwidth(*opts.bandwidth);
    usleep(pll_delay);
    if (v.verbose)
      std::cout << "core_clk=" << with_underscores(int(core_clk.get_freq(0))) << "Hz" << std::endl;
    if (v.veryverbose)
      core_clk.report();
  }
};

class pll_int_clk {
public:
  pll int_clk;

  pll_int_clk(mm &dev_lw, std::string name = "pll_int"s) :
    int_clk(dev_lw, PLL_RECONFIG_INT_CLK_BASE, name) {}

  // Program the internal candidate streamer clock PLL.
  void set_int_clk(const PllOptions &opts, const Verbosity &v) {
    int_clk.set_from_string(applyReplacement(opts.profile, pll_rules));
    if (opts.charge_pump)
      int_clk.set_charge_pump(*opts.charge_pump);
    if (opts.bandwidth)
      int_clk.set_bandwidth(*opts.bandwidth);
    usleep(pll_delay);
    if (v.verbose)
      std::cout << "int_clk=" << with_underscores(int(int_clk.get_freq(0))) << "Hz" << std::endl;
    if (v.veryverbose)
      int_clk.report();
  }
};
