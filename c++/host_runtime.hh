// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Shared host-process bootstrap for PulsePins executables.
//
// `HostRuntime` centralizes the process-level and FPGA-level startup sequence shared by the
// main CLI entry points. It owns the parsed input, verbosity, top-level
// FPGA wrapper, and the startup frequency-meter report used to cache the active
// `streamer_clk` frequency in the `FPGA` object.

#pragma once

#include <optional>
#include <string>

#include "freq_meter.hh"
#include "parser.hh"
#include "ppmisc.hh"
#include "startup.hh"

struct HostRuntime {
  std::string progname;
  InputParser input;
  Verbosity verbosity;
  std::optional<FPGA> fpga;
  std::optional<pp_freq_meter> freq_meter;

  HostRuntime(int argc, char *argv[], const int version) :
    progname(get_program_name(argc, argv)),
    input(argc, argv),
    verbosity(set_verbosity(input))
  {
    about(progname);
    bootstrap_process(verbosity, version);
    if (!input.exists("-noreset")) {
      rstmgr rm;
      rm.s2f_reset();
    }
    fpga.emplace(verbosity);
    apply_fpga_startup_policy(*fpga, input);
    freq_meter.emplace(input, *fpga);
    freq_meter->report();
  }

  FPGA &get_fpga() {
    return *fpga;
  }
};
