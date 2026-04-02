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
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

#include "streamer.hh"
#include "colors.hh"
#include "fpga.hh"
#include "options.hh"

class ReadbackException : std::exception {
  std::string msg;
public:
  ReadbackException(const std::string &_msg) : msg(_msg) {}
  const char* what() const noexcept override {
    return msg.c_str();
  }
};

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

public:
  readback(FPGA &_fpga,
            const mm &dev,
            const std::uintptr_t base,
            const std::uintptr_t csr_base,
            const std::uintptr_t control_base) :
    fpga(_fpga),
    f(dev, base, csr_base),
    lcontrol(dev.get_loc(control_base)),    // w
    lmode(dev.get_loc(control_base, 4)),    // w
    lstatus(dev.get_loc(control_base)),     // r
    lcounter(dev.get_loc(control_base, 4)), // r
    lcrc32(dev.get_loc(control_base, 8)),   // r
    v(fpga.v)
    {
      reset();
    }

  readback(const ReadbackOptions &opts,
            FPGA &_fpga) :
    readback(_fpga, _fpga.dev_h2f, FIFO_RL_OUT_BASE, FIFO_RL_IN_CSR_BASE, RL_ENCODER_IF_BASE) {
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
    while (filled())
      f.read(); // ignore return value
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
    fpga.wait_for_N_streamer_clk_periods(5);
    BITMASK_CLEAR(control, mask);
    lcontrol.write(control);
    clear_fifo();
  }

    // Select the readback sampling mode. The mode meanings come from the RTL wrapper.
    void mode(const uint32_t m) {
    if (v.veryverbose)
      F << "Readback: setting mode=" << m << " " << (m == 1 ? "{valid/clk}" : "{strobe}") << std::endl;
    lmode.write(m);
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
    std::cout << (s & 4 ? "{mode=strobe} " : "{mode=clk/valid} ");
    if (s & 8) std::cout << red << "{overflow} " << rst;
    std::cout << " counter=" << std::dec << c;
    std::cout << " CRC=0x" << std::hex << std::setw(8) << std::setfill('0') << crc32 << std::endl;
  }

    // Returns true if the encoder attempted to write into a full FIFO. This implies data
    // loss and should be treated as a verification failure.
    bool overflow() {
    return lstatus.read() & 8;
  }

  el read() {
    [[maybe_unused]] control_t control = f.read();
    count_t count = f.read();
    value_t value = f.read();
    return el{count, value}; // regular element
  }

    // Read back indefinitely and print each captured run if verbosity is enabled.
    // If timeout>0: timeout in seconds after the last data were read.
    // If timeout<0: timeout in seconds (abs value) after the initial time.
  void read_all(const double timeout = 0.0) {
    if (v.veryverbose)
      status_report();
    std::chrono::steady_clock::time_point initial_time = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_read;
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
          if (timeout < 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time);
            if (elapsed.count() > abs(timeout))
              throw ReadbackException("Timeout.");
          }
        }
        if (timeout != 0.0 && (timeout > 0 ? n : true)) { // if timeout enabled and if we have started reading...
          auto now = std::chrono::steady_clock::now();
          auto elapsed = (timeout > 0 ? std::chrono::duration_cast<std::chrono::duration<double>>(now - last_read) :
                                        std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time));
          if (elapsed.count() > abs(timeout))
            throw ReadbackException("Timeout.");
        }
      }
    }
    catch (const ReadbackException &e) {
      std::cout << "Caught ReadbackException: " << e.what() << "\n";
    }
    F << "Readback report: size=" << std::dec << n << " length=" << std::dec << len << std::endl;
    if (v.veryverbose)
      status_report();
  }

    // Compare a captured readback stream against a reference sequence.
    // Returns true if no errors are detected.
  bool check(Sequence elements,
              const double timeout = 0.0) {
    // size, data_size, length need to be computed now, because elements are consumed in the checking process
    const size_t size = elements.size();
    const size_t data_size = elements.data_size();
    const size_t length = elements.length();
    if (v.veryverbose)
      status_report();
    if (v.verbosecheck)
      F << prefix << "Starting a readback check, size=" << std::dec << size << " length=" << std::dec << length << std::endl;
    size_t n = 0;       // Element counter
    size_t n_error = 0; // Number of errors
    size_t len = 0;     // Total length counter
    std::chrono::steady_clock::time_point initial_time = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_read;
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
          if (timeout < 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time);
            if (elapsed.count() > abs(timeout))
              throw ReadbackException("Timeout.");
          }
        }
        if (elements.empty())
          throw ReadbackException("Reference sequence exhaused, terminating the check.");
        el e_next = elements.front();
        if (e_next.is_final())
          break;
        if (timeout != 0.0 && (timeout > 0 ? n : true)) { // if timeout enabled and if we have started reading...
          auto now = std::chrono::steady_clock::now();
          auto elapsed = (timeout > 0 ? std::chrono::duration_cast<std::chrono::duration<double>>(now - last_read) :
                                        std::chrono::duration_cast<std::chrono::duration<double>>(now - initial_time));
          if (elapsed.count() > abs(timeout))
            throw ReadbackException("Timeout.");
        }
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
};
