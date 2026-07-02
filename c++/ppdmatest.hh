// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Streaming using the direct memory access

#pragma once

#include <iostream>
#include <iomanip>
#include <thread>
#include <cmath>
#include <algorithm>
#include <optional>
#include <unistd.h>
#include <algorithm>

#include "basic_multi_dma.hh"
#include "PMODDA3.hh"
#include "readback.hh"
#include "parser.hh"
#include "streamer.hh"
#include "config.h"
#include "definitions.hh"
#include "ppworkflow.hh"

class dmatests {
public:
  dma_streamer &ds;
  readback &rb;
  counter &ctr;
  const InputParser &input;
  const Verbosity &verb;

  static void trig_force(streamer_control &sc) {
    const int delay = 100*1000;
    usleep(delay);
    sc.trigger_force();
    std::cout << "%%% Triggered." << std::endl;
  }

  int wait_to_complete_or_abort() {
    const int rc = ds.sc.wait_to_complete(verb);
    if (rc & RC_TIMEOUT)
      abort_streamer_after_timeout(ds.sc, force_trigger, verb);
    return rc;
  }

  int test4() {
    std::cout << "test4 - sequential counter (or randomized values using the -rnd switch) with random duration" << std::endl;
    const auto c = parse_count(input, "-c", "10000");
    const auto vmax = parse_value(input, "-v", "10");
    const auto rnd = input.exists("-rnd");
    auto elements = prepare_random_test_sequence(vmax, c, rnd);
    return send_and_trig(ds.dma, ds.sc, rb, ctr, elements, input, force_trigger, verb);
  }

  size_t write_sequence(bool terminator) {
    const auto c = parse_count(input, "-c", "1000");
    long unsigned v = parse_value(input, "-v", "10");
    const long unsigned vmax = ds.dma.max_size/BYTES_TOTAL-1; // maximum number of elements (include one position for terminal element)
    v = std::min(v, vmax);
    size_t len = v;
    if (verb.verbose)
      std::cout << "c=" << std::dec << c << " v=" << v << std::endl;
    for (size_t i = 0; i < v; i++)
      ds.dma.write_element(i, el(c, i));
    if (terminator) {
      ds.dma.write_element(v, el());
      len++;
    }
    return len;
  }

  int test21() {
    std::cout << "test21 - long sequence (DMA)" << std::endl;
    const auto len = write_sequence(true);
    std::thread trig(trig_force, std::ref(ds.sc));
    try {
      ds.dma.transfer(BYTES_TOTAL*len);
    } catch (...) {
      if (trig.joinable())
        trig.join();
      throw;
    }
    trig.join();
    return wait_to_complete_or_abort();
  }

  int test22() {
    std::cout << "test22 - loops of long sequence (DMA)" << std::endl;
    const auto len = write_sequence(false);
    const auto reps = parse_count(input, "-reps", "0"); // repetitions, 0 = infinity
    const size_t sequence_bytes = BYTES_TOTAL*len;
    std::optional<size_t> terminator_offset;
    if (reps != 0) {
      terminator_offset = sequence_bytes;
      ds.dma.write_element(len, el());
    }
    std::cout << "reps=" << std::dec << reps << std::endl;
    std::thread trig(trig_force, std::ref(ds.sc));
    try {
      ds.dma.transfer_multiple_times(sequence_bytes, reps, terminator_offset, BYTES_TOTAL);
    } catch (...) {
      if (trig.joinable())
        trig.join();
      throw;
    }
    trig.join();
    return wait_to_complete_or_abort();
  }

  int test25() {
    std::cout << "test25 - PMOD DA3 sine generator (DMA)" << std::endl;

    const auto samples = parse_count(input, "-samples", "250");
    const auto reps = parse_count(input, "-reps", "0"); // repetitions, 0 = infinity
    const auto dwell = parse_time(input, "-dwell", "10us");
    const auto vmin = parse_double(input, "-vmin", "0.0");
    const auto vmax = parse_double(input, "-vmax", "2.5");
    auto cfg = pmod_da3::default_spi_config();
    cfg.spi_clock_hz = parse_double(input, "-spi_clock", "10e6");

    double streamer_clk_hz = 100e6;
    try {
      streamer_clk_hz = ds.fpga.streamer_freq();
    } catch (const std::exception &) {
      if (verb.verbose)
        std::cout << "streamer clock not measured, assuming " << streamer_clk_hz << " Hz" << std::endl;
    }
    cfg.decoder_clock_hz = streamer_clk_hz;

    if (samples < 2)
      throw std::runtime_error("test25 requires at least two samples per sine period");
    if (!(dwell >= 0.0))
      throw std::runtime_error("test25 requires a non-negative -dwell");
    if (vmax < vmin)
      throw std::runtime_error("test25 requires the upper voltage bound to be at least the lower bound");

    const auto dwell_ticks = static_cast<count_t>(std::llround(dwell * streamer_clk_hz));

    const double vmid = 0.5 * (vmin + vmax);
    const double amplitude = 0.5 * (vmax - vmin);
    const double pi = std::acos(-1.0);

    size_t pos = 0;
    size_t seq_len = 0;
    size_t total_len = 0;
    value_t final_value = default_final_value;
    for (count_t i = 0; i < samples; i++) {
      const double phase = (2.0 * pi * static_cast<double>(i)) / static_cast<double>(samples);
      const double volts = vmid + amplitude * std::sin(phase);
      auto builder = pmod_da3::transaction_for_voltage(volts, 2.5, cfg);
      const auto &tx = builder.sequence();
      seq_len = tx.length();
      final_value = tx.back().value();
      for (const auto &e : tx) {
        if (4*pos + BYTES_TOTAL > ds.dma.max_size)
          throw std::runtime_error("test25 sequence does not fit in DMA buffer");
        ds.dma.write_element(pos, e);
        total_len += e.count();
        pos++;
      }
      if (4*pos + BYTES_TOTAL > ds.dma.max_size)
        throw std::runtime_error("test25 sequence does not fit in DMA buffer");
      if (dwell_ticks != 0) {
        ds.dma.write_element(pos, el(dwell_ticks, tx.back().value()));
        total_len += dwell_ticks;
        pos++;
      }
    }
    const size_t waveform_bytes = BYTES_TOTAL*pos;
    std::optional<size_t> terminator_offset;
    if (reps != 0) {
      if (4*pos + BYTES_TOTAL > ds.dma.max_size)
        throw std::runtime_error("test25 sequence does not fit in DMA buffer");
      terminator_offset = waveform_bytes;
      ds.dma.write_element(pos, el(final_value));
    }

    if (verb.verbose) {
      std::cout << "dwell_ticks=" << dwell_ticks << " sequence_len=" << seq_len << std::endl;
      std::cout << "total_len=" << total_len << std::endl; // samples*(dwell_ticks+seq_len)
      const auto period = samples * dwell;
      std::cout << "samples=" << std::dec << samples << " reps=" << reps << " dwell=" << dwell << "s"
                << " period=" << period << "s"
                << " waveform_freq=" << (period > 0.0 ? 1.0/period : 0.0) << "Hz" << std::endl;
      const auto actual_period = samples * (dwell_ticks+seq_len)/streamer_clk_hz;
      const auto actual_freq = 1/actual_period;
      std::cout << "actual period=" << actual_period << " s  actual freq=" << actual_freq << " Hz" << std::endl;
      std::cout << "requested SPI clock=" << cfg.spi_clock_hz << "Hz achieved SPI clock="
                << pmod_da3::transaction_for_voltage(vmid, 2.5, cfg).achieved_spi_clock_hz() << "Hz" << std::endl;
    }

    std::thread trig(trig_force, std::ref(ds.sc));
    try {
      ds.dma.transfer_multiple_times(waveform_bytes, reps, terminator_offset, BYTES_TOTAL);
    } catch (...) {
      if (trig.joinable())
        trig.join();
      throw;
    }
    trig.join();
    return wait_to_complete_or_abort();
  }

  dmatests(dma_streamer &_ds, readback &_rb, counter &_ctr, const InputParser &_input, const Verbosity &_v) :
    ds(_ds), rb(_rb), ctr(_ctr), input(_input), verb(_v) {}

  int run(const int test) {
    std::cout << "Requested test " << std::dec << test << std::endl;
    int rc = RC_OK;
    switch (test) {
    case 0:
      std::cout << "There is nothing to do!" << std::endl;
      break;
    case 4:
      rc = test4();
      break;
    case 21:
      rc = test21();
      break;
    case 22:
      rc = test22();
      break;
    case 25:
      rc = test25();
      break;
    default:
      std::cerr << "Unknown test " << test << std::endl;
      rc = RC_INVALID_ARG;
      break;
    }
    return rc;
  }
};
