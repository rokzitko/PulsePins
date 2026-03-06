// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#pragma once

#include <cstdlib>
#include <string>
#include <iostream>

#include "fpga.hh"
#include "pll.hh"
#include "pll_rules.hh"
#include "parser.hh"

// PLL clocks. It uses environment variables PP_(CORE|INT)_PLL, or the -(core|int)_pll command line switch.
// Command line switch takes precedence.

constexpr int pll_delay = 2*1000;  // 2ms delay for things to settle (docs say 500us is worst case)

class pll_core_clk {
 public:
   const FPGA &fpga;
   pll core_clk;

   pll_core_clk(const FPGA &_fpga) :
     fpga(_fpga),
     core_clk(fpga.dev_lw, PLL_RECONFIG_INT_CLK_BASE) {}

   void set_core_clk(const InputParser &input, const Verbosity &v) {
     core_clk.set_from_string(applyReplacement(input.get_string("-core_pll", get_env("PP_CORE_PLL")), pll_rules));
     if (input.exists("-core_pll_charge_pump"))
       core_clk.set_charge_pump(input.get_uint32("-core_pll_charge_pump", 1));
     if (input.exists("-core_pll_bandwidth"))
       core_clk.set_bandwidth(input.get_uint32("-core_pll_bandwidth", 7));
     usleep(pll_delay);
     if (v.verbose)
       std::cout << "core_clk=" << with_underscores(int(core_clk.get_freq(0))) << "Hz" << std::endl;
     if (v.veryverbose)
       core_clk.report();
   }
};

class pll_int_clk {
 public:
   const FPGA &fpga;
   pll int_clk;

   pll_int_clk(const FPGA &_fpga) :
     fpga(_fpga),
     int_clk(fpga.dev_lw, PLL_RECONFIG_INT_CLK_BASE) {}

   void set_int_clk(const InputParser &input, const Verbosity &v) {
     int_clk.set_from_string(applyReplacement(input.get_string("-int_pll", get_env("PP_INT_PLL")), pll_rules));
     if (input.exists("-int_pll_charge_pump"))
       int_clk.set_charge_pump(input.get_uint32("-int_pll_charge_pump", 1));
     if (input.exists("-int_pll_bandwidth"))
       int_clk.set_bandwidth(input.get_uint32("-int_pll_bandwidth", 7));
     usleep(pll_delay);
     if (v.verbose)
       std::cout << "int_clk=" << with_underscores(int(int_clk.get_freq(0))) << "Hz" << std::endl;
     if (v.veryverbose)
       int_clk.report();
   }
};
