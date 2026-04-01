// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko
//
// Helpers for configuring the core and internal PLL clocks.

#pragma once

#include <cstdlib>
#include <string>
#include <iostream>

#include "pll.hh"
#include "pll_rules.hh"
#include "parser.hh"
#include "options.hh"

// PLL clocks. It uses environment variables PP_(CORE|INT)_PLL, or the -(core|int)_pll command line switch.
// Command line switch takes precedence.

constexpr int pll_delay = 2*1000;  // 2ms delay for things to settle (docs say 500us is worst case)

class pll_core_clk {
 public:
   pll core_clk;

   pll_core_clk(mm &dev_lw) :
     core_clk(dev_lw, PLL_RECONFIG_INT_CLK_BASE) {}

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

   pll_int_clk(mm &dev_lw) :
     int_clk(dev_lw, PLL_RECONFIG_INT_CLK_BASE) {}

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
