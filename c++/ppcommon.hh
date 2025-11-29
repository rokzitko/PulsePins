// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Common code for pptool and ppserver

#pragma once

#include <iostream>
#include <fstream>
#include <thread>
#include <bitset>
#include <cassert>
#include <unistd.h> // usleep
#include <cctype> // isdigit
#include <cstdlib> // rand
#include <future> // async
#include <utility> // pair
#include <vector>
#include <filesystem>
#include <map>

#include "tidbit.hh"
#include "misc.hh"
#include "parser.hh"
#include "fpga.hh"
#include "elements.hh"
#include "sequence.hh"
#include "streamer.hh"
#include "sequences.hh"
#include "readback.hh"
#include "throttler.hh"
#include "timer.hh"
#include "realtime.hh"
#include "combiner.hh"
#include "trigger.hh"
#include "qout.hh"
#include "st_mux.hh"
#include "pll_clk.hh"
#include "pll_rules.hh"
#include "basic_multi_dma.hh"
#include "counter.hh"

#define VERSION "2025.1"

const int tidbit = TIDBITNR;
const int version = 2;

// do nothing
inline void identity(Sequence &e) {
}

// drop an element (nr is 0-based)
template <std::size_t nr>
void drop(Sequence &s)
{
  if (nr < s.size())
    s.erase(s.begin() + nr);
}

// drop elements with zero count from the sequence
inline void drop_count0(Sequence &s)
{
  for (auto it = s.begin(); it != s.end(); ) {
    if (it->count() == 0) {
      it = s.erase(it); // erase returns iterator to next element
    } else {
      ++it;
    }
  }
}

// Convert a general sequence containing a mix of update types (load, set, clear, flip,...) to pure BitLoad updates and merge equal value updates.
// This generates a sequence that we expect to obtain when we read back the data stream for checking.
inline void convert_for_readback_check(Sequence &s) {
  auto s_new = s.convert_to_BitLoad().merge();
  s = s_new;
}

// Positive value: timeout after last received element (in seconds)
// Negative value: timeout after start (in seconds)
auto readback_timeout(const InputParser &input) {
  double timeout = 0.0;
  if (input.exists("-timeout")) {
    timeout = parse_double(input, "-timeout", "0");
    std::cout << "readback timeout=" << abs(timeout) << (timeout > 0 ? "s [after last read]" : "s [after start]") << std::endl;
  }
  return timeout;
}

// Max. nr. of elements that can be queued in (assuming no replay elements).
constexpr auto max_size =
  (FIFO_1_IN_FIFO_DEPTH*(FIFO_1_IN_AVALONMM_AVALONST_DATA_WIDTH/8))/BYTES_TOTAL // Avalon ST FIFO
  + SIZE_FIFO_IN1   // FIFO 1 in input_fifo
  + SIZE_FIFO_IN2   // FIFO 2 in input_fifo
  - 2*almost_shift  // because we are using almostfull for stalling
  + 1;

// Send a test sequence 'elements' to streamer 'fifo' and use readback 'rb' for testing equivalence (if -check command line argument is used).
template<typename Transport, typename Convert>
int send_and_trig(Transport &tr,
                  streamer_control &sc,
                  readback &rb,
                  counter &ctr,
                  Sequence &elements,
                  const InputParser &input,
                  const bool force_trigger,
                  const Verbosity &v,
                  Convert convert)
{
  int rc = 0; // return code, 0 indicates no error
  const value_t final = input.exists("-t") ? parse_value(input, "-t", "0") : random_value();
  elements.push_back(el(final)); // add the terminator element with a specified final data value
  if (v.veryverbose) elements.dump(std::cout, "| ");
  if (v.veryverbose) tr.report();
  // NOTE: internal trigger forcing can only work if the sequence is short enough to fit into the input FIFO buffer, otherwise
  // this code will stall on fifo.send_sequence(elements) line.
  if (elements.size() > max_size)
    std::cout << red << "Sequence will not fit in buffers. size=" << elements.size() << " max_size=" << max_size << rst << std::endl;
  tr.send_sequence(elements);
  sc.status_report();
  if (v.veryverbose) tr.report();
  if (force_trigger) {
    usleep(100);
    if (v.verbose) std::cout << cyan << " ---> Forcing trigger." << rst << std::endl;
    if (input.exists("-delay")) {
      const double delay = parse_time(input, "-delay", "0"); // in seconds
      usleep(1000 * 1000 * delay);
    }
    sc.trigger_force();
    sc.status_report();
  } else {
    if (v.verbose) std::cout << cyan << " --->  Arming trigger." << rst << std::endl;
    sc.trigger_enable();
    sc.status_report();
  }

  const double timeout = readback_timeout(input);

  if (input.exists("-check")) {
    convert(elements);
    // no-strobe handling?
    drop_count0(elements);
    if (v.veryverbose && input.exists("-dump-converted"))
      elements.dump(std::cout, "% ");
    usleep(10);
    if (v.veryverbose) rb.check_fill_status();
    int successful = rb.check(elements, timeout);
    if (!successful)
      rc |= 1;
  }

  if (input.exists("-read")) {
    usleep(10);
    if (v.veryverbose) rb.check_fill_status();
    rb.read_all(timeout);
  }

  if (input.exists("-dont_wait")) // don't wait for the streamer to complete its work
    return rc;

  sc.wait_to_complete(v);
  usleep(1000); // 1ms delay
  value_t final_qout = sc.get_qout();
  const bool match = final_qout == final;
  std::cout << "send_and_trig(): Final qout=" << "0x" << std::hex << final_qout << "=" << std::dec << final_qout;
  if (match) {
    std::cout << " OK" << std::endl;
  } else {
    if (!(envVarExists("PP_IGNORE_QOUT_FINAL") || input.exists("-pp_ignore_qout_final"))) {
      std::cout << red << " Mismatch: expecting 0x" << std::hex << final << rst << std::endl;
      rc |= 2;
    }
  }
  sc.statistics();
  if (sc.get_input_fifo1_ctr_in() != sc.get_input_fifo1_ctr_out()) {
    std::cout << red << "Mismatch in the streamer input FIFO1 detected." << rst << std::endl;
    rc |= 8;
  }
  if (sc.get_input_fifo2_ctr_in() != sc.get_input_fifo2_ctr_out()) {
    std::cout << red << "Mismatch in the streamer input FIFO2 detected." << rst << std::endl;
    rc |= 8;
  }
  if (sc.get_output_fifo_ctr_in() != sc.get_output_fifo_ctr_out()) {
    std::cout << red << "Mismatch in the streamer output FIFO detected." << rst << std::endl;
    rc |= 8;
  }
  if (sc.get_overflow()) {
    std::cout << red << "Input FIFO overflow detected." << rst << std::endl;
    rc |= 16;
  }
  if (rb.overflow()) {
    std::cout << red << "Readback overflow detected." << rst << std::endl;
    rc |= 32;
  }
  ctr.latch_all();
  ctr.short_report();
  if (sc.buffer_error()) {
    std::cout << red << "Buffer error detected." << rst << std::endl;
    rc |= 64;
  }
  return rc;
}

// Wrapper for the case where the conversion function is not required
template<typename Transport>
inline int send_and_trig(Transport &tr, streamer_control &sc, readback &rb, counter &ctr,
                         Sequence &elements, const InputParser &input, const bool force_trigger, const Verbosity &v) {
  return send_and_trig(tr, sc, rb, ctr, elements, input, force_trigger, v, identity);
}

constexpr bool force_trigger = true;
constexpr bool do_not_force_trigger = false;
