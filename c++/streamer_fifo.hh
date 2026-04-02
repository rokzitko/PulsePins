// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Rok Zitko
//
// FIFO-backed sequence transport for the PulsePins streamer.
//
// This is the simplest host-side transport: sequence elements are written directly into the
// streamer ingress FIFO through the Avalon-MM mapped FIFO window. It is the default path
// used by the single-stream `streamer` helper in `basic_multi_dma.hh`.

#pragma once

#include <cstdint> // integer types, uint32_t, etc.
#include <string>
#include <iostream>
#include <unistd.h> // usleep

#include "tidbit.hh"
#include "delay.hh"
#include "fifo.hh"
#include "dma.hh"
#include "misc.hh"
#include "verbosity.hh"
#include "config.h"
#include "definitions.hh"
#include "colors.hh"
#include "elements.hh"
#include "sequence.hh"

// FIFO transport wrapper for one streamer input path.
class streamer_fifo : private fifo
{
private:
  static constexpr size_t size = SIZE_FIFO_IN1;

  // Keep one full element worth of slack so a 3-word element write never straddles a full FIFO.
  bool has_room() const noexcept {
    return (fill() + BYTES_TOTAL) < SIZE_FIFO_IN1;
  }

  // Low-level write preserving the 3-word `{control,count,value}` element layout.
  void perform_write(const bus_t a, const bus_t b, const bus_t c) const noexcept {
    f.write(a);
    f.write(b);
    f.write(c);
  }

  // Busy-wait for space and then push one complete element.
  void write(const bus_t a, const bus_t b, const bus_t c) const noexcept {
    while (1) {
      if (has_room()) {  // prevents lockups!
        perform_write(a, b, c);
        return;
      }
    }
  }

  void write_with_warning(const bus_t a, const bus_t b, const bus_t c) const {
    bool warning_emitted = false;
    while (1) {
      if (has_room()) {
        perform_write(a, b, c);
        return;
      }
      if (!warning_emitted) { // Show warning only once!
        std::cout << "FIFO full, stalling." << std::endl;
        warning_emitted = true;
      }
    }
  }

  void unsafe_write(const bus_t a, const bus_t b, const bus_t c) const noexcept {
    perform_write(a, b, c);
  }

  // Intermediate helper keeping the host-side element word order explicit.
  void out_ll(const control_t y, const count_t c, const value_t v) const noexcept {
      write(y, c, v);
  }

public:
  streamer_fifo(const mm &dev, const std::uintptr_t base, const std::uintptr_t in_csr_base) :
    fifo(dev, base, in_csr_base) {}

  // Send one host-side element object.
  void out(const el &e, const bool dump = false) const noexcept {
    out_ll(e.control(), e.count(), e.value());
    if (dump)
      std::cout << "Wrote " << e << std::endl;
  }

  // Report current FIFO occupancy and status bits for debugging/transport diagnostics.
  void check_fill_status() {
    std::cout << "streamer FIFO fill=" << std::dec << fill() << " status=" << status_str() << std::endl;
  }

  void report() {
    check_fill_status();
  }

  // Spool the whole sequence into the ingress FIFO in element order.
  void send_sequence(const Sequence &elements) const noexcept {
    for(const auto &e : elements)
      out(e);
  }
};
