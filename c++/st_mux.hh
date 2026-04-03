// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Host-side wrapper for the Avalon-ST multiplexer block.
//
// This class mirrors the small programming model in `ip/st_mux/st_mux_if.sv`: select
// the active upstream channel and read the two traffic counters. Architectural overview
// lives in `docs/docs/st_mux.md` and `ip/st_mux/README.md`.

#pragma once

#include <iostream>
#include <stdexcept>

#include "memory.hh"
#include "verbosity.hh"
#include "misc.hh"

class st_mux {
private:
  loc lchannel;
  loc ctr1_l, ctr1_h, ctr2_l, ctr2_h;
  const Verbosity &v;

public:
  st_mux(const mm &dev, const Verbosity &_v, const std::uintptr_t base) :
    lchannel(dev.get_loc(base, 0)),
    ctr1_l(dev.get_loc(base, 0)),
    ctr1_h(dev.get_loc(base, 4)),
    ctr2_l(dev.get_loc(base, 8)),
    ctr2_h(dev.get_loc(base, 12)),
    v(_v) {}

  // Switch between the two upstream Avalon-ST sources.
  void channel(const int ch) {
    if (ch != 1 && ch != 2)
      throw std::runtime_error("Only channels 1 and 2 are available");
    if (v.veryverbose)
      std::cout << "st_mux, channel=" << ch << std::endl;
    lchannel.write(ch-1);
  }

  // Number of successful Avalon-ST transfers observed on input 1.
  uint64_t ctr1() {
    return (uint64_t(ctr1_h.read()) << 32) + uint64_t(ctr1_l.read());
  }

  // Number of successful Avalon-ST transfers observed on input 2.
  uint64_t ctr2() {
    return (uint64_t(ctr2_h.read()) << 32) + uint64_t(ctr2_l.read());
  }

  void report() {
    if (v.veryverbose)
      std::cout << "Avalon ST multiplexer statistics: ctr1=" << with_underscores(ctr1())
        << " ctr2=" << with_underscores(ctr2()) << std::endl;
  }
};
