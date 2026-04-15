// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Common process-startup helpers for PulsePins host executables.
//
// This header centralizes the runtime policy shared by `pptool`-style programs:
//   - process-level bootstrap such as version checks, mlock, and reset-manager setup
//   - FPGA startup policy such as clock-source selection, PLL programming, and optional
//     output-enable handling
//
// Architectural overview lives in `c++/README.md`, `docs/docs/cpp.md`, and
// `docs/docs/pptool.md`.

#pragma once

#include <cerrno>
#include <iostream>
#include <system_error>
#include <sys/mman.h>

#include "options.hh"
#include "ppmisc.hh"
#include "realtime.hh"
#include "fpga.hh"

inline RealtimeScheduler bootstrap_process(const Verbosity &v, const int version)
{
  // Process bootstrap is intentionally separated from FPGA startup so future tools can
  // reuse one policy without necessarily applying the other.
  check_version(version);
  if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
    throw std::system_error(errno, std::generic_category(), "mlockall failed");
  RealtimeScheduler rt;
  if (v.veryverbose)
    std::cout << "Scheduler: " << rt.report() << std::endl;
  rstmgr rm;
  rm.s2f_reset();
  return rt;
}

inline void apply_fpga_startup_policy(FPGA &fpga,
                                      const ClockSelectionOptions &clock_selection,
                                      const PllOptions &core_pll,
                                      const PllOptions &int_pll,
                                      const bool oe = false);

inline void apply_fpga_startup_policy(FPGA &fpga, const InputParser &input, const bool oe = false)
{
  apply_fpga_startup_policy(
    fpga,
    resolve_clock_selection_options(input),
    resolve_core_pll_options(input),
    resolve_int_pll_options(input),
    oe);
}

inline void apply_fpga_startup_policy(FPGA &fpga,
                                      const ClockSelectionOptions &clock_selection,
                                      const PllOptions &core_pll,
                                      const PllOptions &int_pll,
                                      const bool oe)
{
  // Order matters here:
  //   1. choose the active streamer clock source
  //   2. program the core and internal PLLs
  //   3. optionally enable outputs
  //   4. perform a short visible bring-up indication
  fpga.set_clk(clock_selection);
  fpga.pll_core.set_core_clk(core_pll, fpga.v);
  fpga.pll_int.set_int_clk(int_pll, fpga.v);
  if (oe)
    fpga.output_enable(true);
  fpga.blink_led();
  if (fpga.v.veryverbose) {
    fpga.mgr.status();
    fpga.status();
  }
}
