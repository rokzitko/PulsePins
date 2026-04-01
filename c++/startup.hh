// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <iostream>
#include <sys/mman.h>

#include "ppmisc.hh"
#include "realtime.hh"
#include "fpga.hh"

inline RealtimeScheduler bootstrap_process(const Verbosity &v, const int version)
{
  check_version(version);
  mlockall(MCL_CURRENT | MCL_FUTURE);
  RealtimeScheduler rt;
  if (v.veryverbose)
    std::cout << "Scheduler: " << rt.report() << std::endl;
  rstmgr rm;
  rm.s2f_reset();
  return rt;
}

inline void apply_fpga_startup_policy(FPGA &fpga, const InputParser &input, const bool oe = false)
{
  fpga.set_clk(resolve_clock_selection_options(input));
  fpga.pll_core.set_core_clk(resolve_core_pll_options(input), fpga.v);
  fpga.pll_int.set_int_clk(resolve_int_pll_options(input), fpga.v);
  if (oe)
    fpga.output_enable(true);
  fpga.blink_led();
  if (fpga.v.veryverbose) {
    fpga.mgr.status();
    fpga.status();
  }
}
