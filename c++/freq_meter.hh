// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Host-side accessors for the FPGA frequency measurement blocks.
//
// The low-level `freq_meter` wrapper exposes the Avalon-MM register file directly enough
// to keep the hardware model visible: configure gate length, wait for one full gate, then
// read per-channel counts or convert them to Hz. `pp_freq_meter` adds PulsePins-specific
// policy such as storing the measured streamer clock back into the `FPGA` object.

#pragma once

#include <cassert>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <cmath>
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
   freq_meter(const mm &dev, const std::uintptr_t base, const bool _verbose = true) :
     lctl(dev.get_loc(base, 0)),
     lgate_len(dev.get_loc(base, 4)),
     ln_ch(dev.get_loc(base, 8)),
     verbose(_verbose)
   {
     n_ch = ln_ch.read();
     assert(1 <= n_ch && n_ch <= 4);
     if (verbose)
       std::cout << "freq_meter: n_ch=" << std::dec << n_ch << std::endl;
     set_gate_len(default_gate_len);
     lresult.reserve(n_ch);
     for (int i = 0; i < n_ch; i++)
       lresult.push_back(dev.get_loc(base, 0x10 + 4*i));
   }

    // Reprogram the measurement window and restart accumulation.
    void set_gate_len(Ticks new_gate_len) {
     gate_len = new_gate_len;
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

   void wait_one_gate_time() const {
     usleep( double(gate_len)/nominal_cnt_clk_freq * 1000*1000 );
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
    pp_freq_meter(const FreqMeterOptions &opts, FPGA &_fpga, const bool wait = true) :
     fpga(_fpga),
     meter(fpga.dev_h2f, FREQ_METER_0_BASE) {
       if (opts.correction_factor)
         meter.set_correction_factor(*opts.correction_factor);
        assert(meter.get_n_ch() == 4);
        if (wait)
          meter.wait_one_gate_time();
       fpga.set_streamer_clk(meter.read_freq(METER_STREAMER_CLK));
     }

   pp_freq_meter(const InputParser &input, FPGA &_fpga, const bool wait = true) :
     pp_freq_meter(resolve_freq_meter_options(input), _fpga, wait) {}

    // Standard four-channel PulsePins report used by the CLI tools.
    void report() {
     std::cout << "ext_clk      " << meter.read_freq_str(METER_EXT_CLK) << std::endl;
     std::cout << "int_clk      " << meter.read_freq_str(METER_INT_CLK) << std::endl;
     std::cout << "streamer_clk " << meter.read_freq_str(METER_STREAMER_CLK) << std::endl;
     std::cout << "core_clk     " << meter.read_freq_str(METER_CORE_CLK) << std::endl;
   }
};
