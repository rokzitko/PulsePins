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
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <unistd.h> // usleep

#include "tidbit.hh"
#include "address_map.hh"
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
#include "stall_timeout.hh"

static_assert(address_map::contains(address_map::h2f::fifo_1_in, 0));
static_assert(address_map::contains(address_map::h2f::fifo_1_in_csr, fifo_event_shift));
static_assert(address_map::contains(address_map::h2f::fifo_2_in, 0));
static_assert(address_map::contains(address_map::h2f::fifo_2_in_csr, fifo_event_shift));
static_assert(address_map::contains(address_map::h2f::fifo_3_in, 0));
static_assert(address_map::contains(address_map::h2f::fifo_3_in_csr, fifo_event_shift));
static_assert(address_map::contains(address_map::h2f::fifo_4_in, 0));
static_assert(address_map::contains(address_map::h2f::fifo_4_in_csr, fifo_event_shift));

// FIFO transport wrapper for one streamer input path.
class streamer_fifo : private fifo
{
private:
  static constexpr size_t size = SIZE_FIFO_IN1;

  // Low-level write preserving the 3-word `{control,count,value}` element layout.
  void perform_write(const bus_t a, const bus_t b, const bus_t c) const noexcept {
    f.write(a);
    f.write(b);
    f.write(c);
  }

  std::string fifo_wait_details(const uint32_t current_fill, const uint32_t current_status) {
    return "fill=" + std::to_string(current_fill) + " status=" + status_str(current_status);
  }

  void wait_for_room_or_timeout(const bool warn_once) {
    TimeoutGuard watchdog("streamer FIFO enqueue", default_transport_stall_timeout_s);
    std::optional<std::pair<uint32_t, uint32_t>> previous_state;
    bool warning_emitted = false;
    while (1) {
      const auto current_fill = fill();
      const auto current_status = status();
      if ((current_fill + BYTES_TOTAL) < SIZE_FIFO_IN1)
        return;
      watchdog.progress_if_changed(previous_state, std::make_pair(current_fill, current_status));
      if (warn_once && !warning_emitted) {
        std::cout << "FIFO full, stalling." << std::endl;
        warning_emitted = true;
      }
      watchdog.throw_if_stalled(fifo_wait_details(current_fill, current_status));
      sleep_1ms();
    }
  }

  // Busy-wait for space and then push one complete element.
  void write(const bus_t a, const bus_t b, const bus_t c) {
    wait_for_room_or_timeout(false);
    perform_write(a, b, c);
  }

  void write_with_warning(const bus_t a, const bus_t b, const bus_t c) {
    wait_for_room_or_timeout(true);
    perform_write(a, b, c);
  }

  void unsafe_write(const bus_t a, const bus_t b, const bus_t c) const noexcept {
    perform_write(a, b, c);
  }

  // Intermediate helper keeping the host-side element word order explicit.
  void out_ll(const control_t y, const count_t c, const value_t v) {
      write(y, c, v);
  }

public:
  streamer_fifo(const mm &dev, const address_map::H2fRegion base, const address_map::H2fRegion in_csr_base, std::string name = "streamer_fifo") :
    fifo(dev, base.base, in_csr_base.base, name) {}

  // Send one host-side element object.
  void out(const el &e, const bool dump = false) {
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
  void send_sequence(const Sequence &elements) {
    for(const auto &e : elements)
      out(e);
  }
};
