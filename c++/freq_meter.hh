// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Host-side accessors for the FPGA frequency measurement blocks.
//
// The low-level `freq_meter` wrapper exposes the Avalon-MM register file directly enough
// to keep the hardware model visible: configure gate length, wait for one full gate, then
// read per-channel counts or convert them to Hz. `pp_freq_meter` adds PulsePins-specific
// policy such as storing the measured streamer clock back into the `FPGA` object.
// Architectural overview lives in `docs/docs/freq_meter.md` and `ip/freq_meter/README.md`.

#pragma once

#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "tidbit.hh"
#include "freqfmt.hh"
#include "fpga.hh"
#include "options.hh"

class freq_meter {
private:
  using Ticks = uint32_t; // width of all counters (incl. gate length)
  double nominal_cnt_clk_freq = 50000000; // Hz
  double correction_factor = 1.0; // true_cnt_clk_freq/nominal_cnt_clk_freq
  static constexpr uint32_t default_gate_len =  500*1000; // 10ms, assuming 50MHz clock
  Ticks gate_len;
  loc lctl;
  loc lgate_len;
  loc ln_ch;
  std::vector<loc> lresult;
  int n_ch;
  bool verbose;

public:
  static Ticks normalize_gate_len(const Ticks requested_gate_len) {
    return std::max<Ticks>(1, requested_gate_len);
  }

  static useconds_t gate_wait_time_us(const Ticks current_gate_len, const double cnt_clk_freq_hz) {
    if (!(cnt_clk_freq_hz > 0.0))
      throw std::runtime_error("freq_meter: counter clock frequency must be positive");
    const auto wait_us = std::llround(double(current_gate_len) / cnt_clk_freq_hz * 1000.0 * 1000.0);
    return static_cast<useconds_t>(std::max<long long>(1, wait_us));
  }

  freq_meter(const mm &dev, const std::uintptr_t base, const bool _verbose = false) :
    lctl(dev.get_addr(base, 0), "fm/ctl"),
    lgate_len(dev.get_addr(base, 4), "fm/gate_len"),
    ln_ch(dev.get_addr(base, 8), "fm/n_ch"),
    verbose(_verbose)
  {
    n_ch = ln_ch.read();
    if (n_ch < 1 || n_ch > 4)
      throw std::runtime_error("freq_meter: unexpected channel count");
    if (verbose)
      std::cout << "freq_meter: n_ch=" << std::dec << n_ch << std::endl;
    set_gate_len(default_gate_len);
    lresult.reserve(n_ch);
    for (int i = 0; i < n_ch; i++)
      lresult.emplace_back(loc(dev.get_addr(base, 0x10 + 4*i), "fm/result_" + std::to_string(i)));
  }

  // Reprogram the measurement window and restart accumulation.
  void set_gate_len(Ticks new_gate_len) {
    gate_len = normalize_gate_len(new_gate_len);
    lgate_len.write(gate_len);
    lctl.write(2); // clear
    lctl.write(1); // enable
    if (verbose)
      std::cout << "freq_meter: gate_len=" << std::dec << gate_len << std::endl;
  }

  auto get_gate_len() const {
    return gate_len;
  }

  // Convenience wrapper around `set_gate_len`, using seconds instead of raw cycles.
  void set_gate_time(double t) { // t in seconds
    set_gate_len(t*nominal_cnt_clk_freq);
  }

  // Read the raw per-gate edge count for channel `i`.
  Ticks read(const int i) {
    return lresult[i].read();
  }

  // Convert raw counter delta into Hz using the currently configured gate length.
  double read_freq(const int i) {
    const auto t = read(i);
    return double(t)/gate_len * nominal_cnt_clk_freq * correction_factor;
  }

  // Formatted output with precision matched to the current gate-time resolution.
  std::string read_freq_str(const int i) {
    const auto digits = std::ceil(std::log10(gate_len));
    return freqfmt::format_frequency(read_freq(i), digits, '\'');
  }

  void set_nominal_cnt_clk_freq(double f) {
    nominal_cnt_clk_freq = f;
  }

  void set_correction_factor(double f) {
    correction_factor = f;
  }

  auto get_n_ch() const {
    return n_ch;
  }

  // Wait long enough for one fresh measurement interval to complete.
  void wait_one_gate_time() const {
    usleep(gate_wait_time_us(gate_len, nominal_cnt_clk_freq));
  }
};

constexpr int METER_EXT_CLK = 0;
constexpr int METER_INT_CLK = 1;
constexpr int METER_STREAMER_CLK = 2;
constexpr int METER_CORE_CLK = 3;

class pp_freq_meter {
private:
  FPGA &fpga;

public:
  freq_meter meter;

  // If `wait` is true, block until the first post-configuration measurement is valid.
  pp_freq_meter(const FreqMeterOptions &opts, FPGA &_fpga, const bool wait, const bool verbose) :
    fpga(_fpga),
    meter(fpga.dev_h2f, FREQ_METER_0_BASE, verbose) {
    if (opts.correction_factor)
      meter.set_correction_factor(*opts.correction_factor);
    if (meter.get_n_ch() != 4)
      throw std::runtime_error("PulsePins expects exactly 4 frequency-meter channels.");
    if (wait)
      meter.wait_one_gate_time();
    fpga.set_streamer_clk(meter.read_freq(METER_STREAMER_CLK));
  }

  pp_freq_meter(const FreqMeterOptions &opts, FPGA &_fpga, const bool wait = true) :
    pp_freq_meter(opts, _fpga, wait, false) {} // verbose=false

  pp_freq_meter(const InputParser &input, FPGA &_fpga, const bool wait = true, const bool verbose = false) :
    pp_freq_meter(resolve_freq_meter_options(input), _fpga, wait, verbose) {}

  // Standard four-channel PulsePins report used by the CLI tools.
  void report() {
    const auto res_ext = meter.read_freq_str(METER_EXT_CLK);
    const auto res_int = meter.read_freq_str(METER_INT_CLK);
    const auto res_streamer = meter.read_freq_str(METER_STREAMER_CLK);
    const auto res_core = meter.read_freq_str(METER_CORE_CLK);
    const std::string prefix = "freq_meter: "s;
    std::cout << prefix << "ext_clk      " << res_ext << std::endl;
    std::cout << prefix << "int_clk      " << res_int << std::endl;
    std::cout << prefix << "streamer_clk " << res_streamer << std::endl;
    std::cout << prefix << "core_clk     " << res_core << std::endl;
  }
};
