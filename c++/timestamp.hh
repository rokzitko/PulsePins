// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko
//
// Host-side helpers for timestamp capture and routing control.
//
// The `timestamp` wrapper owns two FIFOs and one configuration PIO block. It provides the
// software-visible model for the `ts_core` subsystem used by `ppts` and `ppgpsdo`:
// select sources, clear stale samples, then read complete 64-bit timestamps from the PPS
// and/or auxiliary capture paths. Architectural overview lives in `docs/docs/timestamp.md`
// and `ip/ts_core/README.md`.

#pragma once

#include <cstdint>
#include <chrono>
#include <exception>

#include <sstream>
#include <stdexcept>
#include <string>

#include "address_map.hh"
#include "fifo.hh"
#include "pio.hh"

constexpr int TS_SEL_PULSE_1MS = 7;
constexpr int TS_SEL_PULSE_10MS = 6;
constexpr int TS_SEL_PULSE_100MS = 5;
constexpr int TS_SEL_PULSE_1S = 4;
constexpr int TS_SEL_PIO_AUX_IN0 = 3;
constexpr int TS_SEL_EXT_TRIGGER_IN0 = 2;
constexpr int TS_SEL_STREAMER_TRIGGER_IN0 = 1;
constexpr int TS_SEL_STREAMER_TRIGGER_ACTIVATED = 0;

constexpr int CFG_TS_SEL_PPS = 1;
constexpr int CFG_TS_SEL_SIGA_MUX_OFFSET = 2;

static_assert(address_map::contains(address_map::h2f::fifo_ts_pps_out, 0));
static_assert(address_map::contains(address_map::h2f::fifo_ts_pps_in_csr, fifo_event_shift));
static_assert(address_map::contains(address_map::h2f::fifo_ts_siga_out, 0));
static_assert(address_map::contains(address_map::h2f::fifo_ts_siga_in_csr, fifo_event_shift));
static_assert(address_map::contains(address_map::lw::pio_cfg, 0x04*5));

inline std::string sel_str(int sel)
{
  switch (sel) {
  case TS_SEL_PULSE_1MS:
    return "pulse 1ms";
  case TS_SEL_PULSE_10MS:
    return "pulse 10ms";
  case TS_SEL_PULSE_100MS:
    return "pulse 100ms";
  case TS_SEL_PULSE_1S:
    return "pulse 1s";
  case TS_SEL_PIO_AUX_IN0:
    return "aux in 0";
  case TS_SEL_EXT_TRIGGER_IN0:
    return "ext trig 0";
  case TS_SEL_STREAMER_TRIGGER_IN0:
    return "trigger in0";
  case  TS_SEL_STREAMER_TRIGGER_ACTIVATED:
    return "trigger activated";
  default:
    return "INVALID";
  }
}

class timestamp {
private:
  fifo ff;
  fifo ffA;
  pio_out_bits pio_cfg;

  // Busy-wait until one 32-bit FIFO word is available on the PPS path.
  uint32_t read_one() {
    while (!filled()) {}
    return ff.read();
  }
  // Busy-wait until one 32-bit FIFO word is available on the auxiliary path.
  uint32_t read_oneA() {
    while (!filledA()) {}
    return ffA.read();
  }

public:
  timestamp(mm &dev_h2f,
            mm &dev_lw,
            const address_map::H2fRegion base, const address_map::H2fRegion in_csr_base,
            const address_map::H2fRegion baseA, const address_map::H2fRegion in_csr_baseA,
            const address_map::LwRegion pio_cfg_base) :
    ff(dev_h2f, base.base, in_csr_base.base),
    ffA(dev_h2f, baseA.base, in_csr_baseA.base),
    pio_cfg(dev_lw, pio_cfg_base.base)
    {
      clear_fifo();
      clear_fifoA();
    }

    // Select the crystal-derived PPS source.
    void sel_pps_xtal() {
    pio_cfg.clear(1 << CFG_TS_SEL_PPS);
  }

    // Select the external PPS input source.
    void sel_pps_in() {
    pio_cfg.set(1 << CFG_TS_SEL_PPS);
  }

    // Route one of the predefined timestamp sources to the auxiliary capture path.
    void selA(const int i) {
    if (i < 0 || i > 7)
      throw std::out_of_range("timestamp::selA selector must be in range 0..7");
    pio_cfg.clear((1 << 2) + (1 << 3) + (1 << 4));
    pio_cfg.set(i << 2);
  }

    // Human-readable summary of the current routing configuration.
    std::string get_cfg() {
    const auto cfg = pio_cfg.read();
    std::stringstream ss;
    ss << ((cfg & (1 << CFG_TS_SEL_PPS)) ? "PPS_IN" : "PPS_XTAL");
    const int i = (cfg >> 2) & 0x7;
    ss << " " << "sel=[" << sel_str(i) << "]";
    return ss.str();
  }

  // Returns true if there are elements to be read back.
  bool filled() {
    return ff.fill() > 0;
  }
  bool filledA() {
    return ffA.fill() > 0;
  }

  void clear_fifo() {
    while (filled())
      ff.read(); // ignore return value
  }
  void clear_fifoA() {
    while (filledA())
      ffA.read(); // ignore return value
  }

    // Each timestamp record is stored as two 32-bit FIFO words.
    uint64_t read() {
    return (uint64_t(read_one()) << 32) + read_one();
  }
  uint64_t readA() {
    return (uint64_t(read_oneA()) << 32) + read_oneA();
  }

    // Wait until a full 64-bit PPS event record is present, then reconstruct it.
    uint64_t read_with_timeout(const double timeout = 2.0) {
    std::chrono::steady_clock::time_point initial_time = std::chrono::steady_clock::now();
    while (ff.fill() < 2) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time);
      if (timeout > 0.0 && elapsed.count() > abs(timeout))
        throw std::runtime_error("Timeout.");
      usleep(100); // don't hose CPU in poll loop
    }
    return read();
  }

  // Wait until a full 64-bit auxiliary event record is present.
  uint64_t readA_with_timeout(const double timeout = 2.0) {
    std::chrono::steady_clock::time_point initial_time = std::chrono::steady_clock::now();
    while (ffA.fill() < 2) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time);
      if (timeout > 0.0 && elapsed.count() > abs(timeout))
        throw std::runtime_error("Timeout.");
      usleep(100); // don't hose CPU in poll loop
    }
    return readA();
  }
};
