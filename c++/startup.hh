// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Common process-startup helpers for PulsePins host executables.
//
// This header centralizes the runtime policy shared by `pptool`-style programs:
//   - process-level setup such as memory lock and realtime scheduler setup
//   - FPGA startup policy such as clock-source selection and PLL programming
//
// Architectural overview lives in `c++/README.md`, `docs/docs/cpp.md`, and
// `docs/docs/pptool.md`.

#pragma once

#include <cerrno>
#include <iostream>
#include <string>
#include <system_error>
#include <sys/mman.h>

#include "options.hh"
#include "realtime.hh"
#include "fpga.hh"

inline RealtimeScheduler enable_realtime_process_mode(const Verbosity &v)
{
  // Realtime process setup is intentionally separated from FPGA startup so future tools can
  // reuse one policy without necessarily applying the other.
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
  return rt;
}

inline bool command_forces_startup_reset(const std::string &progname)
{
  return progname == "ppreset";
}

inline void apply_fpga_startup_policy(FPGA &fpga,
                                      const bool reset_FPGA,
                                      const bool dark_mode,
                                      const ClockSelectionOptions &clock_selection,
                                      const PllOptions &core_pll,
                                      const PllOptions &int_pll)
{
  // Order matters here:
  //   1. optionally reset the FPGA
  //   2. choose the active streamer clock source
  //   3. program the core and internal PLLs
  //   4. perform a short visible bring-up indication
  if (reset_FPGA)
    fpga.rm.s2f_reset();
  fpga.dark_mode = dark_mode;
  if (fpga.dark_mode) {
    fpga.led_en(false);
    fpga.status_en(false);
  }
  fpga.set_clk(clock_selection);
  fpga.pll_core.set_core_clk(core_pll, fpga.v);
  fpga.pll_int.set_int_clk(int_pll, fpga.v);
  if (!fpga.dark_mode)
    fpga.blink_led();
  if (fpga.v.veryverbose) {
    fpga.mgr.status();
    fpga.status();
  }
}

inline void apply_fpga_startup_policy(FPGA &fpga,
                                      const InputParser &input,
                                      const bool force_reset_FPGA = false)
{
  apply_fpga_startup_policy(
    fpga,
    resolve_reset_FPGA(input, force_reset_FPGA),
    resolve_dark_mode(input),
    resolve_clock_selection_options(input),
    resolve_core_pll_options(input),
    resolve_int_pll_options(input));
}
