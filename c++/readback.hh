// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Host-side wrapper for the readback / run-length encoder subsystem.
//
// The readback path observes the streamer output (or external pins when output-enable is
// disabled), re-encodes it into run-length elements, and lets software either dump the
// captured sequence or compare it against a host-side reference `Sequence`.
// Architectural overview lives in `docs/docs/readback.md` and `ip/rl_encoder_if/README.md`.

#pragma once

#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "colors.hh"
#include "address_map.hh"
#include "delay.hh"
#include "fpga.hh"
#include "options.hh"
#include "stall_timeout.hh"
#include "streamer.hh"

static_assert(address_map::contains(address_map::h2f::rl_encoder_if, 8));

class ReadbackException : public std::exception {
  std::string msg;
public:
  ReadbackException(const std::string &_msg) : msg(_msg) {}
  const char* what() const noexcept override {
    return msg.c_str();
  }
};

struct ReadbackTimeoutPolicy {
  double first_element_timeout_s = 0.0;
  double idle_timeout_s = 0.0;
  double total_timeout_s = 0.0;

  bool enabled() const noexcept {
    return first_element_timeout_s > 0.0 || idle_timeout_s > 0.0 || total_timeout_s > 0.0;
  }
};

inline ReadbackTimeoutPolicy legacy_readback_timeout_policy(const double timeout) {
  if (timeout > 0.0)
    return {0.0, timeout, 0.0};
  if (timeout < 0.0)
    return {0.0, 0.0, std::abs(timeout)};
  return {};
}

class readback
{
private:
  FPGA &fpga;
  fifo f;
  loc lcontrol;                // control interface port (reset signal)
  loc lmode;                   // readback mode port
  loc lstatus;                 // status port
  loc lcounter;                // pulse counter port
  loc lcrc32;                  // CRC port
  const Verbosity &v;
  std::ostream &F = std::cout; // output stream for messages
  std::string prefix = "C ";   // prefix for element reporting
  bool last_operation_timed_out_ = false;

  using time_point = std::chrono::steady_clock::time_point;

  [[noreturn]] void throw_timeout(const std::string &message) {
    last_operation_timed_out_ = true;
    throw ReadbackException(message);
  }

  void check_timeout(const ReadbackTimeoutPolicy &timeout_policy,
                     const time_point &initial_time,
                     const std::optional<time_point> &last_read) {
    if (!timeout_policy.enabled())
      return;

    const auto now = std::chrono::steady_clock::now();
    if (timeout_policy.total_timeout_s > 0.0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time);
      if (elapsed.count() > timeout_policy.total_timeout_s)
        throw_timeout("Timeout waiting for readback completion.");
    }

    if (!last_read.has_value()) {
      if (timeout_policy.first_element_timeout_s > 0.0) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time);
        if (elapsed.count() > timeout_policy.first_element_timeout_s)
          throw_timeout("Timeout waiting for the first readback element.");
      }
      return;
    }

    if (timeout_policy.idle_timeout_s > 0.0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - *last_read);
      if (elapsed.count() > timeout_policy.idle_timeout_s)
        throw_timeout("Timeout waiting for more readback data.");
    }
  }

  void wait_for_more_data(const ReadbackTimeoutPolicy &timeout_policy,
                          const time_point &initial_time,
                          const std::optional<time_point> &last_read) {
    check_timeout(timeout_policy, initial_time, last_read);
    sleep_1ms();
  }

public:
  readback(FPGA &_fpga,
            const mm &dev,
            const address_map::H2fRegion base,
            const address_map::H2fRegion csr_base,
            const address_map::H2fRegion control_base,
            std::string name = "readback"s) :
    fpga(_fpga),
    f(dev, base.base, csr_base.base, name),
    lcontrol(dev, control_base.base, name + "/control"),    // w
    lmode(dev, control_base.base, 4, name + "/mode"),    // w
    lstatus(dev, control_base.base, name + "/status"),     // r
    lcounter(dev, control_base.base, 4, name + "/counter"), // r
    lcrc32(dev, control_base.base, 8, name + "/crc32"),   // r
    v(fpga.v)
    {
      reset();
    }

  readback(const ReadbackOptions &opts,
            FPGA &_fpga) :
    readback(_fpga, _fpga.dev_h2f,
             address_map::h2f::fifo_rl_out,
             address_map::h2f::fifo_rl_in_csr,
             address_map::h2f::rl_encoder_if) {
      mode(opts.mode);
    }

  readback(const InputParser &input,
            FPGA &_fpga) :
    readback(resolve_readback_options(input), _fpga) {
    }

  void check_fill_status() {
    f.fill();
    f.status();
  }

  // Returns true if there are elements to be read back.
  bool filled() {
    return f.fill() > 0;
  }

  void clear_fifo() {
    TimeoutGuard watchdog("readback FIFO drain", default_transport_stall_timeout_s);
    while (1) {
      const auto current_fill = f.fill();
      if (current_fill == 0)
        return;
      watchdog.throw_if_total_timeout("fill=" + std::to_string(current_fill));
      f.read(); // ignore return value
    }
  }

    // Reset the hardware encoder, wait long enough for the streamer domain to observe it,
    // then drain any stale FIFO contents.
    void reset() {
    if (v.veryverbose)
      F << "Readback: resetting" << std::endl;
    uint32_t control = 0;
    uint32_t mask = 0b1;
    BITMASK_SET(control, mask);
    lcontrol.write(control);
    fpga.sleep_for_at_least_n_streamer_periods(5);
    BITMASK_CLEAR(control, mask);
    lcontrol.write(control);
    clear_fifo();
  }

    // Normal bitstreams support only valid/qin_clk sampling. The strobe-clocked
    // mode is dormant in RTL behind WEIRD_CLOCK, so reject attempts to select it.
    void mode(const uint32_t m) {
    if (m != readback_mode_valid_clk)
      throw std::runtime_error("readback strobe mode requires a WEIRD_CLOCK RTL build");
    if (v.veryverbose)
      F << "Readback: setting mode=" << readback_mode_valid_clk << " {valid/clk}" << std::endl;
    lmode.write(readback_mode_valid_clk);
  }

  port_t get_crc32() {
    return lcrc32.read();
  }

  void status_report() {
    const auto s = lstatus.read();
    const auto c = lcounter.read();
    const auto crc32 = lcrc32.read();
    std::cout << "Readback status: ";
    if (s & 1) std::cout << yellow << "{empty} " << rst;;
    if (s & 2) std::cout << red << "{reset} " << rst;
    std::cout << (s & 4 ? "{mode=valid/clk} " : "{mode=valid/clk,strobe-disabled} ");
    if (s & 8) std::cout << red << "{overflow} " << rst;
    std::cout << " counter=" << std::dec << c;
    std::cout << " CRC=0x" << std::hex << std::setw(8) << std::setfill('0') << crc32 << std::endl;
  }

    // Returns true if the encoder attempted to write into a full FIFO. This implies data
    // loss and should be treated as a verification failure.
  bool overflow() {
    return lstatus.read() & 8;
  }

  bool last_operation_timed_out() const noexcept {
    return last_operation_timed_out_;
  }

  el read() {
    [[maybe_unused]] control_t control = f.read();
    count_t count = f.read();
    value_t value = f.read();
    return el{count, value}; // regular element
  }

  Sequence capture_sequence(const ReadbackTimeoutPolicy &timeout_policy = {}) {
    if (v.veryverbose)
      status_report();
    last_operation_timed_out_ = false;
    Sequence captured;
    const auto initial_time = std::chrono::steady_clock::now();
    std::optional<time_point> last_read;
    try {
      while (1) {
        auto fill = filled();
        while (fill > 0) {
          el e = read();
          captured.push_back(e);
          last_read = std::chrono::steady_clock::now();
          fill = filled();
          check_timeout(timeout_policy, initial_time, last_read);
        }
        wait_for_more_data(timeout_policy, initial_time, last_read);
      }
    }
    catch (const ReadbackException &e) {
      if (v.veryverbose)
        std::cout << "Caught ReadbackException: " << e.what() << "\n";
    }
    if (v.veryverbose)
      status_report();
    return captured;
  }

  Sequence capture_sequence(const double timeout) {
    return capture_sequence(legacy_readback_timeout_policy(timeout));
  }

    // Read back indefinitely and print each captured run if verbosity is enabled.
    // If timeout>0: timeout in seconds after the last data were read.
    // If timeout<0: timeout in seconds (abs value) after the initial time.
  void read_all(const ReadbackTimeoutPolicy &timeout_policy = {}) {
    if (v.veryverbose)
      status_report();
    last_operation_timed_out_ = false;
    const auto initial_time = std::chrono::steady_clock::now();
    std::optional<time_point> last_read;
    size_t n = 0;   // Element counter
    size_t len = 0; // Total length counter
    try {
      while (1) {
        auto fill = filled();
        while (fill > 0) {
          el e = read();
          last_read = std::chrono::steady_clock::now();
          if (v.verbose) // Keep as is: verbose, not verbosecheck!
            F << prefix << n << " " << e << std::endl;
          n++;
          len += e.count();
          fill = filled();
          check_timeout(timeout_policy, initial_time, last_read);
        }
        wait_for_more_data(timeout_policy, initial_time, last_read);
      }
    }
    catch (const ReadbackException &e) {
      std::cout << "Caught ReadbackException: " << e.what() << "\n";
    }
    F << "Readback report: size=" << std::dec << n << " length=" << std::dec << len << std::endl;
    if (v.veryverbose)
      status_report();
  }

  void read_all(const double timeout) {
    read_all(legacy_readback_timeout_policy(timeout));
  }

    // Compare a captured readback stream against a reference sequence.
    // Returns true if no errors are detected.
  bool check(Sequence elements,
             const ReadbackTimeoutPolicy &timeout_policy = {}) {
    // size, data_size, length need to be computed now, because elements are consumed in the checking process
    const size_t size = elements.size();
    const size_t data_size = elements.data_size();
    const size_t length = elements.length();
    if (v.veryverbose)
      status_report();
    last_operation_timed_out_ = false;
    if (v.verbosecheck)
      F << prefix << "Starting a readback check, size=" << std::dec << size << " length=" << std::dec << length << std::endl;
    size_t n = 0;       // Element counter
    size_t n_error = 0; // Number of errors
    size_t len = 0;     // Total length counter
    const auto initial_time = std::chrono::steady_clock::now();
    std::optional<time_point> last_read;
    try {
      while (!elements.empty()) {
        auto fill = filled();
        while (fill > 0) {
          el e = read();
          last_read = std::chrono::steady_clock::now();
          if (v.verbosecheck)
            F << prefix << n << " " << e << std::endl;
          if (elements.empty())
            throw ReadbackException("Reference sequence exhaused, terminating the check.");
          el e_ref = elements.front();
          elements.pop_front();
          while (!e_ref.is_regular() || (e_ref.is_regular() && e_ref.count() == 0)) { // skip elements which encode no pulses
            if (elements.empty())
              throw ReadbackException("Reference sequence exhaused, terminating the check.");
            e_ref = elements.front();
            elements.pop_front();
          }
          if (e != e_ref) {
            F << prefix << "ERROR: " << n << " " << e << "   <--->   " << e_ref << std::endl;
            n_error++;
          }
          n++;
          len += e.count();
          fill = filled();
          check_timeout(timeout_policy, initial_time, last_read);
        }
        if (elements.empty())
          throw ReadbackException("Reference sequence exhaused, terminating the check.");
        el e_next = elements.front();
        if (e_next.is_final())
          break;
        wait_for_more_data(timeout_policy, initial_time, last_read);
      }
    }
    catch (const ReadbackException &e) {
      std::cout << "Caught ReadbackException: " << e.what() << "\n";
      return false;
    }
    F << prefix << "Checker report: " << n_error << " errors for " << n << " elements checked, error ratio=" << double(n_error)/n
          << ", length=" << len << std::endl;
    if (v.veryverbose)
      status_report();
    int64_t diff_size = int64_t(n) - int64_t(data_size);
    int64_t diff_length = int64_t(len) - int64_t(length);
    F << prefix << "Size difference=" << diff_size << " Length difference=" << diff_length << std::endl;
    F << prefix << (n_error ? red : green) << (n_error ? "#### FAILURE ####" : "SUCCESS") << rst << std::endl;
    return n_error == 0;
  }

  bool check(Sequence elements, const double timeout) {
    return check(elements, legacy_readback_timeout_policy(timeout));
  }
};
