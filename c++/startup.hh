// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Common process-startup helpers for PulsePins host executables.
//
// This header centralizes the runtime policy shared by `pptool`-style programs:
//   - process-level bootstrap such as version checks, meory lock, real-time scheduler setup
//   - FPGA startup policy such as clock-source selection and PLL programming
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

inline void bootstrap_process(const Verbosity &v, const int version)
{
  // Process bootstrap is intentionally separated from FPGA startup so future tools can
  // reuse one policy without necessarily applying the other.
  check_version(version);
  // lock the entire virtual address space into physical RAM, preventing the operating
  // system from swapping those pages out to disk.
  if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
    throw std::system_error(errno, std::generic_category(), "mlockall failed");
  // SCHED_FIFO is a fixed-priority, real-time scheduling policy. A thread using this policy
  // runs uninterrupted until it voluntarily yields, blocks for I/O, or is preempted by a higher-priority thread.
  // There is no time-slicing for tasks at the same priority level.
  RealtimeScheduler rt;
  if (v.veryverbose)
    std::cout << "Scheduler: " << rt.report() << std::endl;
}

inline void apply_fpga_startup_policy(FPGA &fpga,
                                      const ClockSelectionOptions &clock_selection,
                                      const PllOptions &core_pll,
                                      const PllOptions &int_pll)
{
  // Order matters here:
  //   1. choose the active streamer clock source
  //   2. program the core and internal PLLs
  //   3. optionally enable outputs
  //   4. perform a short visible bring-up indication
  fpga.set_clk(clock_selection);
  fpga.pll_core.set_core_clk(core_pll, fpga.v);
  fpga.pll_int.set_int_clk(int_pll, fpga.v);
  fpga.blink_led();
  if (fpga.v.veryverbose) {
    fpga.mgr.status();
    fpga.status();
  }
}

inline void apply_fpga_startup_policy(FPGA &fpga, const InputParser &input)
{
  apply_fpga_startup_policy(
    fpga,
    resolve_clock_selection_options(input),
    resolve_core_pll_options(input),
    resolve_int_pll_options(input));
}
