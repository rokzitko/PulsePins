#pragma once

#include <cstdio>

#include "fpga.hh"

// Note: the reset is not performed here if the clock is not explicitly specified by either
// -int_clk or -ext_clk switch.
void set_clk(const InputParser &input, FPGA &fpga)
{
  rstmgr rm;
  if (envVarExists("PP_INT_CLK") || input.exists("-int_clk")) {
    rm.s2f_hold_reset();
    fpga.sel_clk_int();
    rm.s2f_release_reset();
  }
  if (envVarExists("PP_EXT_CLK") || input.exists("-ext_clk")) {
    rm.s2f_hold_reset();
    fpga.sel_clk_ext();
    rm.s2f_release_reset();
  }
  if (envVarExists("PP_CLK") || input.exists("-clk")) {
    int val = parse_value(input, "-clk", get_env("PP_CLK"));
    if (val < 0 || val > 3)
      std::cerr << "Invalid sel_clk value " << val << "." << std::endl;
    rm.s2f_hold_reset();
    fpga.sel_clk(val);
    rm.s2f_release_reset();
  }
}
